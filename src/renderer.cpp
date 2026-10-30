#include "renderer.hpp"
#include <d3dcompiler.h>
#include <cstring>

namespace {
constexpr char shader[]=R"(
cbuffer Viewport : register(b0) { float4 crop; };
struct V { float4 position:SV_POSITION; float2 uv:TEXCOORD0; };
V VS(uint id:SV_VertexID) {
    V o; float2 uv=float2((id<<1)&2,id&2);
    o.position=float4(uv.x*2-1,1-uv.y*2,0,1);
    o.uv=crop.xy+uv*crop.zw; return o;
}
Texture2D screenImage:register(t0); SamplerState sampling:register(s0);
float4 PS(V input):SV_TARGET {
    return screenImage.Sample(sampling,input.uv);
})";
}
bool Renderer::open(HWND host,RECT monitor,bool diagnostic){
    close();bounds=monitor;
    // Windows 10 2004+ excludes this window from capture. No hide /
    // recapture / show loop is ever used, and no pointer shape is composited.
    if(!SetWindowDisplayAffinity(host,WDA_EXCLUDEFROMCAPTURE))return false;
    Ptr<IDXGIFactory1> factory;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return false;
    Ptr<IDXGIAdapter1> adapter;Ptr<IDXGIOutput> output;
    HMONITOR wanted=MonitorFromRect(&monitor,MONITOR_DEFAULTTONEAREST);
    bool found=false;
    for(UINT a=0;!found;++a){
        Ptr<IDXGIAdapter1> candidate;
        if(FAILED(factory->EnumAdapters1(a,&candidate)))break;
        for(UINT o=0;;++o){
            Ptr<IDXGIOutput> candidateOutput;
            if(FAILED(candidate->EnumOutputs(o,&candidateOutput)))break;
            DXGI_OUTPUT_DESC desc{};candidateOutput->GetDesc(&desc);
            if(desc.Monitor==wanted){adapter=candidate;output=candidateOutput;found=true;break;}
        }
    }
    if(!found)return false;
    if(FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return false;
    Ptr<IDXGIFactory2> factory2;
    if(FAILED(adapter->GetParent(IID_PPV_ARGS(&factory2))))return false;
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width=monitor.right-monitor.left;sd.Height=monitor.bottom-monitor.top;
    sd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;sd.SampleDesc.Count=1;
    sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;
    sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;sd.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
    if(FAILED(factory2->CreateSwapChainForHwnd(device.Get(),host,&sd,nullptr,nullptr,&swap)))return false;
    factory2->MakeWindowAssociation(host,DXGI_MWA_NO_ALT_ENTER);
    if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&backBuffer)))||FAILED(device->CreateRenderTargetView(backBuffer.Get(),nullptr,&target)))return false;
    Ptr<ID3DBlob> vs,ps;
    if(FAILED(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"VS","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,nullptr))||
       FAILED(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"PS","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,nullptr)))return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex))||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel)))return false;
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&bd,nullptr,&constants)))return false;
    D3D11_SAMPLER_DESC ss{};ss.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;ss.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(device->CreateSamplerState(&ss,&sampler)))return false;
    if(diagnostic){
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=1;
        td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device->CreateTexture2D(&td,nullptr,&probeTexture)))return false;
    }
    if(!captureSource.open(device.Get(),wanted,sd.Width,sd.Height))return false;
    // Do not show an empty overlay if the first desktop frame is unavailable.
    const auto deadline=GetTickCount64()+1000;
    while(!hasFrame&&GetTickCount64()<deadline){if(capture()<0)return false;if(!hasFrame)Sleep(2);}
    return hasFrame;
}
int Renderer::capture(){
    Ptr<ID3D11Texture2D> frame;
    const int result=captureSource.next(frame);
    if(result<=0)return result;
    HRESULT hr=S_OK;
    if(SUCCEEDED(hr)&&!desktop){
        D3D11_TEXTURE2D_DESC td{};frame->GetDesc(&td);
        td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=0;td.MiscFlags=0;
        hr=device->CreateTexture2D(&td,nullptr,&desktop);
        if(SUCCEEDED(hr))hr=device->CreateShaderResourceView(desktop.Get(),nullptr,&image);
    }
    if(SUCCEEDED(hr)){context->CopyResource(desktop.Get(),frame.Get());hasFrame=true;}
    captureSource.releaseFrame();return SUCCEEDED(hr)?1:-1;
}
bool Renderer::update(const ez::View& v){
    int fresh=capture();if(fresh<0)return false;
    if(!fresh&&hasView&&v.scale==previous.scale&&v.x==previous.x&&v.y==previous.y)return true;
    float values[4]={float(v.x/v.width),float(v.y/v.height),float(1/v.scale),float(1/v.scale)};
    context->UpdateSubresource(constants.Get(),0,nullptr,values,0,0);
    ID3D11RenderTargetView* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT viewport{0,0,float(v.width),float(v.height),0,1};context->RSSetViewports(1,&viewport);
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);
    ID3D11Buffer* cb=constants.Get();context->VSSetConstantBuffers(0,1,&cb);context->PSSetConstantBuffers(0,1,&cb);
    ID3D11ShaderResourceView* texture=image.Get();context->PSSetShaderResources(0,1,&texture);
    ID3D11SamplerState* sample=sampler.Get();context->PSSetSamplers(0,1,&sample);
    context->Draw(3,0);
    // Test the frame just drawn, BEFORE Present rotates the back buffers.
    if(probeTexture){D3D11_BOX box{32,32,0,33,33,1};context->CopySubresourceRegion(probeTexture.Get(),0,0,0,0,backBuffer.Get(),0,&box);}
    HRESULT hr=swap->Present(1,0);
    if(FAILED(hr))return false;
    previous=v;hasView=hr==S_OK;return true;
}
COLORREF Renderer::probe(){
    if(!hasView||!probeTexture)return CLR_INVALID;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(probeTexture.Get(),0,D3D11_MAP_READ,0,&mapped)))return CLR_INVALID;
    auto* p=static_cast<const BYTE*>(mapped.pData);COLORREF result=RGB(p[2],p[1],p[0]);context->Unmap(probeTexture.Get(),0);return result;
}
void Renderer::close(){
    captureSource.close();
    if(context){context->ClearState();context->Flush();}
    image.Reset();desktop.Reset();target.Reset();backBuffer.Reset();probeTexture.Reset();
    constants.Reset();sampler.Reset();vertex.Reset();pixel.Reset();swap.Reset();context.Reset();device.Reset();
    hasView=hasFrame=false;
}

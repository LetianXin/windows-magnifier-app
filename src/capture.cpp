#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture.hpp"
// MinGW's generated Foundation header aliases BYTE and boolean to one type.
#ifdef __MINGW32__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#endif
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <roapi.h>
#include <winstring.h>

using Microsoft::WRL::ComPtr;
namespace GC=ABI::Windows::Graphics::Capture;
namespace D3D=ABI::Windows::Graphics::DirectX::Direct3D11;
namespace DX=ABI::Windows::Graphics::DirectX;
namespace {
template<class T> HRESULT factory(const wchar_t* name,ComPtr<T>& result){
    HSTRING text=nullptr;HRESULT hr=WindowsCreateString(name,UINT32(wcslen(name)),&text);
    if(SUCCEEDED(hr))hr=RoGetActivationFactory(text,IID_PPV_ARGS(&result));
    WindowsDeleteString(text);return hr;
}
void closeObject(IUnknown* object){
    if(!object)return;
    ComPtr<ABI::Windows::Foundation::IClosable> closable;
    if(SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&closable))))closable->Close();
}
}
struct Capture::State {
    ComPtr<GC::IDirect3D11CaptureFramePool> pool;
    ComPtr<GC::IGraphicsCaptureSession> session;
    ComPtr<GC::IGraphicsCaptureSession2> cursorSettings;
    ComPtr<GC::IDirect3D11CaptureFrame> frame;
    ComPtr<GC::IGraphicsCaptureItem> item;
    ComPtr<D3D::IDirect3DDevice> device;
    bool initialized=false,cursorOff=false;
    UINT width=0,height=0;
    ~State(){
        closeObject(frame.Get());frame.Reset();
        closeObject(session.Get());cursorSettings.Reset();session.Reset();
        closeObject(pool.Get());pool.Reset();item.Reset();device.Reset();
        if(initialized)RoUninitialize();
    }
};
Capture::Capture()=default;
Capture::~Capture()=default;
bool Capture::open(ID3D11Device* device,HMONITOR monitor,UINT width,UINT height){
    close();state=std::make_unique<State>();
    HRESULT hr=RoInitialize(RO_INIT_MULTITHREADED);
    if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE)return false;
    state->initialized=SUCCEEDED(hr);state->width=width;state->height=height;
    ComPtr<IDXGIDevice> dxgi;ComPtr<IInspectable> wrapper;
    if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||
       FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(),&wrapper))||
       FAILED(wrapper.As(&state->device)))return false;
    ComPtr<IGraphicsCaptureItemInterop> interop;
    if(FAILED(factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",interop))||
       FAILED(interop->CreateForMonitor(monitor,IID_PPV_ARGS(&state->item))))return false;
    ComPtr<GC::IDirect3D11CaptureFramePoolStatics2> poolFactory;
    if(FAILED(factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",poolFactory)))return false;
    ABI::Windows::Graphics::SizeInt32 size{INT32(width),INT32(height)};
    if(FAILED(poolFactory->CreateFreeThreaded(state->device.Get(),DX::DirectXPixelFormat_B8G8R8A8UIntNormalized,2,size,&state->pool))||
       FAILED(state->pool->CreateCaptureSession(state->item.Get(),&state->session))||
       FAILED(state->session.As(&state->cursorSettings)))return false;
    // Required, not a best-effort setting. Never fall back to cursor-inclusive frames.
    if(FAILED(state->cursorSettings->put_IsCursorCaptureEnabled(false)))return false;
    boolean included=true;
    if(FAILED(state->cursorSettings->get_IsCursorCaptureEnabled(&included))||included)return false;
    state->cursorOff=true;
    return SUCCEEDED(state->session->StartCapture());
}
int Capture::next(ComPtr<ID3D11Texture2D>& texture){
    if(!state||!state->pool)return -1;
    releaseFrame();
    HRESULT hr=state->pool->TryGetNextFrame(&state->frame);
    if(FAILED(hr))return -1;
    if(!state->frame)return 0;
    ABI::Windows::Graphics::SizeInt32 size{};
    if(FAILED(state->frame->get_ContentSize(&size))||size.Width!=INT32(state->width)||size.Height!=INT32(state->height))return -1;
    ComPtr<D3D::IDirect3DSurface> surface;
    ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> access;
    if(FAILED(state->frame->get_Surface(&surface))||FAILED(surface.As(&access))||
       FAILED(access->GetInterface(IID_PPV_ARGS(&texture))))return -1;
    return 1;
}
void Capture::releaseFrame(){if(state){closeObject(state->frame.Get());state->frame.Reset();}}
bool Capture::cursorExcluded()const{return state&&state->cursorOff;}
void Capture::close(){state.reset();}

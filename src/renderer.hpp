#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <wrl/client.h>
#include "view.hpp"
#include "capture.hpp"

// Capture and presentation stay on the GPU. A complete frame remains visible
// until its replacement is ready; there are no layered-window repaint cycles.
class Renderer {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;
    Ptr<ID3D11DeviceContext> context;
    Capture captureSource;
    Ptr<IDXGISwapChain1> swap;
    Ptr<IDCompositionDevice> composition;
    Ptr<IDCompositionTarget> compositionTarget;
    Ptr<IDCompositionVisual> compositionVisual;
    HANDLE frameReady=nullptr;
    Ptr<ID3D11Texture2D> desktop,backBuffer,probeTexture;
    Ptr<ID3D11ShaderResourceView> image;
    Ptr<ID3D11RenderTargetView> target;
    Ptr<ID3D11VertexShader> vertex;
    Ptr<ID3D11PixelShader> pixel;
    Ptr<ID3D11SamplerState> sampler;
    Ptr<ID3D11Buffer> constants;
    RECT bounds{};
    ez::View previous{};
    bool hasView=false,hasFrame=false;
    bool pending=false;
    int capture();
public:
    bool open(HWND host,RECT monitor,bool diagnostic=false,bool begin=true);
    bool beginCapture();
    void endCapture();
    bool ready()const{return hasView;}
    bool update(const ez::View& view);
    COLORREF probe(); // Explicit diagnostic only; never used in ordinary operation.
    bool cursorExcluded()const{return captureSource.cursorExcluded();}
    void close();
    ~Renderer(){close();}
};

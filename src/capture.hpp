#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

// Windows Graphics Capture with cursor inclusion explicitly disabled.
// The WinRT details are kept out of the window/input and renderer code.
class Capture {
    struct State;
    std::unique_ptr<State> state;
public:
    Capture();
    ~Capture();
    bool open(ID3D11Device* device,HMONITOR monitor,UINT width,UINT height);
    int next(Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture);
    void releaseFrame();
    bool cursorExcluded()const;
    void close();
};

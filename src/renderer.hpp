#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <magnification.h>
#include "view.hpp"

// Windows owns live capture and composition; the app supplies only a viewport.
// No screenshot buffer, Direct3D device, or extra cursor is maintained here.
class Renderer {
    HMODULE library=nullptr;
    HWND magnifier=nullptr;
    RECT bounds{};
    bool initialized=false;
    double scale=0;
    BOOL (WINAPI *initialize)()=nullptr;
    BOOL (WINAPI *uninitialize)()=nullptr;
    BOOL (WINAPI *transform)(HWND,PMAGTRANSFORM)=nullptr;
    BOOL (WINAPI *source)(HWND,RECT)=nullptr;
    BOOL (WINAPI *filter)(HWND,DWORD,int,HWND*)=nullptr;
public:
    bool open(HWND host,RECT monitor);
    bool update(const ez::View& view);
    void close();
    ~Renderer(){close();}
};

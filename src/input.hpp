#pragma once
#include "view.hpp"
#include <windows.h>
#include <mutex>
#include <thread>
#include <atomic>

// A dedicated hook thread never waits for capture, shaders or Present.
// Only movement is transformed. Windows delivers the original buttons,
// wheel, drag and keyboard events to the real application below the overlay.
class InputRouter {
    inline static InputRouter* owner=nullptr;
    static constexpr ULONG_PTR marker=0x455A4D50;
    std::thread worker;
    DWORD threadId=0;
    HANDLE ready=nullptr;
    HHOOK mouse=nullptr,keyboard=nullptr;
    HWND recipient=nullptr;
    UINT zoomMessage=0,stopMessage=0;
    std::mutex guard;
    ez::View view;
    double px=0,py=0;
    RECT bounds{};
    bool active=false;
    std::atomic<bool> enabled{true};
    std::atomic<int> modifier{VK_MENU};
    bool moveSystemPointer();
    static LRESULT CALLBACK mouseProc(int,WPARAM,LPARAM);
    static LRESULT CALLBACK keyProc(int,WPARAM,LPARAM);
public:
    struct Snapshot {ez::View view;POINT pointer;};
    bool open(HWND target,UINT zoom,UINT stop);
    void configure(bool on,int key){enabled=on;modifier=key;}
    void begin(ez::View v,RECT monitor,POINT screenPointer);
    Snapshot snapshot();
    void zoom(int delta);
    void setView(ez::View v);
    void end();
    void close();
    ~InputRouter(){close();}
};

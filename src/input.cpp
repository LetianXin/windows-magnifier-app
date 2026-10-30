#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "input.hpp"
#include <windowsx.h>

bool InputRouter::open(HWND target,UINT zoom,UINT stop){
    recipient=target;zoomMessage=zoom;stopMessage=stop;owner=this;
    ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ready)return false;
    worker=std::thread([this]{
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        threadId=GetCurrentThreadId();MSG msg{};PeekMessageW(&msg,nullptr,0,0,PM_NOREMOVE);
        mouse=SetWindowsHookExW(WH_MOUSE_LL,mouseProc,GetModuleHandleW(nullptr),0);
        keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,keyProc,GetModuleHandleW(nullptr),0);
        SetEvent(ready);
        while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        if(mouse)UnhookWindowsHookEx(mouse);if(keyboard)UnhookWindowsHookEx(keyboard);
    });
    WaitForSingleObject(ready,INFINITE);CloseHandle(ready);ready=nullptr;
    return mouse&&keyboard;
}
bool InputRouter::moveSystemPointer(){
    const int x=bounds.left+int(std::lround(view.x+px/view.scale));
    const int y=bounds.top+int(std::lround(view.y+py/view.scale));
    const int left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT input{};input.type=INPUT_MOUSE;
    // Pixel centres, rather than the 65535/(extent-1) approximation.
    input.mi.dx=LONG((double(x-left)+0.5)*65536/width);
    input.mi.dy=LONG((double(y-top)+0.5)*65536/height);
    input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    input.mi.dwExtraInfo=marker;
    return SendInput(1,&input,sizeof(input))==1;
}
void InputRouter::begin(ez::View v,RECT monitor,POINT screenPointer){
    std::lock_guard<std::mutex> lock(guard);
    bounds=monitor;view=v;px=screenPointer.x-bounds.left;py=screenPointer.y-bounds.top;active=true;
}
InputRouter::Snapshot InputRouter::snapshot(){
    std::lock_guard<std::mutex> lock(guard);return {view,{LONG(std::lround(px)),LONG(std::lround(py))}};
}
void InputRouter::zoom(int delta){
    std::lock_guard<std::mutex> lock(guard);
    view.zoom(view.scale*std::pow(1.2,double(delta)/WHEEL_DELTA),px,py);
    if(!moveSystemPointer())PostMessageW(recipient,stopMessage,0,0);
}
void InputRouter::setView(ez::View v){std::lock_guard<std::mutex> lock(guard);view=v;moveSystemPointer();}
void InputRouter::end(){std::lock_guard<std::mutex> lock(guard);active=false;}
LRESULT CALLBACK InputRouter::mouseProc(int code,WPARAM message,LPARAM data){
    auto* self=owner;
    if(code==HC_ACTION&&self){
        const auto& event=*reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        if(event.dwExtraInfo==marker)return CallNextHookEx(nullptr,code,message,data);
        const bool held=(GetAsyncKeyState(self->modifier.load())&0x8000)!=0;
        std::lock_guard<std::mutex> lock(self->guard);
        if(message==WM_MOUSEWHEEL&&self->enabled&&held){
            int delta=GET_WHEEL_DELTA_WPARAM(event.mouseData);
            if(self->active||delta>0){PostMessageW(self->recipient,self->zoomMessage,WPARAM(delta),0);return 1;}
        }
        if(message==WM_MOUSEMOVE&&self->active){
            // Hooks run before the new pointer position is committed. This
            // preserves Windows pointer acceleration instead of using raw counts.
            POINT previous{};GetCursorPos(&previous);
            double dx=event.pt.x-previous.x,dy=event.pt.y-previous.y;
            double oldX=self->px,oldY=self->py;
            self->px=std::clamp(oldX+dx,0.0,self->view.width-1);
            self->py=std::clamp(oldY+dy,0.0,self->view.height-1);
            self->view.pan(ez::edgePush(oldX,self->px,self->view.width,dx),
                           ez::edgePush(oldY,self->py,self->view.height,dy));
            if(!self->moveSystemPointer()){
                self->active=false;PostMessageW(self->recipient,self->stopMessage,0,0);
                return CallNextHookEx(nullptr,code,message,data);
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
LRESULT CALLBACK InputRouter::keyProc(int code,WPARAM message,LPARAM data){
    if(code==HC_ACTION&&owner){
        const auto& key=*reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        std::lock_guard<std::mutex> lock(owner->guard);
        if(owner->active&&key.vkCode==VK_ESCAPE){
            if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN)PostMessageW(owner->recipient,owner->stopMessage,0,0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
void InputRouter::close(){
    end();if(worker.joinable()){PostThreadMessageW(threadId,WM_QUIT,0,0);worker.join();}
    if(owner==this)owner=nullptr;mouse=keyboard=nullptr;
}

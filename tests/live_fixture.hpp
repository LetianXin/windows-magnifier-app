#pragma once
#include <windows.h>
#include <string>

// Only used by --demo/--smoke-test. Real desktop windows are never modified.
// The left quarter is gray; a 2x centered view samples the colored area instead.
// Checking that displayed color twice proves both magnification and liveness.
class LiveFixture {
    HWND window=nullptr;
    RECT bounds{};
    int phase=0;
    POINT pressed{};
    static COLORREF color(int phase){return phase%2?RGB(176,64,48):RGB(32,144,112);}
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
        auto* self=reinterpret_cast<LiveFixture*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<LiveFixture*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)self);}
        if(self&&m==WM_TIMER){self->phase++;InvalidateRect(w,nullptr,FALSE);return 0;}
        if(self){
            if(m==WM_LBUTTONDOWN){++self->leftDown;self->pressed={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};SetCapture(w);return 0;}
            if(m==WM_LBUTTONUP){++self->leftUp;self->dragged=abs(GET_X_LPARAM(lp)-self->pressed.x)>4;ReleaseCapture();return 0;}
            if(m==WM_RBUTTONDOWN){++self->rightDown;return 0;}
            if(m==WM_MBUTTONDOWN){++self->middleDown;return 0;}
            if(m==WM_XBUTTONDOWN){++self->sideDown;return TRUE;}
            if(m==WM_MOUSEWHEEL){self->wheel+=GET_WHEEL_DELTA_WPARAM(wp);return 0;}
            if(m==WM_MOUSEHWHEEL){self->horizontal+=GET_WHEEL_DELTA_WPARAM(wp);return 0;}
            if(m==WM_CHAR){if(wp=='x')++self->typed;return 0;}
        }
        if(self&&m==WM_PAINT){
            PAINTSTRUCT p;HDC dc=BeginPaint(w,&p);RECT r;GetClientRect(w,&r);
            HBRUSH brush=CreateSolidBrush(color(self->phase));FillRect(dc,&r,brush);DeleteObject(brush);
            RECT left=r;left.right=r.right/4;FillRect(dc,&left,(HBRUSH)GetStockObject(GRAY_BRUSH));
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
            std::wstring title=L"LIVE FRAME "+std::to_wstring(self->phase)+L" - the background changes while zoom stays still";
            TextOutW(dc,r.right/4+60,r.bottom/4+70,title.c_str(),int(title.size()));
            EndPaint(w,&p);return 0;
        }
        return DefWindowProcW(w,m,wp,lp);
    }
public:
    unsigned leftDown=0,leftUp=0,rightDown=0,middleDown=0,sideDown=0,typed=0;
    int wheel=0,horizontal=0;bool dragged=false;
    COLORREF lastProbe=CLR_INVALID;
    bool open(RECT monitor,bool animate){
        close();bounds=monitor;phase=0;
        WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=proc;c.lpszClassName=L"EdgeZoomLiveFixture";
        RegisterClassW(&c);
        window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_APPWINDOW,c.lpszClassName,L"EdgeZoom live test source",WS_POPUP,
            monitor.left,monitor.top,monitor.right-monitor.left,monitor.bottom-monitor.top,nullptr,nullptr,c.hInstance,this);
        if(!window)return false;
        ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(window);UpdateWindow(window);
        if(animate)SetTimer(window,1,500,nullptr);
        return true;
    }
    void advance(){++phase;InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);}
    HWND handle()const{return window;}
    bool ownsForeground()const{return GetForegroundWindow()==window;}
    bool displayed(COLORREF actual){
        lastProbe=actual;
        COLORREF expected=color(phase);
        return actual!=CLR_INVALID&&abs(int(GetRValue(actual))-int(GetRValue(expected)))<12
            &&abs(int(GetGValue(actual))-int(GetGValue(expected)))<12
            &&abs(int(GetBValue(actual))-int(GetBValue(expected)))<12;
    }
    void close(){if(window){DestroyWindow(window);window=nullptr;}}
};

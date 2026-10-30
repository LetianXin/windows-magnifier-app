#include "renderer.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <string>
#include <iterator>
#include <cstdio>
#include "../tests/live_fixture.hpp"

namespace {
constexpr UINT TRAY=WM_APP+1,ZOOM=WM_APP+2;
constexpr int KEYBOX=101,DONE=102,QUIT=103,PAUSE=104;
struct Binding {const wchar_t* label;int vk;};
constexpr Binding bindings[]={
    {L"Alt",VK_MENU},{L"Ctrl",VK_CONTROL},{L"Shift",VK_SHIFT},
    {L"Mouse side button 1",VK_XBUTTON1},{L"Mouse side button 2",VK_XBUTTON2},
    {L"Space",VK_SPACE},{L"Caps Lock",VK_CAPITAL},
    {L"F1",VK_F1},{L"F2",VK_F2},{L"F3",VK_F3},{L"F4",VK_F4},
    {L"F5",VK_F5},{L"F6",VK_F6},{L"F7",VK_F7},{L"F8",VK_F8},
    {L"F9",VK_F9},{L"F10",VK_F10},{L"F11",VK_F11},{L"F12",VK_F12}};
HINSTANCE instance;HWND settings=nullptr,overlay=nullptr,previousWindow=nullptr;
HFONT font=nullptr;HHOOK mouseHook=nullptr;
Renderer renderer;ez::View view;
LiveFixture fixture;
RECT monitorBounds{},oldClip{};POINT lastPointer{};
bool active=false,practice=false,clipped=false,trayAdded=false;
bool enabled=true;
int binding=0;UINT taskbarCreated=0;std::wstring config;
bool smoke=false,smokePass=false,renderHealthy=false;
unsigned smokeChecks=0;COLORREF firstProbe=CLR_INVALID,secondProbe=CLR_INVALID;
ULONGLONG smokeStarted=0;unsigned frameSamples=0,badFrames=0;

bool held(){return (GetAsyncKeyState(bindings[binding].vk)&0x8000)!=0;}
void stop(){
    if(!active)return;
    active=false;KillTimer(overlay,1);
    RAWINPUTDEVICE raw{1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&raw,1,sizeof(raw));
    if(clipped){ClipCursor(&oldClip);clipped=false;}
    const bool restore=GetForegroundWindow()==overlay;
    ShowWindow(overlay,SW_HIDE);renderer.close();fixture.close();
    if(restore&&IsWindow(previousWindow))SetForegroundWindow(previousWindow);
    practice=false;
}
void redraw(){
    wchar_t title[100];swprintf_s(title,L"EdgeZoom - %.2fx",view.scale);
    SetWindowTextW(overlay,title);
    if(active){renderHealthy=renderer.update(view);if(!renderHealthy)stop();}
}
bool start(bool test=false){
    if(active)return true;
    previousWindow=GetForegroundWindow();
    POINT p;GetCursorPos(&p);
    MONITORINFO mi{sizeof(mi)};
    if(!GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi))return false;
    monitorBounds=mi.rcMonitor;
    if(IsWindowVisible(settings)){ShowWindow(settings,SW_HIDE);DwmFlush();}
    const int w=monitorBounds.right-monitorBounds.left,h=monitorBounds.bottom-monitorBounds.top;
    SetWindowPos(overlay,HWND_TOPMOST,monitorBounds.left,monitorBounds.top,w,h,SWP_NOACTIVATE);
    if(test&&!fixture.open(monitorBounds,!smoke))return false;
    if(!renderer.open(overlay,monitorBounds,test)){
        renderer.close();fixture.close();MessageBoxW(settings,L"Live desktop capture could not start. Windows 10 version 2004 or newer and a compatible display driver are required.",L"EdgeZoom",MB_OK|MB_ICONERROR);return false;
    }
    view={double(w),double(h),1,0,0};lastPointer={p.x-monitorBounds.left,p.y-monitorBounds.top};
    RAWINPUTDEVICE raw{1,2,RIDEV_INPUTSINK,overlay};
    if(!RegisterRawInputDevices(&raw,1,sizeof(raw))){renderer.close();fixture.close();return false;}
    clipped=GetClipCursor(&oldClip)&&ClipCursor(&monitorBounds);
    practice=test;active=true;
    renderHealthy=renderer.update(view);
    if(!renderHealthy){stop();return false;}
    SetWindowPos(overlay,HWND_TOPMOST,monitorBounds.left,monitorBounds.top,w,h,SWP_SHOWWINDOW);
    SetForegroundWindow(overlay);SetFocus(overlay);
    SetTimer(overlay,1,16,nullptr);return active;
}
void zoom(int delta){
    if(!active&&(!held()||delta<=0))return;
    if(!start())return;
    POINT p;GetCursorPos(&p);ScreenToClient(overlay,&p);
    // Immediate pointer-anchored zoom: no easing, timer scrolling, or inertia.
    view.zoom(view.scale*std::pow(1.2,double(delta)/WHEEL_DELTA),p.x,p.y);redraw();
}
LRESULT CALLBACK mouseProc(int code,WPARAM message,LPARAM data){
    if(!smoke&&code==HC_ACTION&&message==WM_MOUSEWHEEL&&(active||(enabled&&held()))){
        const auto* event=reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        const int delta=GET_WHEEL_DELTA_WPARAM(event->mouseData);
        if(active||delta>0){PostMessageW(settings,ZOOM,WPARAM(delta),0);return 1;}
    }
    return CallNextHookEx(mouseHook,code,message,data);
}
void rawMouse(HRAWINPUT handle){
    if(!active||smoke)return;
    RAWINPUT raw{};UINT bytes=sizeof(raw);
    if(GetRawInputData(handle,RID_INPUT,&raw,&bytes,sizeof(RAWINPUTHEADER))!=sizeof(raw))return;
    if(raw.header.dwType!=RIM_TYPEMOUSE)return;
    POINT p;GetCursorPos(&p);ScreenToClient(overlay,&p);
    if(!(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)){
        const double dx=ez::edgePush(lastPointer.x,p.x,view.width,raw.data.mouse.lLastX);
        const double dy=ez::edgePush(lastPointer.y,p.y,view.height,raw.data.mouse.lLastY);
        if(dx||dy){const double x=view.x,y=view.y;view.pan(dx,dy);if(x!=view.x||y!=view.y)redraw();}
    }
    lastPointer=p;
}
LRESULT CALLBACK OverlayProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    case WM_INPUT:rawMouse(reinterpret_cast<HRAWINPUT>(lp));return DefWindowProcW(w,m,wp,lp);
    case WM_TIMER:
        if(active&&!practice&&!held())stop();
        else if(active){renderHealthy=renderer.update(view);if(!renderHealthy)stop();}
        return 0;
    case WM_KEYDOWN:case WM_SYSKEYDOWN:
        if(wp==VK_ESCAPE){stop();return 0;}
        // Normal system switching remains available while inspecting.
        if(wp==VK_TAB){stop();return DefWindowProcW(w,m,wp,lp);}
        return 0;
    case WM_KEYUP:case WM_SYSKEYUP:if(!practice&&!held())stop();return 0;
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE&&active)stop();return 0;
    case WM_DISPLAYCHANGE:case WM_CLOSE:stop();return 0;
    case WM_SETCURSOR:SetCursor(LoadCursorW(nullptr,IDC_ARROW));return TRUE;
    }
    return DefWindowProcW(w,m,wp,lp);
}
void tray(){
    NOTIFYICONDATAW n{sizeof(n)};n.hWnd=settings;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    n.uCallbackMessage=TRAY;n.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));
    if(!n.hIcon)n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    swprintf_s(n.szTip,L"EdgeZoom - %s - hold %s + scroll",enabled?L"ON":L"PAUSED",bindings[binding].label);
    if(trayAdded)trayAdded=Shell_NotifyIconW(NIM_MODIFY,&n)!=FALSE;
    if(!trayAdded)trayAdded=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;
    if(trayAdded){n.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&n);}
}
void pauseZoom(){stop();enabled=!enabled;SetWindowTextW(GetDlgItem(settings,PAUSE),enabled?L"Pause":L"Resume");tray();}
void showSettings(){stop();ShowWindow(settings,SW_SHOWNORMAL);SetForegroundWindow(settings);}
void saveBinding(){
    int selected=int(SendMessageW(GetDlgItem(settings,KEYBOX),CB_GETCURSEL,0,0));
    if(selected<0||selected>=int(std::size(bindings)))return;
    binding=selected;tray();
    const std::wstring value=std::to_wstring(bindings[binding].vk);
    if(!WritePrivateProfileStringW(L"EdgeZoom",L"ActivationKey",value.c_str(),config.c_str()))
        MessageBoxW(settings,L"This button works for this session, but the setting could not be saved beside EdgeZoom.exe.",L"EdgeZoom",MB_OK|MB_ICONINFORMATION);
}
HWND control(HWND parent,const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int width,int height,int id){
    const int dpi=GetDpiForWindow(parent);
    HWND c=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style,MulDiv(x,dpi,96),MulDiv(y,dpi,96),
        MulDiv(width,dpi,96),MulDiv(height,dpi,96),parent,(HMENU)(INT_PTR)id,instance,nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
LRESULT CALLBACK SettingsProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(taskbarCreated&&m==taskbarCreated){trayAdded=false;tray();return 0;}
    switch(m){
    case WM_CREATE:{
        control(w,L"STATIC",L"Hold this button, then scroll to zoom:",0,20,18,350,22,0);
        HWND combo=control(w,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,20,48,350,240,KEYBOX);
        for(auto b:bindings)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)b.label);
        SendMessageW(combo,CB_SETCURSEL,binding,0);
        control(w,L"STATIC",L"Push beyond an edge to pan at normal speed.\nRelease the button to return. Esc also exits.\nThe screen stays live while you hold.\n\nTray icon: Pause / Quit   |   Ctrl+Alt+Q: quit",0,20,92,350,110,0);
        control(w,L"BUTTON",L"Done - keep in tray",WS_TABSTOP|BS_DEFPUSHBUTTON,20,218,176,34,DONE);
        control(w,L"BUTTON",L"Pause",WS_TABSTOP,204,218,78,34,PAUSE);
        control(w,L"BUTTON",L"Quit",WS_TABSTOP,290,218,80,34,QUIT);
        return 0;}
    case WM_COMMAND:
        if(LOWORD(wp)==KEYBOX&&HIWORD(wp)==CBN_SELCHANGE)saveBinding();
        if(LOWORD(wp)==DONE){if(trayAdded)ShowWindow(w,SW_HIDE);}
        if(LOWORD(wp)==PAUSE)pauseZoom();
        if(LOWORD(wp)==QUIT)DestroyWindow(w);
        return 0;
    case ZOOM:zoom(int(wp));return 0;
    case TRAY:
        if(LOWORD(lp)==NIN_SELECT||LOWORD(lp)==NIN_KEYSELECT)showSettings();
        if(LOWORD(lp)==WM_CONTEXTMENU){
            HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,DONE,L"Settings");
            AppendMenuW(menu,MF_STRING,PAUSE,enabled?L"Pause zoom":L"Resume zoom");
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,QUIT,L"Quit EdgeZoom\tCtrl+Alt+Q");
            POINT p;GetCursorPos(&p);SetForegroundWindow(w);
            UINT id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,w,nullptr);DestroyMenu(menu);
            if(id==DONE)showSettings();if(id==PAUSE)pauseZoom();if(id==QUIT)DestroyWindow(w);PostMessageW(w,WM_NULL,0,0);
        }return 0;
    case WM_HOTKEY:if(wp==1)DestroyWindow(w);return 0;
    case WM_TIMER:
        if(wp==9){KillTimer(w,9);if(start(true)){view.zoom(2,view.width/2,view.height/2);redraw();}else PostQuitMessage(2);}
        if(wp==14&&active){
            const auto elapsed=GetTickCount64()-smokeStarted;
            if((elapsed>900&&elapsed<2300)||(elapsed>2800&&elapsed<4200)){
                ++frameSamples;if(!fixture.displayed(renderer.probe()))++badFrames;
                view.pan(3,0);redraw();
            }
        }
        if(wp==11){
            KillTimer(w,11);
            smokeChecks=(active?1:0)|(renderHealthy?2:0)|(std::abs(view.scale-2)<1e-6?4:0)|(fixture.displayed(renderer.probe())?8:0);
            firstProbe=fixture.lastProbe;
            fixture.advance(); // Change ONLY the underlying source, keeping the viewport stationary.
        }
        if(wp==12){
            KillTimer(w,12);smokeChecks|=(active?16:0)|(fixture.displayed(renderer.probe())?32:0);secondProbe=fixture.lastProbe;
            zoom(120);smokeChecks|=std::abs(view.scale-2.4)<1e-6?64:0;
            practice=false; // Exercise the real release timer with no activation key held.
        }
        if(wp==13){KillTimer(w,13);RECT clip{};GetClipCursor(&clip);smokeChecks|=(!active?128:0)|(EqualRect(&clip,&oldClip)?256:0);}
        if(wp==10){
            smokePass=smokeChecks==511&&frameSamples>=10&&badFrames==0&&trayAdded;
            if(smoke){
                FILE* report=_wfopen((config+L".test.txt").c_str(),L"w");
                if(report){fprintf(report,"Live integration test: %s\nChecks: %u / 511\nFirst rendered pixel: %06lx\nSecond rendered pixel: %06lx\nFrame samples while panning: %u\nUnexpected frames: %u\nTray icon registered: %s\n",smokePass?"PASS":"FAIL",smokeChecks,firstProbe,secondProbe,frameSamples,badFrames,trayAdded?"yes":"no");fclose(report);}
            }
            DestroyWindow(w);if(smoke)PostQuitMessage(smokePass?0:3);
        }return 0;
    case WM_QUERYENDSESSION:stop();return TRUE;
    case WM_DISPLAYCHANGE:stop();return 0;
    case WM_CLOSE:if(trayAdded)ShowWindow(w,SW_HIDE);else DestroyWindow(w);return 0;
    case WM_DESTROY:{
        stop();if(mouseHook){UnhookWindowsHookEx(mouseHook);mouseHook=nullptr;}
        UnregisterHotKey(w,1);
        NOTIFYICONDATAW n{sizeof(n)};n.hWnd=w;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);PostQuitMessage(0);return 0;}
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,PWSTR args,int){
    instance=h;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\EdgeZoom_Hold_v2");
    if(mutex&&GetLastError()==ERROR_ALREADY_EXISTS){
        if(wcsstr(args,L"--smoke-test")){CloseHandle(mutex);return 4;}
        HWND other=FindWindowW(L"EdgeZoomSettings",nullptr);if(other){ShowWindow(other,SW_SHOWNORMAL);SetForegroundWindow(other);}
        CloseHandle(mutex);return 0;
    }
    wchar_t path[32768];DWORD length=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));
    if(!length||length>=std::size(path))return 1;
    config=std::wstring(path,length);config=config.substr(0,config.find_last_of(L"\\/"))+L"\\EdgeZoom.ini";
    int vk=int(GetPrivateProfileIntW(L"EdgeZoom",L"ActivationKey",VK_MENU,config.c_str()));
    for(int i=0;i<int(std::size(bindings));++i)if(bindings[i].vk==vk)binding=i;
    const int dpi=GetDpiForSystem();
    font=CreateFontW(-MulDiv(14,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    WNDCLASSEXW c{sizeof(c)};c.hInstance=h;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    c.hIcon=LoadIconW(h,MAKEINTRESOURCEW(1));c.hIconSm=c.hIcon;c.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    c.lpfnWndProc=SettingsProc;c.lpszClassName=L"EdgeZoomSettings";RegisterClassExW(&c);
    c.lpfnWndProc=OverlayProc;c.lpszClassName=L"EdgeZoomOverlay";c.hbrBackground=nullptr;RegisterClassExW(&c);
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT rect{0,0,MulDiv(390,dpi,96),MulDiv(274,dpi,96)};AdjustWindowRectEx(&rect,style,FALSE,0);
    settings=CreateWindowExW(0,L"EdgeZoomSettings",L"EdgeZoom settings",style,CW_USEDEFAULT,CW_USEDEFAULT,
        rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,h,nullptr);
    const DWORD overlayStyle=WS_EX_TOPMOST|(wcsstr(args,L"--demo")?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW);
    overlay=CreateWindowExW(overlayStyle,L"EdgeZoomOverlay",L"EdgeZoom",WS_POPUP|WS_CLIPCHILDREN,0,0,1,1,nullptr,nullptr,h,nullptr);
    if(!settings||!overlay)return 1;
    mouseHook=SetWindowsHookExW(WH_MOUSE_LL,mouseProc,h,0);
    if(!mouseHook){MessageBoxW(settings,L"Could not register the mouse-wheel shortcut.",L"EdgeZoom",MB_OK|MB_ICONERROR);return 1;}
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");tray();
    RegisterHotKey(settings,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'Q');
    if(!wcsstr(args,L"--tray")||!trayAdded){ShowWindow(settings,SW_SHOWNORMAL);UpdateWindow(settings);}
    if(wcsstr(args,L"--demo")||wcsstr(args,L"--smoke-test"))SetTimer(settings,9,350,nullptr);
    if(wcsstr(args,L"--smoke-test")){
        smoke=true;SetTimer(settings,11,2500,nullptr);SetTimer(settings,12,4400,nullptr);
        SetTimer(settings,13,4800,nullptr);SetTimer(settings,10,5100,nullptr);
        smokeStarted=GetTickCount64();SetTimer(settings,14,7,nullptr);
    }
    MSG msg{};BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0){
        if(!active&&IsDialogMessageW(settings,&msg))continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    stop();if(mouseHook)UnhookWindowsHookEx(mouseHook);
    DestroyWindow(overlay);DeleteObject(font);if(mutex)CloseHandle(mutex);
    return result<0?1:int(msg.wParam);
}

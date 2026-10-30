#include "renderer.hpp"
#include "input.hpp"
#include "pointer.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <string>
#include <iterator>
#include <cstdio>
#include "../tests/live_fixture.hpp"

namespace {
constexpr UINT TRAY=WM_APP+1,ZOOM=WM_APP+2,STOP=WM_APP+3;
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
HFONT font=nullptr;InputRouter input;PointerVisual pointer;
Renderer renderer;ez::View view;
LiveFixture fixture;
RECT monitorBounds{},oldClip{},warmBounds{};
bool warm=false,visible=false,diagnostic=false;
ULONGLONG captureStarted=0,firstFrameMs=0;
bool active=false,clipped=false,trayAdded=false;
bool enabled=true;
int binding=0;UINT taskbarCreated=0;std::wstring config;
bool smoke=false,smokePass=false,renderHealthy=false;
bool cursorCaptureDisabled=false;
unsigned smokeChecks=0;COLORREF firstProbe=CLR_INVALID,secondProbe=CLR_INVALID;
ULONGLONG smokeStarted=0;unsigned frameSamples=0,badFrames=0;
ULONGLONG sourceChanged=0,changeLatency=0;bool inputPass=false;
POINT mappedBefore{},mappedAfter{};
// Explicit test mode only: generated events target our own fullscreen fixture.
// Abort injection if the user has switched to any other application.
bool fixtureMouse(DWORD flags,DWORD data=0,LONG dx=0,LONG dy=0){
    if(!fixture.ownsForeground())return false;
    INPUT event{};event.type=INPUT_MOUSE;event.mi.dwFlags=flags;
    event.mi.mouseData=data;event.mi.dx=dx;event.mi.dy=dy;
    return SendInput(1,&event,sizeof(event))==1;
}

void stop(){
    if(!active)return;
    active=false;KillTimer(overlay,1);
    input.end();pointer.hide();visible=false;
    if(clipped){ClipCursor(&oldClip);clipped=false;}
    const bool restore=GetForegroundWindow()==overlay;
    ShowWindow(overlay,SW_HIDE);renderer.endCapture();fixture.close();
    if(restore&&IsWindow(previousWindow))SetForegroundWindow(previousWindow);
}
void redraw(){
    if(!active)return;
    POINT position{};
    if(!smoke){auto state=input.snapshot();view=state.view;position=state.pointer;}
    renderHealthy=renderer.update(view);
    if(!renderHealthy){stop();return;}
    if(!renderer.ready()){
        if(GetTickCount64()-captureStarted>2000)stop();
        return;
    }
    if(!visible){
        firstFrameMs=GetTickCount64()-captureStarted;
        if(!pointer.open(instance)){stop();return;}
        SetWindowPos(overlay,HWND_TOPMOST,monitorBounds.left,monitorBounds.top,
            int(view.width),int(view.height),SWP_SHOWWINDOW|SWP_NOACTIVATE);
        visible=true;
    }
    position.x+=monitorBounds.left;position.y+=monitorBounds.top;
    if(!pointer.update(position))stop();
}
void warmRenderer(){
    POINT p{};GetCursorPos(&p);MONITORINFO mi{sizeof(mi)};
    if(!GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi))return;
    warmBounds=mi.rcMonitor;
    warm=renderer.open(overlay,warmBounds,false,false);diagnostic=false;
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
    captureStarted=GetTickCount64();
    const bool reused=warm&&EqualRect(&warmBounds,&monitorBounds)&&diagnostic==test;
    const bool opened=reused?renderer.beginCapture():renderer.open(overlay,monitorBounds,test);
    if(!opened){
        warm=false;renderer.close();fixture.close();MessageBoxW(settings,L"Live desktop capture could not start. Windows 10 version 2004 or newer and a compatible display driver are required.",L"EdgeZoom",MB_OK|MB_ICONERROR);return false;
    }
    cursorCaptureDisabled=renderer.cursorExcluded();
    warm=true;warmBounds=monitorBounds;diagnostic=test;
    view={double(w),double(h),1,0,0};
    if(!smoke)input.begin(view,monitorBounds,p);
    clipped=GetClipCursor(&oldClip)&&ClipCursor(&monitorBounds);
    active=true;visible=false;
    SetTimer(overlay,1,8,nullptr);return true;
}
void zoom(int delta){
    // The event was already authorized by the hook when the wheel moved.
    if(!enabled||(!active&&delta<=0))return;
    if(!start())return;
    if(smoke)view.zoom(view.scale*std::pow(1.2,double(delta)/WHEEL_DELTA),view.width/2,view.height/2);
    else {input.zoom(delta);view=input.snapshot().view;}
    if(view.scale<=1){stop();return;}
    redraw();
}
LRESULT CALLBACK OverlayProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    case WM_TIMER:redraw();return 0;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_NCHITTEST:return HTTRANSPARENT;
    case WM_DISPLAYCHANGE:case WM_CLOSE:stop();return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
void tray(){
    input.configure(enabled&&!smoke,bindings[binding].vk);
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
        control(w,L"STATIC",L"Release the button: keep zoom, keep panning.\nHold it again + scroll to adjust the zoom.\nZoom down to 100% or press Esc to return.\n\nTray icon: Pause / Quit   |   Ctrl+Alt+Q: quit",0,20,92,350,110,0);
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
    case STOP:stop();return 0;
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
        if(wp==20){KillTimer(w,20);if(!active)warmRenderer();}
        if(wp==9){KillTimer(w,9);if(start(true)){view.zoom(2,view.width/2,view.height/2);if(!smoke)input.setView(view);redraw();}else PostQuitMessage(2);}
        if(wp==14&&active){
            if(sourceChanged&&!changeLatency&&fixture.displayed(renderer.probe()))changeLatency=GetTickCount64()-sourceChanged;
            const auto elapsed=GetTickCount64()-smokeStarted;
            if((elapsed>900&&elapsed<2300)||(elapsed>2800&&elapsed<4200)){
                ++frameSamples;if(!fixture.displayed(renderer.probe()))++badFrames;
                if(elapsed<3200||elapsed>4000)view.pan(3,0);redraw();
            }
        }
        if(wp==11){
            KillTimer(w,11);
            smokeChecks=(active?1:0)|(renderHealthy?2:0)|(std::abs(view.scale-2)<1e-6?4:0)|(fixture.displayed(renderer.probe())?8:0);
            firstProbe=fixture.lastProbe;
            sourceChanged=GetTickCount64();fixture.advance(); // Change ONLY the underlying source, keeping the viewport stationary.
        }
        if(wp==12){
            KillTimer(w,12);smokeChecks|=(active?16:0)|(fixture.displayed(renderer.probe())?32:0);secondProbe=fixture.lastProbe;
            zoom(120);smokeChecks|=std::abs(view.scale-2.4)<1e-6?64:0;
        }
        if(wp==13){
            KillTimer(w,13);
            // No activation key is held: zoom must remain live until explicitly
            // scrolled back to 100%, which must restore normal cursor boundaries.
            smokeChecks|=(active&&std::abs(view.scale-2.4)<1e-6)?128:0;
            zoom(-2400);RECT clip{};GetClipCursor(&clip);
            smokeChecks|=(!active&&EqualRect(&clip,&oldClip))?256:0;
        }
        if(wp==15){
            KillTimer(w,15);
            // Verify native dispatch through the overlay using only our fixture.
            SetForegroundWindow(fixture.handle());SetFocus(fixture.handle());
            POINT p{monitorBounds.left+int(view.width/2),monitorBounds.top+int(view.height/2)};
            input.begin(view,monitorBounds,p);input.setView(view);
        }
        if(wp==16){
            KillTimer(w,16);GetCursorPos(&mappedBefore);
            fixtureMouse(MOUSEEVENTF_LEFTDOWN);
        }
        if(wp==17){KillTimer(w,17);fixtureMouse(MOUSEEVENTF_MOVE,0,40,0);}
        if(wp==18){
            KillTimer(w,18);GetCursorPos(&mappedAfter);
            fixtureMouse(MOUSEEVENTF_LEFTUP);
            fixtureMouse(MOUSEEVENTF_RIGHTDOWN);fixtureMouse(MOUSEEVENTF_RIGHTUP);
            fixtureMouse(MOUSEEVENTF_MIDDLEDOWN);fixtureMouse(MOUSEEVENTF_MIDDLEUP);
            fixtureMouse(MOUSEEVENTF_XDOWN,XBUTTON1);fixtureMouse(MOUSEEVENTF_XUP,XBUTTON1);
            fixtureMouse(MOUSEEVENTF_WHEEL,WHEEL_DELTA);fixtureMouse(MOUSEEVENTF_HWHEEL,WHEEL_DELTA);
            if(fixture.ownsForeground()){
                INPUT keys[2]{};keys[0].type=keys[1].type=INPUT_KEYBOARD;
                keys[0].ki.wScan=keys[1].ki.wScan='x';keys[0].ki.dwFlags=KEYEVENTF_UNICODE;
                keys[1].ki.dwFlags=KEYEVENTF_UNICODE|KEYEVENTF_KEYUP;SendInput(2,keys,sizeof(INPUT));
            }
        }
        if(wp==19){
            KillTimer(w,19);auto moved=input.snapshot();
            const double expected=monitorBounds.left+moved.view.x+moved.pointer.x/moved.view.scale;
            inputPass=fixture.leftDown==1&&fixture.leftUp==1&&fixture.dragged&&fixture.rightDown==1&&
                fixture.middleDown==1&&fixture.sideDown==1&&fixture.wheel==120&&fixture.horizontal==120&&
                fixture.typed==1&&abs(mappedAfter.x-expected)<=1&&mappedAfter.x>mappedBefore.x&&
                fixture.ownsForeground();
            input.end();
        }
        if(wp==10){
            smokePass=smokeChecks==511&&frameSamples>=10&&badFrames==0&&trayAdded&&cursorCaptureDisabled&&inputPass&&changeLatency>0&&changeLatency<250;
            if(smoke){
                FILE* report=_wfopen((config+L".test.txt").c_str(),L"w");
                if(report){fprintf(report,"Live integration test: %s\nChecks: %u / 511\nFirst rendered pixel: %06lx\nSecond rendered pixel: %06lx\nFrame samples while panning: %u\nUnexpected frames: %u\nTray icon registered: %s\nCursor capture disabled (read back from session): %s\n",smokePass?"PASS":"FAIL",smokeChecks,firstProbe,secondProbe,frameSamples,badFrames,trayAdded?"yes":"no",cursorCaptureDisabled?"yes":"no");
                    fprintf(report,"Native click/drag/buttons/wheel/typing test: %s\nInitial first frame: %llu ms\nSource change to rendered frame: %llu ms\nInput counts: left %u/%u, right %u, middle %u, side %u, wheel %d/%d, typed %u, drag %d\nMapped move: %ld -> %ld\n",inputPass?"PASS":"FAIL",firstFrameMs,changeLatency,fixture.leftDown,fixture.leftUp,fixture.rightDown,fixture.middleDown,fixture.sideDown,fixture.wheel,fixture.horizontal,fixture.typed,int(fixture.dragged),mappedBefore.x,mappedAfter.x);fclose(report);}
            }
            DestroyWindow(w);if(smoke)PostQuitMessage(smokePass?0:3);
        }return 0;
    case WM_QUERYENDSESSION:stop();return TRUE;
    case WM_DISPLAYCHANGE:stop();return 0;
    case WM_CLOSE:if(trayAdded)ShowWindow(w,SW_HIDE);else DestroyWindow(w);return 0;
    case WM_DESTROY:{
        stop();input.close();
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
    const DWORD overlayStyle=WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOREDIRECTIONBITMAP|(wcsstr(args,L"--demo")?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW);
    overlay=CreateWindowExW(overlayStyle,L"EdgeZoomOverlay",L"EdgeZoom",WS_POPUP|WS_CLIPCHILDREN,0,0,1,1,nullptr,nullptr,h,nullptr);
    if(!settings||!overlay)return 1;
    SetLayeredWindowAttributes(overlay,0,255,LWA_ALPHA);
    smoke=wcsstr(args,L"--smoke-test")!=nullptr;
    if(!input.open(settings,ZOOM,STOP)){
        MessageBoxW(settings,L"Could not register the input shortcuts.",L"EdgeZoom",MB_OK|MB_ICONERROR);return 1;
    }
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");tray();
    RegisterHotKey(settings,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'Q');
    if(!wcsstr(args,L"--tray")||!trayAdded){ShowWindow(settings,SW_SHOWNORMAL);UpdateWindow(settings);}
    if(!smoke&&!wcsstr(args,L"--demo"))SetTimer(settings,20,50,nullptr);
    if(wcsstr(args,L"--demo")||wcsstr(args,L"--smoke-test"))SetTimer(settings,9,350,nullptr);
    if(wcsstr(args,L"--smoke-test")){
        smoke=true;SetTimer(settings,11,2500,nullptr);SetTimer(settings,12,4400,nullptr);
        SetTimer(settings,13,4800,nullptr);SetTimer(settings,10,5100,nullptr);
        smokeStarted=GetTickCount64();SetTimer(settings,14,7,nullptr);
        SetTimer(settings,15,3200,nullptr);SetTimer(settings,16,3400,nullptr);
        SetTimer(settings,17,3550,nullptr);SetTimer(settings,18,3700,nullptr);SetTimer(settings,19,3900,nullptr);
    }
    MSG msg{};BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0){
        if(!active&&IsDialogMessageW(settings,&msg))continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    stop();input.close();
    DestroyWindow(overlay);DeleteObject(font);if(mutex)CloseHandle(mutex);
    return result<0?1:int(msg.wParam);
}

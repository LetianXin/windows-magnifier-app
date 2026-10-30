#pragma once
#include <windows.h>
#include <vector>
#include <algorithm>

// The real OS pointer stays over the source control for native input routing.
// Display its current shape once, at normal size, over the magnified image.
class PointerVisual {
    HMODULE library=nullptr;
    using Initialize=BOOL(WINAPI*)();
    using Show=BOOL(WINAPI*)(BOOL);
    Initialize magInit=nullptr,magEnd=nullptr;Show magShow=nullptr;
    HWND window=nullptr;
    HCURSOR shape=nullptr;
    HBITMAP bitmap=nullptr;
    HDC memory=nullptr;
    HGDIOBJ oldBitmap=nullptr;
    POINT hotspot{};
    SIZE size{};
    bool initialized=false,hidden=false;
    void clearBitmap(){
        if(memory){SelectObject(memory,oldBitmap);DeleteDC(memory);memory=nullptr;}
        if(bitmap){DeleteObject(bitmap);bitmap=nullptr;}
    }
    bool load(HCURSOR cursor){
        ICONINFO info{};if(!GetIconInfo(cursor,&info))return false;
        BITMAP dimensions{};GetObjectW(info.hbmColor?info.hbmColor:info.hbmMask,sizeof(dimensions),&dimensions);
        size={dimensions.bmWidth,info.hbmColor?dimensions.bmHeight:dimensions.bmHeight/2};
        hotspot={LONG(info.xHotspot),LONG(info.yHotspot)};
        if(info.hbmColor)DeleteObject(info.hbmColor);if(info.hbmMask)DeleteObject(info.hbmMask);
        if(size.cx<=0||size.cy<=0||size.cx>1024||size.cy>1024)return false;
        clearBitmap();
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=size.cx;
        bi.bmiHeader.biHeight=-size.cy;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;bitmap=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        memory=CreateCompatibleDC(nullptr);if(!bitmap||!memory)return false;
        oldBitmap=SelectObject(memory,bitmap);
        auto* pixels=static_cast<DWORD*>(bits);const size_t count=size_t(size.cx)*size.cy;
        std::fill(pixels,pixels+count,0x00ffffff);
        DrawIconEx(memory,0,0,cursor,size.cx,size.cy,0,nullptr,DI_NORMAL);
        std::vector<DWORD> white(pixels,pixels+count);
        std::fill(pixels,pixels+count,0);
        DrawIconEx(memory,0,0,cursor,size.cx,size.cy,0,nullptr,DI_NORMAL);
        // Reconstruct premultiplied alpha for both monochrome and color cursors.
        for(size_t i=0;i<count;++i){
            int difference=int(white[i]&255)-int(pixels[i]&255);
            unsigned alpha=255-unsigned(std::clamp(difference,0,255));
            pixels[i]=(pixels[i]&0x00ffffff)|(alpha<<24);
        }
        shape=cursor;return true;
    }
public:
    bool open(HINSTANCE instance){
        if(!initialized){
            library=LoadLibraryExW(L"Magnification.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(!library)return false;
            magInit=reinterpret_cast<Initialize>(GetProcAddress(library,"MagInitialize"));
            magEnd=reinterpret_cast<Initialize>(GetProcAddress(library,"MagUninitialize"));
            magShow=reinterpret_cast<Show>(GetProcAddress(library,"MagShowSystemCursor"));
            initialized=magInit&&magEnd&&magShow&&magInit();if(!initialized)return false;
        }
        if(!window){
            WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"EdgeZoomPointer";
            RegisterClassW(&cls);
            window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT,
                cls.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
            if(!window||!SetWindowDisplayAffinity(window,WDA_EXCLUDEFROMCAPTURE))return false;
        }
        hidden=magShow(FALSE)!=FALSE;return hidden;
    }
    bool update(POINT screen){
        CURSORINFO ci{sizeof(ci)};if(!GetCursorInfo(&ci))return false;
        HCURSOR cursor=ci.hCursor?ci.hCursor:LoadCursorW(nullptr,IDC_ARROW);
        if(cursor!=shape&&!load(cursor))return false;
        POINT position{screen.x-hotspot.x,screen.y-hotspot.y},origin{};
        BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(window,nullptr,&position,&size,memory,&origin,0,&blend,ULW_ALPHA))return false;
        SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        return true;
    }
    void hide(){if(hidden){magShow(TRUE);hidden=false;}if(window)ShowWindow(window,SW_HIDE);}
    ~PointerVisual(){hide();clearBitmap();if(window)DestroyWindow(window);if(initialized)magEnd();if(library)FreeLibrary(library);}
};

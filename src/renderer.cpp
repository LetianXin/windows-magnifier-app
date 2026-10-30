#include "renderer.hpp"

bool Renderer::open(HWND host,RECT monitor){
    close();bounds=monitor;
    // Resolve only from the Windows system directory, never beside the EXE.
    library=LoadLibraryExW(L"Magnification.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library)return false;
    initialize=reinterpret_cast<decltype(initialize)>(GetProcAddress(library,"MagInitialize"));
    uninitialize=reinterpret_cast<decltype(uninitialize)>(GetProcAddress(library,"MagUninitialize"));
    transform=reinterpret_cast<decltype(transform)>(GetProcAddress(library,"MagSetWindowTransform"));
    source=reinterpret_cast<decltype(source)>(GetProcAddress(library,"MagSetWindowSource"));
    filter=reinterpret_cast<decltype(filter)>(GetProcAddress(library,"MagSetWindowFilterList"));
    if(!initialize||!uninitialize||!transform||!source||!filter){close();return false;}
    initialized=initialize()!=FALSE;
    if(!initialized){close();return false;}
    // Deliberately omit MS_SHOWMAGNIFIEDCURSOR. Leave the OS cursor untouched.
    magnifier=CreateWindowW(L"Magnifier",L"Live desktop",WS_CHILD|WS_VISIBLE|WS_DISABLED,
        0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,host,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!magnifier||!filter(magnifier,MW_FILTERMODE_EXCLUDE,1,&host)){close();return false;}
    return true;
}
bool Renderer::update(const ez::View& v){
    if(!magnifier)return false;
    if(scale!=v.scale){
        MAGTRANSFORM matrix{};matrix.v[0][0]=matrix.v[1][1]=float(v.scale);matrix.v[2][2]=1;
        if(!transform(magnifier,&matrix))return false;
        scale=v.scale;
    }
    const LONG width=LONG(std::ceil(v.width/v.scale)),height=LONG(std::ceil(v.height/v.scale));
    const LONG x=std::clamp(LONG(std::lround(v.x)),0L,LONG(v.width)-width);
    const LONG y=std::clamp(LONG(std::lround(v.y)),0L,LONG(v.height)-height);
    RECT region{bounds.left+x,bounds.top+y,bounds.left+x+width,bounds.top+y+height};
    if(!source(magnifier,region))return false;
    InvalidateRect(magnifier,nullptr,FALSE);
    return true;
}
void Renderer::close(){
    if(magnifier){DestroyWindow(magnifier);magnifier=nullptr;}
    if(initialized){uninitialize();initialized=false;}
    if(library){FreeLibrary(library);library=nullptr;}
    scale=0;
}

#pragma once
#include <algorithm>
#include <cmath>

namespace ez {
// Pure screen-space math. Offsets are always in source pixels.
struct View {
    double width=1, height=1, scale=1, x=0, y=0;
    void clamp() {
        scale=std::clamp(scale,1.0,16.0);
        x=std::clamp(x,0.0,width-width/scale);
        y=std::clamp(y,0.0,height-height/scale);
    }
    void zoom(double next,double px,double py) {
        px=std::clamp(px,0.0,width); py=std::clamp(py,0.0,height);
        const double sx=x+px/scale, sy=y+py/scale;
        scale=std::clamp(next,1.0,16.0);
        x=sx-px/scale; y=sy-py/scale; clamp();
    }
    void pan(double dx,double dy) { x+=dx/scale; y+=dy/scale; clamp(); }
};
// Pan only on outward motion AFTER arriving at an edge; never on a timer.
inline double edgePush(double previous,double current,double extent,double delta) {
    if(previous<=0 && current<=0 && delta<0) return delta;
    if(previous>=extent-1 && current>=extent-1 && delta>0) return delta;
    return 0;
}
}

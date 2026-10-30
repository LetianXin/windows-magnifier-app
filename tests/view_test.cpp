#include "../src/view.hpp"
#undef NDEBUG
#include <cassert>
#include <iostream>
bool near(double a,double b){return std::abs(a-b)<1e-7;}
int main(){
 ez::View v{1920,1080,1,0,0};v.zoom(2,960,540);
 assert(near(v.x,480)&&near(v.y,270));
 double sx=v.x+800/v.scale,sy=v.y+400/v.scale;v.zoom(4,800,400);
 assert(near(v.x+800/v.scale,sx)&&near(v.y+400/v.scale,sy));
 assert(ez::edgePush(500,510,1920,10)==0);
 assert(ez::edgePush(1910,1919,1920,10)==0); // arrival does not pan
 assert(ez::edgePush(1919,1919,1920,0)==0); // resting does not pan
 assert(ez::edgePush(1919,1919,1920,25)==25);
 assert(ez::edgePush(0,0,1920,-12)==-12);
 assert(ez::edgePush(0,0,1080,-8)==-8); // top
 assert(ez::edgePush(1079,1079,1080,8)==8); // bottom
 assert(ez::edgePush(1919,1918,1920,-1)==0); // move away
 assert(ez::edgePush(0,0,1920,12)==0); // inward motion
 ez::View push{1920,1080,4,100,100};
 push.pan(ez::edgePush(1919,1919,1920,40),0);assert(push.x==110);
 for(int i=0;i<1000;++i)push.pan(ez::edgePush(1919,1919,1920,0),0);
 assert(push.x==110); // no drift regardless of elapsed messages
 v.pan(1e9,1e9);assert(near(v.x,1440)&&near(v.y,810));
 v.pan(-1e9,-1e9);assert(v.x==0&&v.y==0);
 v.zoom(1,1800,1000);assert(v.x==0&&v.y==0&&v.scale==1);
 v.zoom(100,0,0);assert(v.scale==16);v.zoom(0,0,0);assert(v.scale==1);
 for(int i=0;i<1000;++i){v.zoom(1+(i%150)/10.0,i%1920,i%1080);v.pan(i-500,500-i);assert(v.x>=0&&v.y>=0&&v.x+v.width/v.scale<=v.width+1e-7&&v.y+v.height/v.scale<=v.height+1e-7);}
 std::cout<<"PASS: anchored zoom, edge arrival, stationary edge, four outward pushes, inward motion, proportional pan, no drift, source bounds, zoom limits, 1000 mixed operations.\n";
}

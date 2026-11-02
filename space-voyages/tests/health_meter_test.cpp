// SPDX-License-Identifier: GPL-3.0-or-later
#include "health_meter.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spacevoyages;
namespace {
unsigned checks;
void require(bool okay,const char* message){++checks;if(!okay){std::fprintf(stderr,"health_meter: %s\n",message);std::exit(1);}}
}
int main(){
 // Independent native CLUT loop: compare all palette entries at every native
 // health step, including the initial native hp_bar255 palette update.
 for(int hp=0;hp<=256;++hp){
  std::array<bool,256> original{};original.fill(true);
  int v=int(float(hp)*.00390625f*251.f)+2;if(v<2)v=2;if(v>253)v=253;
  for(int j=2;j<256;++j)if(j>=v)original[j]=false;
  const auto threshold=health_meter::palette_threshold(float(hp),256.f);
  for(int j=0;j<256;++j)require(original[j]==!(j>=2&&j>=threshold),"GPU palette threshold matches native loop");
 }
 require(health_meter::palette_threshold(-20,100)==2,"negative hull is empty");
 require(health_meter::palette_threshold(200,100)==253,"over-full hull clamps");
 require(health_meter::palette_threshold(100,0)==2,"invalid maximum fails empty");
 require(health_meter::palette_threshold(std::numeric_limits<float>::quiet_NaN(),100)==2,"invalid display health fails empty");
 float previous=0;
 for(int i=0;i<=1000;++i){const float p=i/1000.f,black=health_meter::destruction_black(p);require(black>=previous&&black>=0&&black<=1,"destruction fade is monotonic and bounded");if(p<=.45f)require(black==0,"native explosion remains visible before fade");previous=black;}
 require(health_meter::destruction_black(1)==1,"acknowledge only a fully black final frame");
 std::printf("Health meter native CLUT and destruction fade checks:%u passed\n",checks);
}

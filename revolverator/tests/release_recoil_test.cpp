// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "release_recoil.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* what){++checks;if(!condition){std::fprintf(stderr,"release recoil: %s\n",what);std::exit(1);}}
Frame frame(){Frame f;f.equipped=f.playing=f.alive=true;f.epoch=1;return f;}
}
int main(){
 ReleaseRecoil recoil;auto f=frame();require(!recoil.active(),"idle never freezes movement");
 recoil.start(f);require(recoil.active()&&recoil.remaining()==.15f,"release starts a 150ms locomotion hold immediately");
 for(unsigned i=0;i<8;++i){recoil.tick(f);require(recoil.active(),"hold retained through eighth 60Hz tick");}
 recoil.tick(f);require(!recoil.active()&&recoil.remaining()==0,"ninth tick ends hold without float residue");
 recoil.start(f);f.paused=true;f.playing=false;
 for(unsigned i=0;i<600;++i)recoil.tick(f);
 require(recoil.active()&&recoil.remaining()==.15f,"pause never consumes recoil time");
 f.paused=false;f.playing=true;for(unsigned i=0;i<9;++i)recoil.tick(f);
 require(!recoil.active(),"pause/resume keeps the exact remaining duration");
 for(unsigned reason=0;reason<9;++reason){
  f=frame();recoil.start(f);
  if(reason==0)f.equipped=false;
  if(reason==1)f.alive=false;
  if(reason==2)f.playing=false;
  if(reason==3)++f.epoch;
  if(reason==4)f.seconds=0;
  if(reason==5)f.seconds=std::numeric_limits<float>::quiet_NaN();
  if(reason==6)f.seconds=.2f;
  if(reason==7)f.position.x=std::numeric_limits<float>::infinity();
  if(reason==8)f.yaw=std::numeric_limits<float>::infinity();
  recoil.tick(f);require(!recoil.active(),"invalid lifecycle cancels hold immediately");
  f=frame();recoil.tick(f);require(!recoil.active(),"re-equip/recovery cannot revive old hold");
 }
 f=frame();recoil.start(f);recoil.cancel();require(!recoil.active(),"explicit cancel is idempotent");
 recoil.cancel();require(!recoil.active(),"repeated reset remains idle");
 for(unsigned invalid=0;invalid<4;++invalid){
  f=frame();if(invalid==0)f.equipped=false;if(invalid==1)f.alive=false;if(invalid==2)f.playing=false;if(invalid==3)f.paused=true;
  recoil.start(f);require(!recoil.active(),"a non-gameplay release cannot arm a future freeze");
 }
 f=frame();f.seconds=.025f;recoil.start(f);for(unsigned i=0;i<6;++i)recoil.tick(f);
 require(!recoil.active(),"hold lasts 150ms at a different tick duration");
 std::printf("release recoil: %u checks passed\n",checks);
}

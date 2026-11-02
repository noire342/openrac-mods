// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_equip_audio.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace revolverator;
namespace {
unsigned checks,calls;int result;unsigned actor=0x30000;
void require(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"equip audio: %s\n",label);std::exit(1);}}
int loaded(){return 1;}
unsigned relocate(unsigned at){return at;}
int play(int entry,int flags,unsigned owner){require(entry==2&&flags==0&&owner==actor,"uses native mechanical cue on the actual held owner");++calls;return result;}
void* function(const char* name){require(!std::strcmp(name,"FUN_0022da68")||!std::strcmp(name,"func_0022ED80"),"native sound function is region bound");return reinterpret_cast<void*>(&play);}
template<class T>void put(std::vector<unsigned char>& ram,unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
}
int main(){
 for(const char* version:{"ntsc","pal"}){
  std::vector<unsigned char> ram(0x2000000);
  openrac_api api{};api.memory=ram.data();api.game="rac1";api.version=version;
  api.loaded_level=loaded;api.relocate_data=relocate;api.function=function;
  NativeEquipAudio audio(&api);
  const unsigned hero=audio.global(0x13f350),klass=0x40000,definitions=0x50000,sequence=0x60000;
  put(ram,hero+0x1090,actor);put(ram,hero+0x10b8,18);put(ram,actor+0x24,klass);
  put<unsigned short>(ram,actor+0xa6,2017);put<unsigned char>(ram,klass+0xd,3);
  put(ram,klass+0x28,definitions);put(ram,definitions+2*32+0x1c,3);
  put(ram,klass+0x48,sequence);put<unsigned char>(ram,sequence+0x10,5);put<unsigned char>(ram,sequence+0x12,1);
  put(ram,sequence+0x1c+5*4,524290u);
  for(unsigned current:{0u,1u})for(unsigned target:{0u,1u}){
   put<unsigned char>(ram,actor+0x52,current);put<unsigned char>(ram,actor+0x53,target);
   put<unsigned char>(ram,actor+0x7e,1);
   calls=0;result=4;auto expected=ram;const auto played=audio.play(actor);
   const bool automatic=current==0||target==0;
   if(automatic)expected[actor+0x7e]=0;
   require(played==NativeEquipAudio::Result::Played&&calls==1,"fresh and reused actors receive one logical equip cue");
   require(ram==expected,"only the owned appearance event count changes; shared sound/class/sequence memory stays untouched");
  }
  put<unsigned char>(ram,actor+0x52,1);put<unsigned char>(ram,actor+0x53,1);
  for(unsigned render:{180u,2017u,2018u}){
   put<unsigned short>(ram,actor+0xa6,static_cast<unsigned short>(render));calls=0;result=0;
   require(audio.play(actor)==NativeEquipAudio::Result::Played&&calls==1,"native and projected base/Gold appearances use the same native sound adapter");
  }
  result=-1;require(audio.play(actor)==NativeEquipAudio::Result::Rejected,"native pool exhaustion never changes gameplay or retries every frame");
  result=0;calls=0;put(ram,hero+0x1090,actor+256);
  require(audio.play(actor)==NativeEquipAudio::Result::Unavailable&&calls==0,"preview and stale held actors cannot emit equip audio");
  put(ram,hero+0x1090,actor);put(ram,hero+0x10b8,15);
  require(audio.play(actor)==NativeEquipAudio::Result::Unavailable,"another native weapon is outside custom audio ownership");
  put(ram,hero+0x10b8,18);put<unsigned char>(ram,klass+0xd,2);
  require(audio.play(actor)==NativeEquipAudio::Result::Unavailable,"missing class sound entry fails without calling native audio");
  put<unsigned char>(ram,klass+0xd,3);put(ram,definitions+2*32+0x1c,0);
  require(audio.play(actor)==NativeEquipAudio::Result::Unavailable,"unloaded bank cannot queue an invalid sound");
  put(ram,definitions+2*32+0x1c,3);put<unsigned char>(ram,actor+0x52,0);put<unsigned char>(ram,actor+0x53,0);
  put<unsigned char>(ram,sequence+0x12,2);put<unsigned char>(ram,actor+0x7e,2);calls=0;
  auto expected=ram;
  require(audio.play(actor)==NativeEquipAudio::Result::NativeAppearance&&calls==0&&ram==expected,"unknown multi-event appearance keeps its original native audio ownership");
  put<unsigned char>(ram,sequence+0x12,1);put(ram,sequence+0x1c+5*4,524289u);expected=ram;
  require(audio.play(actor)==NativeEquipAudio::Result::NativeAppearance&&ram==expected,"another appearance trigger cannot be silently suppressed");
  put(ram,sequence+0x1c+5*4,524290u);put<unsigned char>(ram,actor+0x7e,1);calls=0;
  expected=ram;expected[actor+0x7e]=0;
  require(audio.suppress_native_appearance(actor)&&calls==0&&ram==expected,"a menu-recreated actor loses only its legacy appearance event without replaying audio");
  for(unsigned i=0;i<20;++i)require(audio.suppress_native_appearance(actor)&&calls==0&&ram==expected,"native/render frame repeats cannot replay a semantic equip cue");
  put<unsigned char>(ram,actor+0x52,1);put<unsigned char>(ram,actor+0x53,1);put<unsigned char>(ram,actor+0x7e,3);expected=ram;
  require(!audio.suppress_native_appearance(actor)&&ram==expected,"ordinary idle/attack sound events are preserved");
  put<unsigned char>(ram,actor+0x52,0);put<unsigned char>(ram,actor+0x53,0);put(ram,hero+0x1090,actor+256);expected=ram;
  require(!audio.suppress_native_appearance(actor)&&ram==expected,"foreign menu preview actors are never silenced");
 }
 std::printf("equip audio: %u checks passed\n",checks);
}

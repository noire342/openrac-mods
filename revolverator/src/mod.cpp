// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "native_world.hpp"
#include "presentation.hpp"
#include "native_vendor.hpp"
#include "native_class_alias.hpp"
#include "revolverator_api.h"
#include <memory>
#include <exception>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
namespace {
using namespace revolverator;
struct Session {
 const openrac_api* api;openrac_mod* mod;GuestArena arena,core_arena;NativeAmmunition ammunition;NativeAmmoAlias alias;NativeVendor vendor;NativeRotor rotor;NativeWorld world;Controller controller;Presentation presentation;
 bool primary=false;unsigned epoch=0;int level=-1;
 Session(const openrac_api* a,openrac_mod* m):api(a),mod(m),ammunition(a,&arena,true),alias(a),vendor(a),rotor(a,core_arena),world(a,m,ammunition,rotor,core_arena),controller(world),presentation(a,m,&arena,true){}
 ~Session(){controller.cancel();rotor.reset(true);presentation.reset();vendor.reset();alias.reset();ammunition.reset();}
 void frame(){
  const int current=api->loaded_level();
  if(current!=level){controller.cancel();rotor.reset(true);presentation.reset();vendor.reset();alias.reset();ammunition.reset();world.reset();level=current;++epoch;primary=false;}
  if(current<0||current>18)return;
  ammunition.frame();alias.frame();vendor.set_preview_class(alias.ready()?NativeAmmoAlias::native_class:225);vendor.frame();
  const auto f=world.snapshot(primary,epoch);controller.tick(f);
  if(controller.phase()!=Phase::Spinning&&!f.paused)world.pose(controller.phase(),controller.spin_angle());
  if(f.equipped)rotor.update(world.weapon(),ammunition.count(),controller.phase(),controller.charge(),controller.spin_angle(),f.seconds,f.playing,f.paused);
  else rotor.reset();
  presentation.frame(f.equipped);
 }
};
std::unique_ptr<Session> session;
using U32=unsigned;
using Update=void(*)(U32);
using Pad=void(*)(U32,U32,int);
using Load=U32(*)(int);
using GetHit=U32(*)(U32,int,int);
using Death=void(*)(U32,int,int);
using Advance=void(*)();
using BuildVendor=void(*)(int);
Update walloper_original{},relay_original{},ammo_original{};Pad pad_original{};Load load_original{};
GetHit hit_original{};Death death_original{};Advance animation_original{};
BuildVendor vendor_original{};Update vendor_preview_original{};
template<int Index>struct EnemyHook{
 static inline Update original{};
 static void update(U32 moby){if(!session||!session->world.frozen(moby))original(moby);}
};
void hook(const char* id,const char* signature,void* replacement,void** original){
 const int r=session->api->hook(session->mod,id,signature,replacement,original);
 if(r!=OPENRAC_HOOK_OK)throw std::runtime_error(std::string("Required hook ")+id+" unavailable or incompatible ("+std::to_string(r)+")");
}
template<int Index>void enemy(const char* id){hook(id,"void(u32)",reinterpret_cast<void*>(&EnemyHook<Index>::update),reinterpret_cast<void**>(&EnemyHook<Index>::original));}
void pad(U32 address,U32 report,int size){
 pad_original(address,report,size);
 if(!session||address!=session->world.global(0x13c940))return;
 auto& w=session->world;
 const auto f=w.snapshot(false,session->epoch);
 if(!f.equipped||!f.playing){session->primary=false;return;}
 session->primary=(w.read<unsigned>(address+0x1a0)&0x20)!=0;
 for(unsigned off:{0x1a0u,0x1a4u,0x1a8u,0x1b0u,0x1b4u,0x1b8u,0x1c0u,0x1c4u,0x1c8u})w.write<unsigned>(address+off,w.read<unsigned>(address+off)&~0x20u);
 const unsigned index=w.read<unsigned short>(address+0x18e);
 if(index<30){const auto at=address+0x1e0+((index+29)%30)*4;w.write<unsigned>(at,w.read<unsigned>(at)&~0x20u);}
}
void relay(U32 element){
 if(session&&session->presentation.owns(element)){
  session->presentation.before_hud(element);session->presentation.draw_base(element,relay_original);session->presentation.after_hud(element,session->controller);
 }else relay_original(element);
}
void walloper(U32 moby){
 // WalloperUpdate only animates its electrical arcs. The drill uses the native
 // held-item transform but does not inherit the Walloper's aura or sparks.
 if(!session||session->world.read<int>(session->world.hero()+0x10b8)!=18)walloper_original(moby);
 else if(session->controller.phase()==Phase::Spinning)session->world.align_weapon(session->controller.spin_angle());
 if(session)session->rotor.apply();
}
void ammo_pickup(U32 moby){
 if(session)session->ammunition.before_update(moby);
 ammo_original(moby);
 if(session)session->ammunition.after_update(moby);
}
void build_vendor(int skip){
 if(session)session->vendor.before_build();
 vendor_original(skip);
 if(session)session->vendor.after_build(skip);
}
void vendor_preview(U32 context){
 if(session)session->vendor.before_vendor_update();
 vendor_preview_original(context);
 if(session)session->vendor.after_vendor_update();
}
U32 load(int full){
 if(session){session->controller.cancel();session->rotor.reset(true);session->presentation.reset();session->vendor.reset();session->alias.reset();session->ammunition.reset();session->world.reset();session->core_arena.reset();session->arena.reset();}
 const auto out=load_original(full);
 auto end=out;
 if(session){
  const auto start=(out+15u)&~15u;
  const unsigned available=start<GuestArena::ceiling?(GuestArena::ceiling-start)&~15u:0;
  end=session->arena.reserve(out,std::min(128u*1024,available));
  unsigned core=session->arena.allocate(1024);
  if(!core)core=session->world.function<unsigned(*)(unsigned)>("FUN_001ff288")(1024);
  session->core_arena.reserve(core,1024);
  if(!core)session->api->log(session->mod,OPENRAC_LOG_WARN,"Revolverator: insufficient native memory for arm and rotor");
  if(session->api->setting_bool(session->mod,"trace")){
   char message[128];std::snprintf(message,sizeof message,"arena level=%d begin=%08x end=%08x",session->api->loaded_level(),session->arena.begin(),end);
   session->api->log(session->mod,OPENRAC_LOG_INFO,message);
  }
  session->level=session->api->loaded_level();++session->epoch;session->primary=false;session->world.reset();session->ammunition.frame();session->alias.frame();session->vendor.set_preview_class(session->alias.ready()?NativeAmmoAlias::native_class:225);session->vendor.frame();session->presentation.names();
 }
 return end;
}
U32 get_hit(U32 moby,int mask,int keep){
 const auto hit=hit_original(moby,mask,keep);
 if(session&&capturable_class(int(session->world.token(moby).klass))&&session->controller.phase()!=Phase::Idle&&session->world.blocks_native_hit(hit)){
  if(keep)hit_original(moby,mask,0);return 0;
 }
 return hit;
}
void death(U32 moby,int flags,int index){
 if(session&&session->world.frozen(moby))session->controller.cancel();death_original(moby,flags,index);
}
void animation(){animation_original();if(session)session->presentation.names();}
int get_state(revolverator_state_v1* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 const auto& c=session->controller;const auto& counts=c.counters();
 *out={sizeof(*out),unsigned(c.phase()),18,c.target().klass,c.charge(),counts.captures,counts.throws,counts.impacts};return 1;
}
int cancel(){if(!session)return 0;session->controller.cancel();return 1;}
int get_mechanics(revolverator_mechanics_v1* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 const auto& motor=session->rotor.controller();
 *out={sizeof(*out),unsigned(session->ammunition.count()),unsigned(session->ammunition.maximum()),motor.angle(),motor.speed(),unsigned(motor.dry_firing()),unsigned(motor.synchronized())};return 1;
}
const revolverator_api_v1 service{sizeof(service),1,get_state,cancel,get_mechanics};
}
extern "C" { OPENRAC_MOD_DEFINE("revolverator","0.1.0") }
extern "C" OPENRAC_MOD_EXPORT const revolverator_api_v1* openrac_revolverator_query(unsigned version){return version==1&&session?&service:nullptr;}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api,openrac_mod* mod){
 try{
  if(!api||!api->memory||std::strcmp(api->game,"rac1")||std::strcmp(api->version,"ntsc"))throw std::runtime_error("Revolverator 0.1.0 requires Ratchet & Clank NTSC-U");
  session=std::make_unique<Session>(api,mod);
  hook("rac1.level.load_gameplay","u32(i32)",reinterpret_cast<void*>(load),reinterpret_cast<void**>(&load_original));
  hook("rac1.pad.process_report","void(u32,u32,i32)",reinterpret_cast<void*>(pad),reinterpret_cast<void**>(&pad_original));
  hook("rac1.moby.moby180.update","void(u32)",reinterpret_cast<void*>(walloper),reinterpret_cast<void**>(&walloper_original));
  hook("rac1.moby.ammo_pickup.update","void(u32)",reinterpret_cast<void*>(ammo_pickup),reinterpret_cast<void**>(&ammo_original));
  hook(NativeVendor::build_hook,NativeVendor::build_signature,reinterpret_cast<void*>(build_vendor),reinterpret_cast<void**>(&vendor_original));
  // A missing hook marks the whole mod as failed, even if hook()'s return is
  // ignored. The native class alias supplies the vendor model on stock builds.
  hook("rac1.moby.bolt.update","void(u32)",reinterpret_cast<void*>(relay),reinterpret_cast<void**>(&relay_original));
  hook("rac1.combat.get_hit","u32(u32,i32,i32)",reinterpret_cast<void*>(get_hit),reinterpret_cast<void**>(&hit_original));
  hook("rac1.moby.record_death","void(u32,i32,i32)",reinterpret_cast<void*>(death),reinterpret_cast<void**>(&death_original));
  hook("rac1.player.advance_animation","void()",reinterpret_cast<void*>(animation),reinterpret_cast<void**>(&animation_original));
  enemy<0>("rac1.moby.birdbot.update");enemy<1>("rac1.moby.sand_shark.update");enemy<2>("rac1.moby.wb_moby.update");
  enemy<3>("rac1.moby.scoutbot.update");enemy<4>("rac1.moby.spiderbot.update");enemy<5>("rac1.moby.blarg_trooper.update");
  enemy<6>("rac1.moby.moby294.update");enemy<7>("rac1.moby.moby573.update");enemy<8>("rac1.moby.moby1382.update");
  enemy<9>("rac1.moby.peckbot.update");enemy<10>("rac1.moby.extermibot.update");enemy<11>("rac1.moby.moby1048.update");
  enemy<12>("rac1.moby.sharkigator_35.update");enemy<13>("rac1.moby.sharkigator_1059.update");
  if(api->on_frame(mod,+[](void*){if(session)session->frame();},nullptr))throw std::runtime_error("Frame registration failed");
  api->log(mod,OPENRAC_LOG_INFO,"Revolverator 0.1.0: native Walloper slot, capture/spin/throw, copy-only API v1");
  return 0;
 }catch(const std::exception& e){api->fail(mod,e.what());session.reset();return 1;}
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){session.reset();}

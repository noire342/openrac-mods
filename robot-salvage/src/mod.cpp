// SPDX-License-Identifier: GPL-3.0-or-later
#include <openrac/openrac_mod.h>
#include "core_api.hpp"
#include "native_hooks.hpp"
#include "native_world.hpp"
#include "native_presentation.hpp"
#include "native_store.hpp"
#include "native_thumbnails.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <shlobj.h>

namespace {
using namespace salvage;
using U32=unsigned;
std::filesystem::path cache_path(){
 wchar_t* environment=nullptr;std::size_t length=0;_wdupenv_s(&environment,&length,L"LOCALAPPDATA");
 if(environment&&*environment){const auto path=std::filesystem::path(environment)/L"OpenRAC/robot-salvage/thumbnails";std::free(environment);return path;}std::free(environment);
 PWSTR local=nullptr;const auto result=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local);const auto base=SUCCEEDED(result)&&local?std::filesystem::path(local):std::filesystem::temp_directory_path();CoTaskMemFree(local);return base/L"OpenRAC/robot-salvage/thumbnails";
}
struct Session {
 const openrac_api* api;openrac_mod* mod;GuestArena arena;Catalog catalog;Inventory inventory;World fragments;Events events;
 Collector collector;NativeWorld world;NativeSuckCannon cannon;NativeFragmentSources sources;NativePresentation presentation;NativeStore store;ApiService service;NativeThumbnails thumbnails;
 bool ng_plus=false,triangle_claimed=false;int current=-1;
 Session(const openrac_api* a,openrac_mod* m,const arsenal_pause_api_v1* pause):api(a),mod(m),collector(catalog,inventory,fragments,events),world(a,m,arena,catalog,fragments,&events),cannon(a,arena,world),sources(a,[this](FragmentOrigin origin,unsigned actor,const FragmentMotion& motion){return world.adopt(origin,actor,motion);}),presentation(a,m,arena,catalog,inventory),store(a,m,catalog,inventory),service(catalog,inventory,fragments,events),thumbnails(a,arena,cache_path()){
  presentation.set_pause_service(pause);
  world.set_materialized_callback([this](unsigned actor){cannon.offer(actor);});
  store.set_restored_callback([this]{cannon.reset();sources.reset();collector.reset_input(true);world.explicit_load();events.publish({0,EventKind::InventoryRestored,0,0,0,0,inventory.revision(),fragments.visit()});events.publish({0,EventKind::VisitChanged,0,0,0,0,inventory.revision(),fragments.visit()});});
  presentation.set_thumbnail_renderer([this](unsigned screen,const PartDescriptor& part){thumbnails.draw(screen,part);});
 }
 void before_load(int next){sources.reset();cannon.reset();presentation.reset();thumbnails.reset();const auto visit=fragments.visit();world.before_load(next);arena.reset();collector.reset_input(true);if(visit!=fragments.visit())events.publish({0,EventKind::VisitChanged,0,0,0,0,inventory.revision(),fragments.visit()});}
 void frame(){
  if(world.level_transitioning())return;
  const int now=api->loaded_level();if(now<0||now>18)return;
  if(current!=now){current=now;world.after_load(now);}
  if(world.read<int>(world.mode())==0){sources.frame();cannon.frame();}
  world.frame();presentation.frame();
  auto input=world.input(false);input.native_interaction|=presentation.native_interaction_active();
  const auto candidate=collector.target(input,[this](const Fragment& f){return world.visible(f);});
  const auto fragment=candidate?fragments.find(*candidate):nullptr;presentation.set_candidate(fragment?fragment->part:0,fragment?std::optional<Vec3>{fragment->position}:std::optional<Vec3>{});
 }
 void pad(int port,unsigned char* report,int size){
  if(port!=0||!report||size<2)return;
  if(world.level_transitioning())return;
  const bool triangle=(report[1]&0x10u)==0;
  if(!triangle)triangle_claimed=false;
  if(presentation.route_triangle(triangle)){report[1]|=0x10u;if(size>10)report[10]=0;return;}
  auto input=world.input(triangle);input.native_interaction|=presentation.native_interaction_active();
  const auto result=collector.update(input,[this](const Fragment& f){return world.visible(f);});
  if(result.status==Status::Ok){triangle_claimed=true;presentation.notify_collected(result.part);world.retire_collected();}
  // Keep ownership of the entire physical press, so masking its first frame
  // cannot create a second, artificial native quick-select edge while held.
  if(triangle_claimed){report[1]|=0x10u;if(size>10)report[10]=0;}
 }
};
std::unique_ptr<NativeHooks> hooks;
std::unique_ptr<Session> session;
U32(*load_original)(int){};
using Spawn=U32(*)(U32,int,U32,U32,int,int,U32,U32,float,U32);
Spawn spawn_original{};
FlyingDebris81Factory flying_original{};PlasmabotDebris325Factory plasma_original{};PathDebris403Factory path_original{};BomberJetDebris1510Factory jet_original{};
U32(*cannon_record_original)(U32){};
int(*cannon_claim_original)(U32,U32,U32,U32,U32){};int(*cannon_store_original)(U32,U32){};
int(*cannon_grab_original)(U32,U32,U32){};
NativeSuckCannon::Launch cannon_launch_original{};int(*cannon_cancel_original)(U32){};NativeSuckCannon::Finalize cannon_finalize_original{};
void(*prepare_original)(int,int){};int(*autosave_original)(int,int){};
void(*io_original)(){};void(*serialize_original)(U32){};void(*restore_original)(U32){};
void(*draw_original)(){};
void(*pause_draw_original)(int){};
void(*hud_draw_original)(){};
int(*restore_data_original)(U32,int,U32){};void(*new_game_original)(){};void(*ng_original)(int){};
U32 load(int value){
 if(session){const auto arg=unsigned(value);if(session->presentation.owns_callback(arg))return session->presentation.invoke_callback(arg);if(session->cannon.owns(arg)){session->cannon.update(arg);return 0;}if(session->world.owns_callback(arg))return 0;if(arg>18u)return load_original(value);session->before_load(session->api->loaded_level());}
 const auto heap=load_original(value);
 if(!session)return heap;
 const auto start=(heap+15u)&~15u;
 const unsigned available=start<GuestArena::ceiling?(GuestArena::ceiling-start)&~15u:0;
 const auto end=session->arena.reserve(heap,std::min(128u*1024,available));session->world.after_load(session->api->loaded_level());
 char text[192];std::snprintf(text,sizeof text,"salvage level storage flag=%d overlay=%d heap=%08x end=%08x",value,session->api->loaded_level(),heap,end);session->api->log(session->mod,OPENRAC_LOG_INFO,text);return end;
}
U32 spawn(U32 source,int klass,U32 position,U32 rotation,int life,int mode,U32 velocity,U32 angular,float gravity,U32 sphere){
 const auto result=spawn_original(source,klass,position,rotation,life,mode,velocity,angular,gravity,sphere);
 if(session&&result){const bool adopted=session->world.adopt(source,result);static unsigned traced=0;if(traced++<20){char text[224];std::snprintf(text,sizeof text,"salvage factory source=%08x sourceclass=%u requested=%d actor=%08x adopted=%d arena=%08x delta=%.6f",source,unsigned(session->world.read<unsigned short>(source+0xa6)),klass,result,int(adopted),session->arena.begin(),session->world.read<float>(session->world.g(NativeMemory::Delta)));session->api->log(session->mod,OPENRAC_LOG_INFO,text);}}return result;
}
void observe_factory(FragmentFactory factory,U32 owner,U32 actor,int mode=1){if(session&&actor)try{session->sources.observe(factory,owner,actor,mode);}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}}
U32 flying(U32 owner,float speed,U32 position){const auto actor=flying_original(owner,speed,position);observe_factory(FragmentFactory::FlyingDebris81,owner,actor);return actor;}
U32 plasma(U32 owner,U32 position){const auto actor=plasma_original(owner,position);observe_factory(FragmentFactory::PlasmabotDebris325,owner,actor);return actor;}
U32 path(U32 owner,U32 position,int mode){const auto actor=path_original(owner,position,mode);observe_factory(FragmentFactory::PathDebris403,owner,actor,mode);return actor;}
U32 jet(U32 owner,U32 position,float speed){const auto actor=jet_original(owner,position,speed);observe_factory(FragmentFactory::BomberJetDebris1510,owner,actor);return actor;}
U32 cannon_record(U32 actor){return session&&session->cannon.handles_record(actor)?session->cannon.record(actor):cannon_record_original(actor);}
int cannon_grab(U32 actor,U32 muzzle,U32 cannon){return session&&session->cannon.handles_record(actor)?session->cannon.grab(actor,muzzle,cannon):cannon_grab_original(actor,muzzle,cannon);}
int cannon_claim(U32 cannon,U32 data,U32 actor,U32 record,U32 muzzle){return session&&session->cannon.handles_record(actor)?session->cannon.candidate(cannon,data,actor,record,muzzle):cannon_claim_original(cannon,data,actor,record,muzzle);}
int cannon_store(U32 actor,U32 muzzle){return session&&session->cannon.handles_record(actor)?session->cannon.store(actor,muzzle):cannon_store_original(actor,muzzle);}
int cannon_launch(U32 actor,U32 velocity,U32 target,float height){return session&&session->cannon.handles_record(actor)?session->cannon.launch(actor,velocity,target,height,cannon_launch_original):cannon_launch_original(actor,velocity,target,height);}
int cannon_cancel(U32 actor){return session&&session->cannon.handles_record(actor)?session->cannon.cancel(actor):cannon_cancel_original(actor);}
void cannon_finalize(U32 actor,U32 record,int explode){if(session&&session->cannon.handles_record(actor))session->cannon.finalize(actor,record,explode,cannon_finalize_original);else cannon_finalize_original(actor,record,explode);}
void prepare(int port,int slot){if(session)session->store.begin_save();prepare_original(port,slot);if(session)session->store.after_prepare();}
int autosave(int port,int slot){if(session)session->store.begin_save();const int result=autosave_original(port,slot);if(session)session->store.after_prepare();return result;}
void serialize(U32 at){if(session)session->store.begin_save();serialize_original(at);}
void io(){io_original();if(session)session->store.after_io();}
void restore(U32 at){restore_original(at);}
int restore_data(U32 source,int slot,U32 descriptors){const int result=restore_data_original(source,slot,descriptors);if(session&&result==0)session->store.after_restore_data(descriptors);return result;}
void new_game(){new_game_original();if(session)session->store.reset_campaign(session->ng_plus);}
void new_game_plus(int slot){if(!session){ng_original(slot);return;}const bool previous=session->ng_plus;session->ng_plus=true;session->store.begin_save();ng_original(slot);session->ng_plus=previous;session->store.after_prepare();}
void frame(void*){if(session)try{session->frame();}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}}
void pad(void*,int port,unsigned char* report,int size){if(session)try{session->pad(port,report,size);}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}}
void draw_callbacks(){if(session)try{session->presentation.queue_world_prompt();}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}draw_original();}
void hud_draw(){
 hud_draw_original();
 if(session)try{session->presentation.draw_hud_prompt();}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}
}
void pause_draw(int transfer){
 if(transfer){pause_draw_original(transfer);return;}
 auto* ui=session?&session->presentation:nullptr;
 struct Restore{NativePresentation* ui;~Restore(){if(ui&&session&&&session->presentation==ui)try{ui->after_pause_render();}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}}}restore{ui};
 if(ui)try{ui->before_pause_render();}catch(const std::exception& e){session->api->log(session->mod,OPENRAC_LOG_ERROR,e.what());}
 pause_draw_original(transfer);
}
}
extern "C" {OPENRAC_MOD_DEFINE("robot-salvage","0.1.0")}
extern "C" OPENRAC_MOD_EXPORT const robot_salvage_api_v1* openrac_robot_salvage_query(unsigned version){return salvage::ApiService::query(version);}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api,openrac_mod* mod){
 try{
  if(!api||!api->memory||!api->game||!api->version||!api->platform||std::strcmp(api->game,"rac1")||std::strcmp(api->version,"pal")||std::strcmp(api->platform,"windows-x86_64"))throw std::runtime_error("Robot Salvage currently requires official OpenRAC0.10 Windows x64 and R&C1 PAL Europe v2.00");
  if(session)throw std::runtime_error("Robot Salvage is already active");
  hooks=std::make_unique<NativeHooks>(api,mod);session=std::make_unique<Session>(api,mod,hooks->pause_api());
  session->presentation.use_native_hud_boundary(true);
  if(api->hook(mod,"rac1.level.load_gameplay","u32(i32)",reinterpret_cast<void*>(load),reinterpret_cast<void**>(&load_original))!=OPENRAC_HOOK_OK)throw std::runtime_error("Robot Salvage needs the native level-loader hook");
  hooks->bind("rac1.moby.spawn_break_fragment","u32(u32,i32,u32,u32,i32,i32,u32,u32,f32,u32)",reinterpret_cast<void*>(spawn),reinterpret_cast<void**>(&spawn_original));
  hooks->bind("rac1.render.dispatch_callbacks","void()",reinterpret_cast<void*>(draw_callbacks),reinterpret_cast<void**>(&draw_original));
  hooks->bind("rac1.menu.pause.render","void(i32)",reinterpret_cast<void*>(pause_draw),reinterpret_cast<void**>(&pause_draw_original));
  hooks->bind("rac1.hud.render","void()",reinterpret_cast<void*>(hud_draw),reinterpret_cast<void**>(&hud_draw_original));
  hooks->bind("rac1.moby.spawn_flying_debris","u32(u32,f32,u32)",reinterpret_cast<void*>(flying),reinterpret_cast<void**>(&flying_original));
  hooks->bind("rac1.moby.spawn_plasmabot_death_debris","u32(u32,u32)",reinterpret_cast<void*>(plasma),reinterpret_cast<void**>(&plasma_original));
  hooks->bind("rac1.moby.spawn_path_or_ballistic_debris","u32(u32,u32,i32)",reinterpret_cast<void*>(path),reinterpret_cast<void**>(&path_original));
  hooks->bind("rac1.moby.spawn_bomber_jet_debris","u32(u32,u32,f32)",reinterpret_cast<void*>(jet),reinterpret_cast<void**>(&jet_original));
  hooks->bind("rac1.suck_cannon.get_record","u32(u32)",reinterpret_cast<void*>(cannon_record),reinterpret_cast<void**>(&cannon_record_original));
  hooks->bind("rac1.suck_cannon.try_claim","i32(u32,u32,u32,u32,u32)",reinterpret_cast<void*>(cannon_claim),reinterpret_cast<void**>(&cannon_claim_original));
  hooks->bind("rac1.suck_cannon.grab","i32(u32,u32,u32)",reinterpret_cast<void*>(cannon_grab),reinterpret_cast<void**>(&cannon_grab_original));
  hooks->bind("rac1.suck_cannon.store","i32(u32,u32)",reinterpret_cast<void*>(cannon_store),reinterpret_cast<void**>(&cannon_store_original));
  hooks->bind("rac1.suck_cannon.launch","i32(u32,u32,u32,f32)",reinterpret_cast<void*>(cannon_launch),reinterpret_cast<void**>(&cannon_launch_original));
  hooks->bind("rac1.suck_cannon.cancel","i32(u32)",reinterpret_cast<void*>(cannon_cancel),reinterpret_cast<void**>(&cannon_cancel_original));
  hooks->bind("rac1.suck_cannon.finalize","void(u32,u32,i32)",reinterpret_cast<void*>(cannon_finalize),reinterpret_cast<void**>(&cannon_finalize_original));
  hooks->bind("rac1.save.prepare","void(i32,i32)",reinterpret_cast<void*>(prepare),reinterpret_cast<void**>(&prepare_original));
  hooks->bind("rac1.save.request_autosave","i32(i32,i32)",reinterpret_cast<void*>(autosave),reinterpret_cast<void**>(&autosave_original));
  hooks->bind("rac1.save.serialize","void(u32)",reinterpret_cast<void*>(serialize),reinterpret_cast<void**>(&serialize_original));
  hooks->bind("rac1.save.update_state","void()",reinterpret_cast<void*>(io),reinterpret_cast<void**>(&io_original));
  hooks->bind("rac1.save.restore","void(u32)",reinterpret_cast<void*>(restore),reinterpret_cast<void**>(&restore_original));
  hooks->bind("rac1.save.restore_data","i32(u32,i32,u32)",reinterpret_cast<void*>(restore_data),reinterpret_cast<void**>(&restore_data_original));
  hooks->bind("rac1.save.new_game","void()",reinterpret_cast<void*>(new_game),reinterpret_cast<void**>(&new_game_original));
  hooks->bind("rac1.save.new_game_plus","void(i32)",reinterpret_cast<void*>(new_game_plus),reinterpret_cast<void**>(&ng_original));
  if(api->on_frame(mod,frame,nullptr)||api->on_pad(mod,pad,nullptr))throw std::runtime_error("Robot Salvage could not register native callbacks");
  hooks->commit();api->log(mod,OPENRAC_LOG_INFO,"Recupero componenti0.1.0: persistent robot parts, solid physics, Triangle pickup, native inventory and paired PAL saves");return 0;
 }catch(const std::exception& e){session.reset();hooks.reset();if(api&&api->fail)api->fail(mod,e.what());return 1;}
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){session.reset();hooks.reset();}

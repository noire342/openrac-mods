// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_world.hpp"
#include "presentation.hpp"
#include "native_vendor.hpp"
#include "native_vendor_icons.hpp"
#include "native_class_alias.hpp"
#include "native_weapon_alias.hpp"
#include "native_weapon_menu.hpp"
#include "native_arsenal_save.hpp"
#include "native_arsenal_xp.hpp"
#include "native_runtime_hooks.hpp"
#include "native_asset_bundle.hpp"
#include "native_compatibility_modules.hpp"
#include "native_gold_vendor.hpp"
#include "native_acquisition.hpp"
#include "revolverator_api.h"
#include <memory>
#include <exception>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
#include <array>
#include <deque>
#include <cstddef>
namespace {
using namespace revolverator;
int grant(unsigned key,int equip);
struct Session {
 const openrac_api* api;openrac_mod* mod;GuestArena arena,core_arena;arsenal::Registry registry;
 NativeAmmunition ammunition;NativeAmmoAlias alias;NativeWeaponAlias weapon_alias;NativeVendor vendor;NativeVendorIcons vendor_icons;NativeRotor rotor,preview_rotor,scene_rotor;NativeWorld world;Controller controller;Presentation presentation;
 NativeArsenal arsenal;NativeWeaponMenu menu;NativeArsenalSave saves;NativeArsenalXP xp;
 NativeGoldVendor gold_vendor;NativeAcquisition acquisition;
 bool primary=false;unsigned epoch=0;int level=-1;
 bool golden()const{const auto* state=registry.state(arsenal::RevolveratorKey);return state&&state->owned&&state->golden;}
 unsigned grant_override=0,native_save_depth=0;bool new_game_plus=false;
 std::deque<NativeArsenalXP::LevelUp> animations,events;
 Session(const openrac_api* a,openrac_mod* m):api(a),mod(m),ammunition(a,&arena,true),alias(a),weapon_alias(a),vendor(a),vendor_icons(a,vendor),rotor(a,core_arena),preview_rotor(a,arena,true),scene_rotor(a,arena,true,true),world(a,m,ammunition,rotor,core_arena),controller(world),presentation(a,m,&arena,true),arsenal(a,registry,presentation),menu(a,arena,arsenal),saves(a,m,arsenal),xp(a,m,arsenal),gold_vendor(a,m,registry,weapon_alias),acquisition(a,m,arena,{this,
  +[](void* user){auto& s=*static_cast<Session*>(user);s.presentation.set_golden(s.golden());s.arsenal.project(arsenal::RevolveratorKey);},
  +[](void* user){static_cast<Session*>(user)->restore_context();},
  +[](void* user,unsigned actor){auto& s=*static_cast<Session*>(user);
   s.scene_rotor.update(actor,0,Phase::Idle,0,0,0,false,true);
   const bool applied=s.weapon_alias.apply_preview(actor,true,s.golden());
   if(!applied&&s.api->setting_bool(s.mod,"trace"))s.api->log(s.mod,OPENRAC_LOG_WARN,"Acquisition weapon is not ready for the Rotatore model");}}){
  registry.register_weapon({arsenal::RevolveratorKey,"noire342.revolverator",18,60037,NativeAmmunition::capacity,15000,12});vendor.set_arsenal(&arsenal);
 }
 ~Session(){before_load();}
 void before_load(){
  acquisition.reset();gold_vendor.reset();
  controller.cancel();rotor.reset(true);preview_rotor.reset(true);scene_rotor.reset(true);world.reset();menu.reset();weapon_alias.reset();arsenal.reset();presentation.reset();vendor.reset();alias.reset();ammunition.reset();core_arena.reset();arena.reset();xp.reset();animations.clear();events.clear();
 }
 void prepare_level(){
  arsenal.begin_level();ammunition.frame();alias.frame();weapon_alias.frame();vendor.set_preview_class(alias.ready()?NativeAmmoAlias::native_class:225);vendor.frame();
  presentation.select_identity(false);arsenal.finish_level();menu.frame();registry.seal();
  if(registry.descriptor(registry.equipped())){
   world.write<int>(world.global(0x141660),18);world.write<int>(world.hero()+0x20b8,18);
  }
  arsenal.project_quick_slots();
  // Weapon XP bootstraps the original item's name before our first frame.
  // Leave its source binding intact even when the restored logical item is Revo.
  presentation.select_identity(false);
 }
 void restore_context(){
  if(native_save_depth)arsenal.project(arsenal::NativeWalloperKey);
  else if(acquisition.active())arsenal.project(arsenal::RevolveratorKey);
  else if(world.read<int>(world.mode())==5)arsenal.project(vendor.selected_key());
  else arsenal.restore_gameplay();
 }
 void reset_progression(){xp.reset(true);animations.clear();events.clear();}
 void unlock_starting_weapon(){
  if(!api->setting_bool(mod,"start_unlocked")||registry.owned(arsenal::RevolveratorKey))return;
  const auto state=world.snapshot(false,epoch);const auto body=world.body();
  if(!arsenal.ready()||!state.playing||state.paused||!state.alive||!state.grounded||
     !Memory::valid(body,256)||world.read<unsigned short>(body+0xa6)!=0)return;
  // An optional starting item has no acquisition cutscene or stock story
  // caption. Reuse the native grant without borrowing Walloper's message IDs.
  const auto messages=world.level(0x1b0340);
  const unsigned addresses[]{messages+18*4,messages+(18+37)*4};
  int before[2]{};
  for(unsigned i=0;i<2;++i){before[i]=world.read<int>(addresses[i]);world.write<int>(addresses[i],-1);}
  const bool given=grant(arsenal::RevolveratorKey,1)!=0;
  for(unsigned i=0;i<2;++i)if(world.read<int>(addresses[i])==-1)world.write<int>(addresses[i],before[i]);
  if(given)api->log(mod,OPENRAC_LOG_INFO,"Revolverator starting unlock: granted20 rounds once; owned weapons are never refilled");
 }
 void progression(){
  xp.frame();NativeArsenalXP::LevelUp event;
  while(xp.poll_level_up(event)){
   if(animations.size()<64)animations.push_back(event);
   if(events.size()==64)events.pop_front();
   events.push_back(event);
  }
  const auto stats=xp.get_stats();
  presentation.set_experience(xp.ready()&&(xp.flags()&WEAPON_XP_SHOW_HUD),stats.level,stats.progress,
   xp.ready()&&(xp.flags()&WEAPON_XP_VERSIONED_NAMES));
  world.set_damage_multiplier(stats.damage_multiplier);
  if(!animations.empty()){
   if(!(xp.flags()&WEAPON_XP_LEVEL_UP_ANIMATION)||acquisition.request())animations.pop_front();
  }
  restore_context();
 }
 void frame(){
  const int current=api->loaded_level();
  if(current!=level){before_load();level=current;++epoch;primary=false;}
  if(current<0||current>18)return;
  if(!arsenal.ready())prepare_level();
  weapon_alias.frame();arsenal.commit();arsenal.observe_equipped();restore_context();unlock_starting_weapon();progression();
  gold_vendor.frame();
  if(gold_vendor.poll_purchase()){
   acquisition.request();
   api->log(mod,OPENRAC_LOG_INFO,"Golden Revolverator purchased:50000 normal bolts,0 gold bolts; ammunition and XP preserved");
  }
  if(world.read<int>(world.mode())!=3)preview_rotor.reset();
  if(world.read<int>(world.mode())!=5)scene_rotor.reset();
  ammunition.frame();alias.frame();vendor.set_preview_class(alias.ready()?NativeAmmoAlias::native_class:225);vendor.frame();menu.frame();
  world.set_active(arsenal.revolverator_active());
  world.set_golden(golden());presentation.set_golden(golden());
  auto f=world.snapshot(primary,epoch);
  const bool safe=f.playing&&!f.paused&&f.alive&&f.grounded&&controller.attack_phase()==Phase::Idle&&
   !controller.flight_count()&&!world.release_recoil_active();
  acquisition.frame(safe,xp.acquisition_presentation_active()>0);
  if(acquisition.active()){primary=false;f=world.snapshot(false,epoch);}
  world.set_drill_head_scale(rotor.controller().radial_scale(),rotor.controller().axial_scale());
  controller.tick(f);
  if(controller.attack_phase()!=Phase::Spinning&&!f.paused)world.pose(controller.attack_phase(),controller.spin_angle());
  if(f.equipped)rotor.update(world.weapon(),ammunition.count(),controller.attack_phase(),controller.charge(),controller.spin_angle(),f.seconds,f.playing,f.paused);
  else rotor.reset();
  world.set_drill_head_scale(rotor.controller().radial_scale(),rotor.controller().axial_scale());
  arsenal.commit();
  presentation.frame(f.equipped);
 }
};
std::unique_ptr<NativeAssetBundle> weapon_assets;
std::unique_ptr<NativeCompatibilityModules> compatibility_modules;
std::unique_ptr<NativeRuntimeHooks> runtime_hooks;
std::unique_ptr<Session> session;
using U32=unsigned;
using Update=void(*)(U32);
using Pad=void(*)(U32,U32,int);
using Load=U32(*)(int);
using GetHit=U32(*)(U32,int,int);
using Death=void(*)(U32,int,int);
using Advance=void(*)();
using BuildVendor=void(*)(int);
using MenuCall=int(*)(unsigned);
using SignedMenuCall=int(*)(int);
using Give=void(*)(int,int);
using ChooseAmmo=int(*)(unsigned,unsigned);
using SpawnAmmo=unsigned(*)(unsigned,int,int,int);
using PrepareSave=void(*)(int,int);
using AutoSave=int(*)(int,int);
using RestoreData=int(*)(unsigned,int,unsigned);
Update walloper_original{},relay_original{},ammo_original{};Pad pad_original{};Load load_original{};
Update gold_offer_original{};
Advance motion_input_original{};void(*planar_velocity_original)(float){};
GetHit hit_original{};Death death_original{};Advance animation_original{};
BuildVendor vendor_original{};
Give give_original{};Advance vendor_update_original{},vendor_selected_original{},vendor_icons_original{},save_io_original{},new_game_original{};
ChooseAmmo choose_original{};SpawnAmmo spawn_original{};Update quick_init_original{},quick_update_original{},preview_pose_original{},serialize_original{},restore_original{};
MenuCall quick_draw_original{};PrepareSave prepare_original{};AutoSave autosave_original{};RestoreData restore_data_original{};void(*new_game_plus_original)(int){};
SignedMenuCall item_ammo_original{};
MenuCall available_draw_original{},selected_draw_original{};
template<int Index>struct EnemyHook{
 static inline Update original{};
 static void update(U32 moby){if(!session||!session->world.frozen(moby))original(moby);}
};
void hook(const char* id,const char* signature,void* replacement,void** original){
 const int r=runtime_hooks->bind(id,signature,replacement,original);
 if(r!=OPENRAC_HOOK_OK)throw std::runtime_error(std::string("Revolverator hook ")+id+" unavailable or incompatible ("+std::to_string(r)+"): "+runtime_hooks->diagnostic());
}
template<int Index>void enemy(const char* id){hook(id,"void(u32)",reinterpret_cast<void*>(&EnemyHook<Index>::update),reinterpret_cast<void**>(&EnemyHook<Index>::original));}
template<int Index,int(NativeWeaponMenu::*Method)(unsigned,NativeWeaponMenu::Call)>struct MenuHook {
 static inline MenuCall original{};
 static int call(unsigned screen){return session?(session->menu.*Method)(screen,original):original(screen);}
 static void install(const char* id){hook(id,"i32(u32)",reinterpret_cast<void*>(&call),reinterpret_cast<void**>(&original));}
};
// Native save packets only contain original item IDs. Both the save and the
// RAM restoration are scoped; the native save file is never patched afterward.
class NativeSaveScope {
 Session& owner_;arsenal::WeaponKey previous_;
 std::array<int,8> quick_{};std::array<int,7> equipment_{};
public:
 explicit NativeSaveScope(Session& owner):owner_(owner),previous_(owner.arsenal.projected()){
  ++owner_.native_save_depth;
  auto& w=owner_.world;
  quick_=w.read<decltype(quick_)>(w.global(0x141ea0));
  equipment_=w.read<decltype(equipment_)>(w.global(0x141660));
  owner_.arsenal.project(arsenal::NativeWalloperKey);owner_.arsenal.project_quick_slots(true);
  if(owner_.registry.descriptor(owner_.registry.equipped()))w.write<int>(w.global(0x141660),int(arsenal::NativeWrenchKey));
 }
 ~NativeSaveScope(){
  auto& w=owner_.world;w.write(w.global(0x141ea0),quick_);w.write(w.global(0x141660),equipment_);
  --owner_.native_save_depth;
  owner_.arsenal.project(previous_);
 }
};
void canonicalize_weapon(unsigned moby){
 if(session&&session->weapon_alias.owns(moby))session->world.write<std::uint16_t>(moby+0xa6,NativeWeaponAlias::source_class);
}
void motion_input(){motion_input_original();if(session)session->world.enforce_release_recoil();}
void planar_velocity(float angle){planar_velocity_original(angle);if(session)session->world.enforce_release_recoil();}
void gold_offer(unsigned moby){if(!session||!session->gold_vendor.update(moby))gold_offer_original(moby);}
void pad(U32 address,U32 report,int size){
 if(session&&address==session->world.global(0x13c940)&&size>0&&Memory::valid(report,unsigned(size)))
  session->acquisition.filter_report(session->api->memory+report,size);
 pad_original(address,report,size);
 if(!session||address!=session->world.global(0x13c940))return;
 auto& w=session->world;
 w.set_active(session->arsenal.revolverator_active());
 const auto f=w.snapshot(false,session->epoch);
 if(!f.equipped||!f.playing||session->acquisition.active()){session->primary=false;return;}
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
 if(!session){walloper_original(moby);return;}
 auto& s=*session;canonicalize_weapon(moby);
 const bool held=moby==s.world.weapon();
 const bool custom=held?s.arsenal.revolverator_active():
  s.arsenal.projected()==arsenal::RevolveratorKey&&
   (s.world.read<int>(s.world.mode())==5||s.acquisition.presenting());
 if(!custom){
  if(held)s.weapon_alias.apply(moby,false);else s.weapon_alias.apply_preview(moby,false);
  walloper_original(moby);return;
 }
 if(held){
  if(s.controller.attack_phase()==Phase::Spinning)s.world.align_weapon(s.controller.spin_angle());
  else s.world.align_release();
  s.rotor.apply();const bool applied=s.weapon_alias.apply(moby,true,s.golden());
  static unsigned traced=0;if(traced<8&&s.api->setting_bool(s.mod,"trace")){
   char text[160];std::snprintf(text,sizeof text,"weapon render alias held=%08x applied=%d class=%u ready=%d",moby,int(applied),unsigned(s.world.read<unsigned short>(moby+0xa6)),int(s.weapon_alias.ready()));s.api->log(s.mod,OPENRAC_LOG_INFO,text);++traced;
  }
 }else{
  s.scene_rotor.update(moby,0,Phase::Idle,0,0,0,false,true);
  s.weapon_alias.apply_preview(moby,true,s.golden());
 }
}
void ammo_pickup(U32 moby){
 if(!session){ammo_original(moby);return;}
 auto& s=*session;const auto vars=s.world.read<unsigned>(moby+0x78);
 const bool custom=Memory::valid(vars,48)&&s.world.read<int>(vars+8)==18;
 if(!custom){ammo_original(moby);return;}
 {ArsenalScope view(s.arsenal,arsenal::RevolveratorKey);
  s.ammunition.before_update(moby);ammo_original(moby);s.ammunition.after_update(moby);s.arsenal.commit();}
 s.restore_context();
}
void build_vendor(int skip){
 if(session)session->vendor.before_build();
 vendor_original(skip);
 if(session)session->vendor.after_build(skip);
}
U32 load(int full){
 if(session)session->before_load();
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
  session->level=session->api->loaded_level();++session->epoch;session->primary=false;session->prepare_level();
 }
 return end;
}
U32 get_hit(U32 moby,int mask,int keep){
 if(session)session->xp.before_hit();
 const auto hit=hit_original(moby,mask,keep);
 if(session&&session->arsenal.revolverator_active()&&capturable_class(int(session->world.token(moby).klass))&&session->controller.attack_phase()!=Phase::Idle&&session->world.blocks_native_hit(hit)){
  session->xp.block_hit(moby);if(keep)hit_original(moby,mask,0);return 0;
 }
 if(session&&hit)session->xp.confirm_hit(moby,hit);
 return hit;
}
void death(U32 moby,int flags,int index){
 if(session){session->xp.death_before(moby);if(session->world.frozen(moby))session->controller.discard(session->world.token(moby));}
 death_original(moby,flags,index);
}
void animation(){if(session)session->restore_context();animation_original();if(session)session->presentation.names();}
void vendor_update(){
 if(!session){vendor_update_original();return;}
 auto& s=*session;s.vendor.before_vendor_update();s.arsenal.project(s.vendor.selected_key());
 vendor_update_original();s.arsenal.commit();s.restore_context();s.vendor.after_vendor_update();
}
void vendor_selected(){
 if(!session){vendor_selected_original();return;}
 auto& s=*session;s.vendor.before_vendor_update();s.arsenal.project(s.vendor.selected_key());
 vendor_selected_original();s.arsenal.commit();s.restore_context();s.vendor.after_vendor_update();
}
void vendor_icon_strip(){
 if(!session||!session->vendor_icons.draw())vendor_icons_original();
}
void give(int item,int equip){
 if(!session||!session->arsenal.ready()||item<0||item>int(arsenal::Registry::MaximumNativeKey)){
  give_original(item,equip);return;
 }
 auto& s=*session;auto& w=s.world;
 const bool custom=item==18&&(s.grant_override==arsenal::RevolveratorKey||
  (w.read<int>(w.mode())==5&&s.vendor.selected_custom()));
 const auto key=custom?arsenal::RevolveratorKey:unsigned(item);
 const auto before=w.read<std::array<int,8>>(w.global(0x141ea0));
 const auto stock=w.read<std::array<unsigned char,12>>(w.global(0x15edd0));
 s.arsenal.project(key);give_original(item,equip);s.arsenal.commit();
 if(custom){
  const auto after=w.read<std::array<unsigned char,12>>(w.global(0x15edd0));
  for(unsigned i=0;i<after.size();++i)if(after[i]!=stock[i]&&(after[i]&0x3f)==18)
   w.write<unsigned char>(w.global(0x15edd0)+i,stock[i]);
 }
 const auto after=w.read<std::array<int,8>>(w.global(0x141ea0));
 for(unsigned i=0;i<after.size();++i)if(after[i]!=before[i]){
  if(after[i]==item)s.registry.assign_quick(i,key);
  else if(after[i]>=0&&after[i]<=int(arsenal::Registry::MaximumNativeKey))s.registry.assign_quick(i,unsigned(after[i]));
 }
 if(custom&&s.registry.owned(key)&&std::find(s.registry.quick().begin(),s.registry.quick().end(),key)==s.registry.quick().end()){
  const auto empty=std::find(s.registry.quick().begin(),s.registry.quick().end(),0u);
  if(empty!=s.registry.quick().end())s.registry.assign_quick(unsigned(empty-s.registry.quick().begin()),key);
 }
 if(equip&&s.registry.equip(key)){
  // Native Give only updates pending equipment on the first purchase.
  w.write<int>(w.hero()+0x20b8,item);
 }
 s.arsenal.project_quick_slots();s.restore_context();w.set_active(s.arsenal.revolverator_active());
}
int choose_ammo(unsigned moby,unsigned out){
 if(!session||!session->arsenal.ready()||!session->registry.owned(arsenal::RevolveratorKey))return choose_original(moby,out);
 auto& s=*session;auto& w=s.world;int result;
 const auto address=w.global(0x15edd0);const auto before=w.read<std::array<unsigned char,12>>(address);auto stock=before;
 bool found=false;unsigned empty=unsigned(stock.size()),taunter=unsigned(stock.size());
 for(unsigned i=0;i<stock.size();++i){
  if(stock[i]==0xff){empty=i;break;}
  if((stock[i]&0x3f)==18)found=true;
  if((stock[i]&0x3f)==14)taunter=i;
 }
 if(!found){
  const unsigned slot=empty<stock.size()?empty:taunter;
  if(slot<stock.size())stock[slot]=18;
 }
 {ArsenalScope view(s.arsenal,arsenal::RevolveratorKey);w.write(address,stock);result=choose_original(moby,out);w.write(address,before);}
 s.restore_context();return result;
}
unsigned spawn_ammo(unsigned position,int item,int amount,int flags){
 if(!session||!session->arsenal.ready()||item!=18)return spawn_original(position,item,amount,flags);
 auto& s=*session;unsigned result;
 {ArsenalScope view(s.arsenal,arsenal::RevolveratorKey);result=spawn_original(position,item,amount,flags);}
 s.restore_context();return result;
}
void preview_pose(unsigned moby){
 canonicalize_weapon(moby);preview_pose_original(moby);
 if(session){
  const bool custom=session->arsenal.projected()==arsenal::RevolveratorKey;
  const bool applied=session->weapon_alias.apply_preview(moby,custom,session->golden());
  static unsigned traced=0;if(custom&&traced<8&&session->api->setting_bool(session->mod,"trace")){
   char text[160];std::snprintf(text,sizeof text,"weapon preview alias moby=%08x applied=%d class=%u ready=%d",moby,int(applied),unsigned(session->world.read<unsigned short>(moby+0xa6)),int(session->weapon_alias.ready()));session->api->log(session->mod,OPENRAC_LOG_INFO,text);++traced;
  }
 }
}
template<MenuCall* Original>int draw_selected_preview(unsigned screen){
 if(!session)return (*Original)(screen);
 auto& s=*session;const auto key=s.menu.selected_key();
 ArsenalScope view(s.arsenal,key?key:s.registry.equipped());
 const bool custom=key==arsenal::RevolveratorKey;
 if(!custom)s.preview_rotor.reset();
 if(Memory::valid(screen,0x4c))for(unsigned offset:{0x44u,0x48u}){
  const auto moby=s.world.read<unsigned>(screen+offset);
  if(!Memory::valid(moby,256))continue;
  // The isolated rotating model has no Ratchet hand pose. Neutralize its
  // Walloper finger skeleton just as the held drill does, keeping the head
  // attached to the housing. The actual held actor uses its separate rotor.
  if(custom&&(s.world.read<unsigned short>(moby+0xa6)==180||s.weapon_alias.owns(moby)))
   s.preview_rotor.update(moby,0,Phase::Idle,0,0,0,false,true);
  if(s.weapon_alias.apply_preview(moby,custom,s.golden())&&custom)
   s.weapon_alias.fit_preview(moby,s.world.read<int>(s.world.global(0x1d5bf0))==5?0.65f:1.f);
  s.alias.apply_preview(moby,custom);
 }
 return (*Original)(screen);
}
int item_ammo(int screen){
 if(!session)return item_ammo_original(screen);
 auto& s=*session;const auto key=s.menu.selected_key();
 if(!key)return item_ammo_original(screen);
 ArsenalScope view(s.arsenal,key);return item_ammo_original(screen);
}
void quick_init(unsigned element){
 if(session){session->arsenal.project_quick_slots();ArsenalScope stock(session->arsenal,arsenal::NativeWalloperKey);quick_init_original(element);}
 else quick_init_original(element);
 if(!session)return;
 auto& s=*session;auto& w=s.world;
 const auto records=w.level(0x17dac0);
 if(!Memory::valid(records,8*0x1c))return;
 for(unsigned i=0;i<8;++i)if(const auto* descriptor=s.registry.descriptor(s.registry.quick()[i])){
  if(w.read<int>(records+i*0x1c+0x18)==int(descriptor->carrier))w.write<int>(records+i*0x1c,int(descriptor->icon_bank));
 }
}
arsenal::WeaponKey selected_quick(unsigned element){
 if(!session||!Memory::valid(element,0x80))return 0;
 const int slot=session->world.read<int>(element+0x74);
 return slot>=0&&slot<8?session->registry.quick()[unsigned(slot)]:0;
}
void quick_update(unsigned element){
 if(!session){quick_update_original(element);return;}
 auto& s=*session;const auto key=selected_quick(element);
 if(key)s.arsenal.project(key);
 quick_update_original(element);s.arsenal.commit();
 if(!(s.world.read<unsigned>(s.world.global(0x13c940)+0x1c0)&0x10)){
  const auto chosen=selected_quick(element);
  if(chosen)s.registry.equip(chosen);
 }
 s.restore_context();s.world.set_active(s.arsenal.revolverator_active());
}
int quick_draw(unsigned element){
 if(!session)return quick_draw_original(element);
 auto& s=*session;const auto key=selected_quick(element);int result;
 {ArsenalScope view(s.arsenal,key?key:s.registry.equipped());
  result=s.presentation.draw_quick_selector(element,quick_draw_original,s.registry.quick());}
 s.restore_context();return result;
}
void serialize(unsigned destination){
 if(!session||!session->arsenal.ready()){serialize_original(destination);return;}
 auto& s=*session;s.saves.begin_save();{NativeSaveScope vanilla(s);serialize_original(destination);}
 s.restore_context();
}
void prepare_save(int port,int slot){
 if(!session||!session->arsenal.ready()){prepare_original(port,slot);return;}
 auto& s=*session;s.saves.begin_save();{NativeSaveScope vanilla(s);prepare_original(port,slot);}
 s.saves.after_prepare();s.restore_context();
}
int autosave(int port,int slot){
 if(!session||!session->arsenal.ready())return autosave_original(port,slot);
 auto& s=*session;s.saves.begin_save();int result;
 {NativeSaveScope vanilla(s);result=autosave_original(port,slot);}
 s.saves.after_prepare();s.restore_context();return result;
}
void save_io(){save_io_original();if(session)session->saves.after_io();}
void before_restore(){
 if(!session)return;
 auto& s=*session;s.controller.cancel();s.rotor.reset(true);s.weapon_alias.frame();s.world.reset();s.primary=false;
 s.arsenal.project(arsenal::NativeWalloperKey);
}
void restore(unsigned source){
 if(!session){restore_original(source);return;}
 before_restore();restore_original(source);session->restore_context();
}
int restore_data(unsigned source,int slot,unsigned descriptors){
 if(!session)return restore_data_original(source,slot,descriptors);
 auto& s=*session;before_restore();const int result=restore_data_original(source,slot,descriptors);
 if(result==0&&descriptors==s.world.global(0x1a04c0)){
  s.saves.import_native_state();s.saves.after_restore_data(descriptors);s.reset_progression();
 }
 s.restore_context();return result;
}
void new_game(){
 if(!session){new_game_original();return;}
 auto& s=*session;before_restore();new_game_original();s.arsenal.project(arsenal::NativeWalloperKey);s.saves.reset_campaign(s.new_game_plus);
 if(!s.new_game_plus)s.reset_progression();
 s.restore_context();
}
void new_game_plus(int slot){
 if(!session){new_game_plus_original(slot);return;}
 auto& s=*session;const bool previous=s.new_game_plus;s.new_game_plus=true;s.saves.begin_save();
 {NativeSaveScope vanilla(s);new_game_plus_original(slot);s.saves.import_native_state();}
 s.new_game_plus=previous;s.saves.after_prepare();s.arsenal.project_quick_slots();s.restore_context();
 if(s.registry.descriptor(s.registry.equipped()))s.world.write<int>(s.world.hero()+0x20b8,18);
}
int get_state(revolverator_state_v1* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 const auto& c=session->controller;const auto& counts=c.counters();
 *out={sizeof(*out),unsigned(c.phase()),18,c.target().klass,c.charge(),counts.captures,counts.throws,counts.impacts};return 1;
}
int cancel(){if(!session)return 0;session->controller.cancel();return 1;}
int get_mechanics(revolverator_mechanics_v1* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 const auto& motor=session->rotor.controller();
 session->arsenal.commit();const auto* state=session->registry.state(arsenal::RevolveratorKey);
 *out={sizeof(*out),state?state->ammo:0,NativeAmmunition::capacity,motor.angle(),motor.speed(),unsigned(motor.dry_firing()),unsigned(motor.synchronized())};return 1;
}
int get_inventory(unsigned key,revolverator_inventory_v2* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 auto& s=*session;s.arsenal.commit();
 revolverator_inventory_v2 result{};result.size=sizeof(result);result.key=key;
 if(const auto* descriptor=s.registry.descriptor(key)){
  const auto* state=s.registry.state(key);if(!state)return 0;
  result.native_item_id=descriptor->carrier;result.owned=state->owned;result.ammunition=state->ammo;
  result.capacity=descriptor->capacity;result.xp=state->xp;result.icon_bank=descriptor->icon_bank;
  result.normal_price=descriptor->price;result.first_level=descriptor->first_level;
 }else if(key<=arsenal::Registry::MaximumNativeKey){
  auto& w=s.world;result.native_item_id=key;
  if(key==arsenal::NativeWalloperKey){
   result.owned=s.registry.walloper().owned;result.ammunition=unsigned(std::max(0,s.registry.walloper().ammo));
   // The original Walloper's native metadata is visible only within this view.
   ArsenalScope original(s.arsenal,arsenal::NativeWalloperKey);
   result.capacity=w.read<unsigned short>(w.level(0x1c40b0)+key*24+14);
   result.icon_bank=w.read<unsigned short>(w.level(0x179ac0)+key*76+0x38);
   result.normal_price=unsigned(std::max(0,w.read<int>(w.level(0x1c40b0)+key*24)));
  }else{
   result.owned=key==arsenal::NativeWrenchKey||w.read<unsigned char>(w.global(0x13d4c0)+key)!=0;
   result.ammunition=unsigned(std::max(0,w.read<int>(w.global(0x13d428)+key*4)));
   result.capacity=w.read<unsigned short>(w.level(0x1c40b0)+key*24+14);
   result.icon_bank=w.read<unsigned short>(w.level(0x179ac0)+key*76+0x38);
   result.normal_price=unsigned(std::max(0,w.read<int>(w.level(0x1c40b0)+key*24)));
  }
 }else return 0;
 *out=result;return 1;
}
int get_quick(revolverator_quick_v2* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 revolverator_quick_v2 result{};result.size=sizeof(result);result.count=8;
 std::copy(session->registry.quick().begin(),session->registry.quick().end(),result.keys);*out=result;return 1;
}
int grant(unsigned key,int equip){
 if(!session||!session->arsenal.ready()||key!=arsenal::RevolveratorKey||!give_original)return 0;
 auto& s=*session;const auto previous=s.grant_override;s.grant_override=key;
 give(18,equip!=0);s.grant_override=previous;
 return s.registry.owned(key)?1:0;
}
int get_progress(revolverator_progress_v2* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 const auto stats=session->xp.get_stats();
 *out={sizeof(*out),stats.level,stats.xp,stats.level_start_xp,stats.next_level_xp,
  stats.progress,stats.damage_multiplier,stats.ammo_multiplier};return 1;
}
int poll_level_up(revolverator_level_up_v2* out){
 if(!session||!out||out->size<sizeof(*out)||session->events.empty())return 0;
 const auto event=session->events.front();session->events.pop_front();
 *out={sizeof(*out),event.old_level,event.new_level,event.xp};return 1;
}
const revolverator_api_v1 service{sizeof(service),1,get_state,cancel,get_mechanics};
const revolverator_api_v2 service_v2{sizeof(service_v2),2,get_state,cancel,get_mechanics,get_inventory,get_quick,grant,get_progress,poll_level_up};
int get_upgrade(revolverator_upgrade_v3* out){
 if(!session||!out||out->size<sizeof(*out))return 0;
 auto& s=*session;s.arsenal.commit();
 *out={sizeof(*out),unsigned(s.golden()),NativeGoldVendor::price,0,s.controller.blast_charge(),
       unsigned(s.world.release_recoil_active()),s.world.golden_explosions(),s.world.golden_hits()};return 1;
}
const revolverator_api_v3 service_v3{sizeof(service_v3),3,get_state,cancel,get_mechanics,get_inventory,get_quick,grant,get_progress,poll_level_up,get_upgrade};
static_assert(offsetof(revolverator_api_v2,get_inventory)==sizeof(revolverator_api_v1),"v2 preserves the published v1 prefix");
static_assert(offsetof(revolverator_api_v3,get_upgrade)==sizeof(revolverator_api_v2),"v3 preserves the published v2 prefix");
}
extern "C" { OPENRAC_MOD_DEFINE("revolverator","0.3.1") }
extern "C" OPENRAC_MOD_EXPORT const revolverator_api_v1* openrac_revolverator_query(unsigned version){
 if(!session)return nullptr;
 if(version==1)return &service;
 if(version==2)return reinterpret_cast<const revolverator_api_v1*>(&service_v2);
 return version==3?reinterpret_cast<const revolverator_api_v1*>(&service_v3):nullptr;
}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api,openrac_mod* mod){
 try{
  if(!api||!api->memory||!api->game||std::strcmp(api->game,"rac1")||!RegionBindings::for_version(api->version))throw std::runtime_error("Revolverator requires Ratchet & Clank PAL Europe v2.00 or NTSC-U and matching Arsenal Compatibility");
  weapon_assets=std::make_unique<NativeAssetBundle>(api->api_minor>=10?4u:3u,api->version);
  compatibility_modules=std::make_unique<NativeCompatibilityModules>(api,mod,weapon_assets->configuration());
  runtime_hooks=std::make_unique<NativeRuntimeHooks>(api,mod);
  session=std::make_unique<Session>(api,mod);
  hook("rac1.level.load_gameplay","u32(i32)",reinterpret_cast<void*>(load),reinterpret_cast<void**>(&load_original));
  hook("rac1.pad.process_report","void(u32,u32,i32)",reinterpret_cast<void*>(pad),reinterpret_cast<void**>(&pad_original));
  hook("rac1.player.update_motion_input","void()",reinterpret_cast<void*>(motion_input),reinterpret_cast<void**>(&motion_input_original));
  hook("rac1.player.set_planar_velocity","void(f32)",reinterpret_cast<void*>(planar_velocity),reinterpret_cast<void**>(&planar_velocity_original));
  hook("rac1.moby.moby180.update","void(u32)",reinterpret_cast<void*>(walloper),reinterpret_cast<void**>(&walloper_original));
  hook("rac1.moby.ammo_pickup.update","void(u32)",reinterpret_cast<void*>(ammo_pickup),reinterpret_cast<void**>(&ammo_original));
  hook(NativeGoldVendor::hook,NativeGoldVendor::signature,reinterpret_cast<void*>(gold_offer),reinterpret_cast<void**>(&gold_offer_original));
  hook("rac1.item.give","void(i32,i32)",reinterpret_cast<void*>(give),reinterpret_cast<void**>(&give_original));
  hook(NativeVendor::build_hook,NativeVendor::build_signature,reinterpret_cast<void*>(build_vendor),reinterpret_cast<void**>(&vendor_original));
  hook("rac1.vendor.menu_update","void()",reinterpret_cast<void*>(vendor_update),reinterpret_cast<void**>(&vendor_update_original));
  hook("rac1.vendor.initialize_selected","void()",reinterpret_cast<void*>(vendor_selected),reinterpret_cast<void**>(&vendor_selected_original));
  hook("rac1.vendor.icon_strip.draw","void()",reinterpret_cast<void*>(vendor_icon_strip),reinterpret_cast<void**>(&vendor_icons_original));
  hook("rac1.ammo.choose_drop","i32(u32,u32)",reinterpret_cast<void*>(choose_ammo),reinterpret_cast<void**>(&choose_original));
  hook("rac1.ammo.spawn_drop","u32(u32,i32,i32,i32)",reinterpret_cast<void*>(spawn_ammo),reinterpret_cast<void**>(&spawn_original));
  MenuHook<0,&NativeWeaponMenu::grid_update>::install("rac1.menu.item_grid.update");
  MenuHook<1,&NativeWeaponMenu::grid_draw>::install("rac1.menu.item_grid.draw");
  MenuHook<2,&NativeWeaponMenu::slots_update>::install("rac1.menu.item_slots.update");
  MenuHook<3,&NativeWeaponMenu::quick_draw>::install("rac1.menu.quick_select.draw");
  MenuHook<4,&NativeWeaponMenu::quick_load>::install("rac1.menu.quick_select.load");
  MenuHook<5,&NativeWeaponMenu::quick_store>::install("rac1.menu.quick_select.store");
  MenuHook<6,&NativeWeaponMenu::selected_call>::install("rac1.menu.item_text.draw");
  hook("rac1.menu.item_ammo.draw","i32(i32)",reinterpret_cast<void*>(item_ammo),reinterpret_cast<void**>(&item_ammo_original));
  MenuHook<7,&NativeWeaponMenu::selected_call>::install("rac1.menu.preview.bind");
  MenuHook<8,&NativeWeaponMenu::selected_call>::install("rac1.menu.preview.update");
  hook("rac1.menu.preview.draw_available","i32(u32)",reinterpret_cast<void*>(&draw_selected_preview<&available_draw_original>),reinterpret_cast<void**>(&available_draw_original));
  hook("rac1.menu.preview.draw_selected","i32(u32)",reinterpret_cast<void*>(&draw_selected_preview<&selected_draw_original>),reinterpret_cast<void**>(&selected_draw_original));
  MenuHook<11,&NativeWeaponMenu::equipped_call>::install("rac1.menu.equipped_preview.update");
  MenuHook<12,&NativeWeaponMenu::equipped_call>::install("rac1.menu.equipped_preview.draw");
  hook("rac1.menu.preview.pose","void(u32)",reinterpret_cast<void*>(preview_pose),reinterpret_cast<void**>(&preview_pose_original));
  hook("rac1.hud.quick_select.init","void(u32)",reinterpret_cast<void*>(quick_init),reinterpret_cast<void**>(&quick_init_original));
  hook("rac1.hud.quick_select.update","void(u32)",reinterpret_cast<void*>(quick_update),reinterpret_cast<void**>(&quick_update_original));
  hook("rac1.hud.quick_select.draw","i32(u32)",reinterpret_cast<void*>(quick_draw),reinterpret_cast<void**>(&quick_draw_original));
  hook("rac1.save.prepare","void(i32,i32)",reinterpret_cast<void*>(prepare_save),reinterpret_cast<void**>(&prepare_original));
  hook("rac1.save.request_autosave","i32(i32,i32)",reinterpret_cast<void*>(autosave),reinterpret_cast<void**>(&autosave_original));
  hook("rac1.save.restore","void(u32)",reinterpret_cast<void*>(restore),reinterpret_cast<void**>(&restore_original));
  hook("rac1.save.restore_data","i32(u32,i32,u32)",reinterpret_cast<void*>(restore_data),reinterpret_cast<void**>(&restore_data_original));
  hook("rac1.save.new_game","void()",reinterpret_cast<void*>(new_game),reinterpret_cast<void**>(&new_game_original));
  hook("rac1.save.new_game_plus","void(i32)",reinterpret_cast<void*>(new_game_plus),reinterpret_cast<void**>(&new_game_plus_original));
  hook("rac1.save.serialize","void(u32)",reinterpret_cast<void*>(serialize),reinterpret_cast<void**>(&serialize_original));
  hook("rac1.save.update_state","void()",reinterpret_cast<void*>(save_io),reinterpret_cast<void**>(&save_io_original));
  hook("rac1.moby.bolt.update","void(u32)",reinterpret_cast<void*>(relay),reinterpret_cast<void**>(&relay_original));
  hook("rac1.combat.get_hit","u32(u32,i32,i32)",reinterpret_cast<void*>(get_hit),reinterpret_cast<void**>(&hit_original));
  hook("rac1.moby.record_death","void(u32,i32,i32)",reinterpret_cast<void*>(death),reinterpret_cast<void**>(&death_original));
  hook("rac1.player.advance_animation","void()",reinterpret_cast<void*>(animation),reinterpret_cast<void**>(&animation_original));
  enemy<0>("rac1.moby.birdbot.update");enemy<1>("rac1.moby.sand_shark.update");enemy<2>("rac1.moby.wb_moby.update");
  enemy<3>("rac1.moby.scoutbot.update");enemy<4>("rac1.moby.spiderbot.update");enemy<5>("rac1.moby.blarg_trooper.update");
  enemy<6>("rac1.moby.moby294.update");enemy<7>("rac1.moby.moby573.update");enemy<8>("rac1.moby.moby1382.update");
  enemy<9>("rac1.moby.peckbot.update");enemy<10>("rac1.moby.extermibot.update");enemy<11>("rac1.moby.moby1048.update");
  enemy<12>("rac1.moby.sharkigator_35.update");enemy<13>("rac1.moby.sharkigator_1059.update");
  session->xp.frame();
  if(!session->xp.compatible())throw std::runtime_error("Revolverator 0.3.0 requires Weapon XP API v4 when Weapon XP is enabled; update both mod packages");
  if(api->on_frame(mod,+[](void*){if(session)session->frame();},nullptr))throw std::runtime_error("Frame registration failed");
  runtime_hooks->commit();
  api->log(mod,OPENRAC_LOG_INFO,"Revolverator 0.3.0: Golden Novalis upgrade, charge-scaled impact explosions, weighted throw, independent arsenal saves, copy-only API v3");
  return 0;
 }catch(const std::exception& e){if(api&&api->fail)api->fail(mod,e.what());session.reset();runtime_hooks.reset();compatibility_modules.reset();weapon_assets.reset();return 1;}
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){session.reset();runtime_hooks.reset();compatibility_modules.reset();weapon_assets.reset();}

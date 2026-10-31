// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
// Fixture only: use a copied mod and an empty card/local directory. Positions,
// base ownership, challenge access and ordinary currency are seeded. HP, enemy
// rewards, XP and Golden ownership are never written by this helper.
#include <openrac/openrac_mod.h>
#include "revolverator_api.h"
#include "weapon_xp_api.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
const openrac_api* api{};openrac_mod* mod{};
const revolverator_api_v3* revo{};const weapon_xp_api_v4* xp{};
using Update=void(*)(unsigned);using Pad=void(*)(unsigned,unsigned,int);
using GetHit=unsigned(*)(unsigned,int,int);using Death=void(*)(unsigned,int,int);
Pad pad_original{};GetHit hit_original{};Death death_original{};
Update walloper_original{},offer_original{};
unsigned(*load_original)(int){};
void(*motion_original)(){};void(*velocity_original)(float){};
enum class Mode{Shop,Short,Long,Switch,Save,Reload,Rapid,Leveling};Mode mode=Mode::Shop;
enum class Step{Wait,Approach,Buy,AfterPurchase,CombatPrep,Capture,Hold,Flight,Settle,SaveRequest,SaveWait,LoadWait,Reuse,HoldAgain,AwaitBoth,Done};
Step step=Step::Wait;
unsigned host_frames{},ticks{},at{},checks{},failures{},deadline=2200;
unsigned spin_at{},fly_at{},impact_at{},projectile{},dome{},display{},display_stamp{},display_pclass{},display_pvars{};
unsigned triangle_pulses{},purchase_events{},purchase_transitions{},offer_calls{},held_gold_views{};
unsigned native_hits{},native_deaths{},recoil_frames{},recoil_motion_samples{},recoil_violations{},moving_after_recoil{};
unsigned baseline_ammo{},baseline_xp{},baseline_wall_xp{},baseline_wall_owned{},baseline_wall_ammo{};
unsigned previous_phase=999,previous_gold{};
unsigned raw_revo_size{},raw_revo_version{},grant_attempts{};
bool revo_module_found{},revo_query_found{},xp_module_found{},xp_query_found{};
std::uint64_t baseline_throws{},baseline_explosions{},baseline_hits{};
bool seeded{},triangle_pending{},want_fire{},want_walk{},finished{},switched{},require_xp=true;
bool acquisition_seen{},acquisition_finished{};
unsigned acquisition_frames{},duplicate_ratchet_frames{},unlocked_acquisition_frames{};
unsigned golden_scene_views{};
float throw_forward_alignment{};
unsigned rapid_windup_tick{};
unsigned acquisition_scene_count{};bool was_acquiring{},rules_overridden{};
weapon_xp_rules_v3 original_rules{};
float launch_blast{},launch_charge{},recoil_seconds{},max_recoil_speed{},max_recoil_displacement{};
struct Vec {float x{},y{},z{};};
Vec spawn{},recoil_last{};bool have_recoil_last{};
std::array<unsigned,4> enemies{};
std::array<unsigned char,120> native_gold{};
struct Receipt {unsigned target{},stamp{},hit{};std::array<unsigned char,64> payload{};};
std::array<Receipt,64> receipts{};unsigned receipt_count{};

template<class T>T read(unsigned p){T v{};if(p>=0x10000&&p<=0x2000000-sizeof(v))std::memcpy(&v,api->memory+p,sizeof(v));return v;}
template<class T>void write(unsigned p,const T& v){if(p>=0x10000&&p<=0x2000000-sizeof(v))std::memcpy(api->memory+p,&v,sizeof(v));}
bool span(unsigned p,unsigned n){return p>=0x10000&&n<=0x2000000&&p<=0x2000000-n;}
unsigned global(unsigned p){return api->relocate_data(p);}unsigned level(unsigned p){return api->relocate_level_data(0,p);}
unsigned load_level(int full){
 const auto result=load_original(full);
 // Seed NG+ before placed Gold cases run their first update, so the ten
 // original offers coexist with the eleventh case during the visual proof.
 if(api->loaded_level()==1)write(global(0x15ee20),1);
 return result;
}
unsigned hero(){return global(0x13f350);}unsigned body(){return read<unsigned>(hero()+0x2080);}
unsigned held(){return read<unsigned>(hero()+0x1090);}
bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
float distance(Vec a,Vec b){const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}
const char* mode_name(){return mode==Mode::Shop?"shop":mode==Mode::Short?"combatshort":mode==Mode::Long?"combatlong":mode==Mode::Switch?"switch-flight":mode==Mode::Save?"save":mode==Mode::Reload?"reload":mode==Mode::Rapid?"rapid":"leveling";}
void log(const char* text){api->log(mod,OPENRAC_LOG_INFO,text);}
void check(bool success,const char* name){
 ++checks;if(!success)++failures;char text[256];
 std::snprintf(text,sizeof text,"GOLDEN_QA {\"event\":\"check\",\"name\":\"%s\",\"pass\":%s,\"host_frame\":%u,\"tick\":%u}",name,success?"true":"false",host_frames,ticks);log(text);
}
revolverator_state_v1 state(){revolverator_state_v1 v{};v.size=sizeof v;if(revo)revo->get_state(&v);return v;}
revolverator_upgrade_v3 upgrade(){revolverator_upgrade_v3 v{};v.size=sizeof v;if(revo)revo->get_upgrade(&v);return v;}
revolverator_inventory_v2 inventory(unsigned key){revolverator_inventory_v2 v{};v.size=sizeof v;if(revo)revo->get_inventory(key,&v);return v;}
revolverator_progress_v2 progress(){revolverator_progress_v2 v{};v.size=sizeof v;if(revo)revo->get_progress(&v);return v;}
unsigned wall_xp(){weapon_xp_stats_v1 v{};v.size=sizeof v;if(xp)xp->v3.v2.base.get_stats(18,&v);return v.xp;}
void resolve(){
 if(!revo){const auto module=GetModuleHandleW(L"revolverator.dll");
  const auto query=module?reinterpret_cast<revolverator_query_fn>(GetProcAddress(module,"openrac_revolverator_query")):nullptr;
  const auto service=query?query(3):nullptr;
  revo_module_found=module!=nullptr;revo_query_found=query!=nullptr;
  if(service){raw_revo_size=service->size;raw_revo_version=service->version;}
  if(service&&service->version==3&&service->size>=sizeof(revolverator_api_v3))revo=reinterpret_cast<const revolverator_api_v3*>(service);
 }
 if(!xp){const auto module=GetModuleHandleW(L"weapon_xp.dll");
  const auto query=module?reinterpret_cast<weapon_xp_query_fn>(GetProcAddress(module,WEAPON_XP_QUERY_SYMBOL)):nullptr;
  xp_module_found=module!=nullptr;xp_query_found=query!=nullptr;
  if(query)xp=reinterpret_cast<const weapon_xp_api_v4*>(query(4));
 }
}
void snapshot(const char* event){
 const auto s=state();const auto u=upgrade();const auto i=inventory(REV_REVOLVERATOR_KEY);const auto p=progress();
 const auto position=read<Vec>(hero()+0x80);char text[1280];
 std::snprintf(text,sizeof text,
  "GOLDEN_QA {\"event\":\"%s\",\"mode\":\"%s\",\"host_frame\":%u,\"tick\":%u,\"step\":%u,\"phase\":%u,\"golden\":%u,\"blast_charge\":%.6f,\"throw_charge\":%.6f,\"recoil\":%u,\"captures\":%llu,\"throws\":%llu,\"impacts\":%llu,\"explosions\":%llu,\"area_hits\":%llu,\"native_hits\":%u,\"native_deaths\":%u,\"ammo\":%u,\"capacity\":%u,\"xp\":%u,\"walloper_xp\":%u,\"bolts\":%d,\"held\":%d,\"requested\":%d,\"held_class\":%u,\"hero_x\":%.5f,\"hero_y\":%.5f,\"hero_z\":%.5f,\"native_mode\":%d}",
  event,mode_name(),host_frames,ticks,unsigned(step),s.phase,u.golden,u.blast_charge,s.charge,u.release_recoil_active,
  static_cast<unsigned long long>(s.captures),static_cast<unsigned long long>(s.throws),static_cast<unsigned long long>(s.impacts),
  static_cast<unsigned long long>(u.golden_explosions),static_cast<unsigned long long>(u.golden_hits),native_hits,native_deaths,
  i.ammunition,i.capacity,p.xp,wall_xp(),read<int>(global(0x15ed98)),read<int>(hero()+0x10b8),read<int>(global(0x141660)),
  unsigned(read<unsigned short>(held()+0xa6)),position.x,position.y,position.z,read<int>(level(0x15f5c4)));log(text);
}
void finish(){
 if(finished)return;finished=true;want_fire=want_walk=triangle_pending=false;step=Step::Done;snapshot("final_state");
 char text[640];std::snprintf(text,sizeof text,
  "GOLDEN_QA {\"event\":\"result\",\"mode\":\"%s\",\"pass\":%s,\"checks\":%u,\"failures\":%u,\"host_frame\":%u,\"tick\":%u,\"triangle_pulses\":%u,\"purchase_events\":%u,\"gold_transitions\":%u,\"gold_render_views\":%u,\"launch_blast\":%.6f,\"recoil_frames\":%u,\"recoil_seconds\":%.6f,\"recoil_motion_samples\":%u,\"max_recoil_speed\":%.6f,\"max_recoil_displacement\":%.6f,\"resumed_motion_samples\":%u}",
  mode_name(),failures?"false":"true",checks,failures,host_frames,ticks,triangle_pulses,purchase_events,purchase_transitions,held_gold_views,
  launch_blast,recoil_frames,recoil_seconds,recoil_motion_samples,max_recoil_speed,max_recoil_displacement,moving_after_recoil);log(text);
}
std::array<unsigned char,120> gold_bytes(){
 std::array<unsigned char,120> result{};
 const auto variants=global(0x13e520),collected=global(0x14bec0);
 if(span(variants,40)&&span(collected,80)){std::memcpy(result.data(),api->memory+variants,40);std::memcpy(result.data()+40,api->memory+collected,80);}return result;
}
bool actor(unsigned p,unsigned klass){return span(p,256)&&read<unsigned short>(p+0xa6)==klass&&read<unsigned char>(p+0x20)<0x80&&span(read<unsigned>(p+0x78),1);}
bool ratchet_body(){
 const auto b=body();
 // The player's real state lives in global Hero, not the generic moby pvars
 // allocation used by enemies. A zero body.pvars is valid for Ratchet.
 return span(b,256)&&read<unsigned short>(b+0xa6)==0&&read<unsigned char>(b+0x20)<0x80&&span(read<unsigned>(b+0x24),0x4c);
}
void heartbeat(const char* event){
 const int loaded=api->loaded_level();const unsigned b=body();char text[1000];
 std::snprintf(text,sizeof text,
  "GOLDEN_QA {\"event\":\"%s\",\"host_frame\":%u,\"tick\":%u,\"loaded_level\":%d,\"step\":%u,\"revo_module\":%s,\"revo_query\":%s,\"revo_api\":%s,\"revo_size\":%u,\"revo_version\":%u,\"xp_module\":%s,\"xp_query\":%s,\"xp_api\":%s,\"native_mode\":%d,\"hero\":%u,\"body\":%u,\"body_class\":%u,\"body_pclass\":%u,\"body_pvars\":%u,\"body_state\":%u,\"ratchet_guard\":%s,\"health\":%d,\"held\":%d,\"grant_attempts\":%u}",
  event,host_frames,ticks,loaded,unsigned(step),revo_module_found?"true":"false",revo_query_found?"true":"false",revo?"true":"false",raw_revo_size,raw_revo_version,
  xp_module_found?"true":"false",xp_query_found?"true":"false",xp?"true":"false",read<int>(level(0x15f5c4)),hero(),b,
  unsigned(read<unsigned short>(b+0xa6)),read<unsigned>(b+0x24),read<unsigned>(b+0x78),unsigned(read<unsigned char>(b+0x20)),ratchet_body()?"true":"false",read<int>(hero()+0x22a8),read<int>(hero()+0x10b8),grant_attempts);log(text);
}
void binding_diagnostics(){
 const unsigned slot=read<unsigned char>(global(0x1b3ac0)+304),count=read<unsigned>(global(0x15ff00));
 const unsigned reverse=read<unsigned short>(global(0x1b3900)+slot*2),definition=read<unsigned>(global(0x1b3200)+slot*4);
 char text[640];std::snprintf(text,sizeof text,
  "GOLDEN_QA {\"event\":\"shop_bindings\",\"host_frame\":%u,\"challenge\":%d,\"class304_slot\":%u,\"resident_count\":%u,\"reverse_class\":%u,\"pclass\":%u,\"pclass_valid\":%s,\"display2019_slot\":%u,\"dome_observed\":%u,\"display_observed\":%u}",
  host_frames,read<int>(global(0x15ee20)),slot,count,reverse,definition,span(definition,0x4c)?"true":"false",unsigned(read<unsigned char>(global(0x1b3ac0)+2019)),dome,display);log(text);
 const char* names[]{"FUN_0020c4f8","FUN_0020c828","FUN_001fa030","FUN_001ff308","FUN_L00_00235e18","FUN_0020b178","FUN_L00_00237190","FUN_L00_002371e0","FUN_L00_00237200"};
 for(const auto name:names){std::snprintf(text,sizeof text,"GOLDEN_QA {\"event\":\"function_lookup\",\"symbol\":\"%s\",\"found\":%s}",name,api->function(name)?"true":"false");log(text);}
 const char* callbacks[]{"FUN_L00_00237190","FUN_L00_002371e0","FUN_L00_00237200"};
 const unsigned addresses[]{0x24c828u,0x24c878u,0x24c898u};
 for(unsigned i=0;i<3;++i){const auto canonical=api->function(callbacks[i]),by_address=api->function_at(addresses[i]);
  std::snprintf(text,sizeof text,"GOLDEN_QA {\"event\":\"callback_binding\",\"index\":%u,\"address\":%u,\"symbol\":\"%s\",\"canonical_found\":%s,\"address_found\":%s,\"same_pointer\":%s}",i,addresses[i],callbacks[i],canonical?"true":"false",by_address?"true":"false",canonical&&canonical==by_address?"true":"false");log(text);
 }
}
template<class Visit>void each_moby(Visit visit){
 const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
 if(!span(first,256)||last<=first||last-first>4096*256||!span(last-256,256))return;
 for(auto m=first;m<last;m+=256)if(read<unsigned char>(m+0x20)<0xfe)visit(m);
}
bool own_case(unsigned moby){
 const auto p=read<Vec>(moby+0x10);return actor(moby,304)&&
  finite(p)&&std::abs(p.x-250.8f)<.2f&&std::abs(p.y-123.6f)<.2f&&std::abs(p.z-54.4966202f)<.3f;
}
void locate_shop(){
 each_moby([](unsigned moby){
  if(own_case(moby))dome=moby;
  const auto c=read<unsigned short>(moby+0xa6);const auto p=read<Vec>(moby+0x10);
  if(!display&&(c==180||c==2018||c==2019)&&moby!=held()&&finite(p)&&
     std::abs(p.x-250.8f)<.2f&&std::abs(p.y-123.6f)<.2f&&std::abs(p.z-56.2466202f)<.3f){
   display=moby;display_stamp=read<unsigned>(moby+0x38);display_pclass=read<unsigned>(moby+0x24);display_pvars=read<unsigned>(moby+0x78);
  }
 });
}
bool display_retired(){
 if(!display)return false;
 const unsigned klass=read<unsigned short>(display+0xa6);
 return !span(display,256)||read<unsigned char>(display+0x20)>=0xfe||read<unsigned>(display+0x38)!=display_stamp||
  read<unsigned>(display+0x24)!=display_pclass||read<unsigned>(display+0x78)!=display_pvars||(klass!=180&&klass!=2018&&klass!=2019);
}
bool teleport(Vec position,float yaw){
 const auto h=hero(),b=body();
 if(!span(h,0x22ac)||!ratchet_body()||read<int>(h+0x22a8)<=0||read<int>(level(0x15f5c4))!=0||!finite(position)||!std::isfinite(yaw))return false;
 const auto place=reinterpret_cast<void(*)(unsigned,unsigned,int,int)>(api->function("FUN_L00_00216f90"));
 if(!place)return false;
 unsigned saved=0;const auto buffer=api->stack_push(32,&saved);if(!buffer)return false;
 const std::array<float,4> point{{position.x,position.y,position.z,1}},rotation{{0,0,yaw,0}};
 std::memcpy(api->memory+buffer,point.data(),16);std::memcpy(api->memory+buffer+16,rotation.data(),16);
 place(buffer,buffer+16,0,1);api->stack_pop(saved);
 const auto old=read<Vec>(b+0x10);auto sphere=read<Vec>(b);sphere.x+=(position.x-old.x)*1024.f;sphere.y+=(position.y-old.y)*1024.f;sphere.z+=(position.z-old.z)*1024.f;
 write(h+0x80,position);write(h+0x98,yaw);write(b+0x10,position);write(b,sphere);write(b+0x48,yaw);
 for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u})write(h+offset,0.f);
 if(const auto basis=reinterpret_cast<void(*)(unsigned,unsigned)>(api->function("FUN_001fa030")))basis(b+0xc0,b+0x40);
 return true;
}
void position_enemy(unsigned enemy,Vec position){
 if(!actor(enemy,459)||!finite(position))return;
 const auto old=read<Vec>(enemy+0x10);auto sphere=read<Vec>(enemy);
 sphere.x+=(position.x-old.x)*1024.f;sphere.y+=(position.y-old.y)*1024.f;sphere.z+=(position.z-old.z)*1024.f;
 write(enemy+0x10,position);write(enemy,sphere);write<unsigned char>(enemy+0x31,1);
}
void choose_enemies(){
 unsigned count=0;each_moby([&](unsigned moby){if(count<enemies.size()&&actor(moby,459))enemies[count++]=moby;});
 check(count==enemies.size(),"four_original_birdbots_available");projectile=enemies[0];
}
void seed_near_target(){
 const float yaw=read<float>(hero()+0x98);const auto p=read<Vec>(hero()+0x80);
 position_enemy(projectile,add(p,{std::cos(yaw)*1.5f,std::sin(yaw)*1.5f,0}));
}
void seed_impact_targets(){
 const float yaw=read<float>(hero()+0x98),c=std::cos(yaw),s=std::sin(yaw);
 auto p=read<Vec>(projectile+0x10);p.z+=.6f;
 const std::array<Vec,3> offsets{{{5.5f,0,0},{6.5f,1.f,0},{6.5f,-1.f,0}}};
 for(unsigned i=1;i<enemies.size();++i){const auto d=offsets[i-1];position_enemy(enemies[i],add(p,{c*d.x-s*d.y,s*d.x+c*d.y,d.z}));}
 char text[280];std::snprintf(text,sizeof text,"GOLDEN_QA {\"event\":\"crowd_positioned_once\",\"host_frame\":%u,\"projectile\":%u,\"target2\":%u,\"target3\":%u,\"target4\":%u,\"health_or_rewards_edited\":false}",host_frames,projectile,enemies[1],enemies[2],enemies[3]);log(text);
}
void inspect_recoil(){
 if(!seeded||finished||!revo)return;
 const auto u=upgrade();const auto velocity=read<Vec>(hero()+0xe0);const float speed=std::hypot(velocity.x,velocity.y);
 if(u.release_recoil_active){
  ++recoil_motion_samples;max_recoil_speed=std::max(max_recoil_speed,speed);
  if(!std::isfinite(speed)||speed>.0001f)++recoil_violations;
 }else if(want_walk&&(step==Step::Flight||step==Step::Settle)&&speed>.001f)++moving_after_recoil;
}
void motion(){motion_original();inspect_recoil();}
void velocity(float angle){velocity_original(angle);inspect_recoil();}
void walloper(unsigned moby){walloper_original(moby);if(moby==held()&&read<unsigned short>(moby+0xa6)==2018){
 ++held_gold_views;
 if(fly_at&&ticks>=fly_at&&ticks<=fly_at+14){const auto shaft=read<Vec>(moby+0xd0);const float norm=std::sqrt(shaft.x*shaft.x+shaft.y*shaft.y+shaft.z*shaft.z);
  const float yaw=read<float>(hero()+0x98);if(norm>.0001f)throw_forward_alignment=std::max(throw_forward_alignment,-(shaft.x*std::cos(yaw)+shaft.y*std::sin(yaw))/norm);}
}}
void offer(unsigned moby){
 const bool ours=seeded&&own_case(moby);const unsigned before=ours?upgrade().golden:0;
 offer_original(moby);
 if(ours){++offer_calls;if(!before&&upgrade().golden){++purchase_events;snapshot("native_purchase_boundary");}}
}
void pad(unsigned address,unsigned report,int size){
 pad_original(address,report,size);
 if(address!=global(0x13c940)||!seeded||finished)return;
 if(triangle_pending){write(address+0x1a4,read<unsigned>(address+0x1a4)|0x10u);triangle_pending=false;++triangle_pulses;}
 // Verified processed left-stick fields used by Hero_UpdateMotionInput. The
 // helper never zeros hero velocity to manufacture a recoil pass.
 if(want_walk){write(address+0x108,.9f);write(address+0x10c,0.f);}
}
void on_pad(void*,int port,unsigned char* report,int size){
 if(port||size<6||!seeded||finished)return;
 report[1]=static_cast<unsigned char>(want_fire?report[1]&~0x20:report[1]|0x20);
 report[1]=static_cast<unsigned char>(report[1]|0x10);
}
unsigned get_hit(unsigned target,int mask,int keep){
 const auto hit=hit_original(target,mask,keep);
 if(!seeded||finished||!span(hit,64)||read<unsigned short>(hit+0x2a)!=2017||read<unsigned>(hit+0x24)!=0x830000)return hit;
 Receipt receipt{target,read<unsigned>(target+0x38),hit,{}};std::memcpy(receipt.payload.data(),api->memory+hit,64);
 for(unsigned i=0;i<receipt_count;++i)if(receipts[i].target==target&&receipts[i].stamp==receipt.stamp)return hit;
 receipts[receipt_count%receipts.size()]=receipt;if(receipt_count<receipts.size())++receipt_count;
 ++native_hits;char text[512];std::snprintf(text,sizeof text,
  "GOLDEN_QA {\"event\":\"native_accepted_hit\",\"host_frame\":%u,\"tick\":%u,\"target\":%u,\"class\":%u,\"stamp\":%u,\"origin\":2017,\"attacker\":%u,\"flags\":%u,\"damage\":%.6f,\"held\":%d}",
  host_frames,ticks,target,unsigned(read<unsigned short>(target+0xa6)),receipt.stamp,read<unsigned>(hit+0x20),read<unsigned>(hit+0x24),read<float>(hit+0x2c),read<int>(hero()+0x10b8));log(text);return hit;
}
void death(unsigned target,int flags,int index){
 bool ours=false;for(unsigned i=0;i<receipt_count;++i)if(receipts[i].target==target&&receipts[i].stamp==read<unsigned>(target+0x38))ours=true;
 if(ours){++native_deaths;char text[280];std::snprintf(text,sizeof text,"GOLDEN_QA {\"event\":\"native_death\",\"host_frame\":%u,\"target\":%u,\"class\":%u,\"base_bolts\":%d,\"budget\":%d}",host_frames,target,unsigned(read<unsigned short>(target+0xa6)),read<short>(target+0xb4),read<short>(target+0xb6));log(text);}
 death_original(target,flags,index);
}
void frame(void*){
 if(finished)return;
 ++host_frames;resolve();
 if(host_frames==1||host_frames%100==0)heartbeat("heartbeat");
 if(host_frames>=deadline){heartbeat("deadline");check(false,"scenario_completed_before_deadline");finish();return;}
 if(api->loaded_level()!=1)return;
 if(!revo||(require_xp&&!xp))return;
 const int native_mode=read<int>(level(0x15f5c4));
 if(native_mode==5&&!was_acquiring)++acquisition_scene_count;
 was_acquiring=native_mode==5;
 if(seeded&&upgrade().golden&&native_mode==5){
  if(!acquisition_seen){snapshot("gold_acquisition_started");acquisition_seen=true;
   const auto scene=api->relocate_level_data(1,0x16cce0);const int count=read<short>(scene+0x44);
   for(int i=0;i<std::min(count,14);++i){const auto actor=read<unsigned>(scene+0x178+unsigned(i)*4);char details[256];
    std::snprintf(details,sizeof details,"GOLDEN_QA {\"event\":\"scene_actor\",\"index\":%d,\"address\":%u,\"class\":%u,\"pclass\":%u,\"update\":%u,\"state\":%u}",
     i,actor,unsigned(read<unsigned short>(actor+0xa6)),read<unsigned>(actor+0x24),read<unsigned>(actor+0x74),unsigned(read<unsigned char>(actor+0x20)));log(details);}
  }
  ++acquisition_frames;
  const auto native_scene=api->relocate_level_data(1,0x16cce0);
  const int actors=read<short>(native_scene+0x44);
  for(int i=0;i<std::min(actors,14);++i){const auto item=read<unsigned>(native_scene+0x178+unsigned(i)*4);
   if(read<unsigned short>(item+0xa6)==2018)++golden_scene_views;}
  if(acquisition_frames==45){
   for(const auto scene:{api->relocate_level_data(1,0x16cce0),global(0x18cb20)}){
    const int count=read<short>(scene+0x44);char header[200];std::snprintf(header,sizeof header,"GOLDEN_QA {\"event\":\"scene_header\",\"address\":%u,\"count\":%d}",scene,count);log(header);
    for(int i=0;i<std::min(count,14);++i){const auto actor=read<unsigned>(scene+0x178+unsigned(i)*4);char details[256];
     std::snprintf(details,sizeof details,"GOLDEN_QA {\"event\":\"scene_actor_late\",\"scene\":%u,\"index\":%d,\"address\":%u,\"class\":%u,\"pclass\":%u,\"update\":%u,\"state\":%u}",
      scene,i,actor,unsigned(read<unsigned short>(actor+0xa6)),read<unsigned>(actor+0x24),read<unsigned>(actor+0x74),unsigned(read<unsigned char>(actor+0x20)));log(details);}
   }
  }
  if(!read<unsigned char>(hero()+0x20a5)||!read<unsigned char>(hero()+0x20ac))++unlocked_acquisition_frames;
  unsigned visible=0;each_moby([&](unsigned moby){if(actor(moby,0)&&!(read<unsigned short>(moby+0x34)&1))++visible;});
  if(visible>1)++duplicate_ratchet_frames;
 }
 if(acquisition_seen&&!acquisition_finished&&native_mode==0){acquisition_finished=true;snapshot("gold_acquisition_finished");}
 if(native_mode!=0||read<int>(hero()+0x22a8)<=0)return;
 ++ticks;const auto s=state();const auto u=upgrade();
 if(seeded&&s.phase!=previous_phase){snapshot("phase");previous_phase=s.phase;}
 if(seeded&&u.golden!=previous_gold){if(u.golden)++purchase_transitions;previous_gold=u.golden;snapshot("gold_transition");}
 if(ticks%30==0&&seeded)snapshot("tick");
 switch(step){
 case Step::Wait:
  if(ticks>=60&&ratchet_body()){
   if(mode==Mode::Reload){
    check(!u.golden&&!inventory(REV_REVOLVERATOR_KEY).owned,"fresh_process_starts_without_custom_ownership");
    const auto mc=global(0x13d290);
    write<int>(mc+0x14,0);write<int>(mc+0xdc,13);write<int>(mc+0xe0,0);write<int>(mc+0xe4,0);
    seeded=true;step=Step::LoadWait;at=ticks;snapshot("native_reload_requested");break;
   }
   spawn=read<Vec>(hero()+0x80);
   ++grant_attempts;if(!revo->grant(REV_REVOLVERATOR_KEY,1)){if(grant_attempts==1)heartbeat("grant_wait");break;}
   baseline_ammo=inventory(REV_REVOLVERATOR_KEY).ammunition;baseline_xp=progress().xp;baseline_wall_xp=wall_xp();
   if(mode==Mode::Leveling){weapon_xp_configuration_v3 cfg{};cfg.size=sizeof cfg;
    original_rules.size=sizeof original_rules;check(xp->v3.get_configuration(&cfg)==WEAPON_XP_OK&&xp->v3.get_rules(18,&original_rules)==WEAPON_XP_OK,"capture_original_session_rules_for_levelup_fixture");
    auto rules=original_rules;for(unsigned i=1;i<WEAPON_XP_LEVEL_COUNT;++i)rules.thresholds[i]=20+(i-1)*10000;
    rules_overridden=xp->v3.set_rules(&rules,1,cfg.revision)==WEAPON_XP_OK;check(rules_overridden,"lower_only_session_thresholds_without_seeding_or_awarding_xp");}
   const auto wall=inventory(18);baseline_wall_owned=wall.owned;baseline_wall_ammo=wall.ammunition;
   native_gold=gold_bytes();write(global(0x15ee20),1);write(global(0x15ed98),60000);
   seeded=true;previous_gold=u.golden;step=Step::Approach;at=ticks;
   check(baseline_ammo==20&&inventory(REV_REVOLVERATOR_KEY).capacity==20,"owned_base_capacity20");
   check(!u.golden&&u.normal_price==50000&&u.gold_bolt_price==0,"gold_unowned_price50000_normal_zero_gold");
   snapshot("fixture_seeded");
  }break;
 case Step::Approach:
  locate_shop();
  if(ticks-at==90)binding_diagnostics();
  if(ticks-at==20)check(teleport({250.8f,125.5f,54.4966202f},-1.5707963f),"guarded_teleport_to_native_gold_room");
  if(ticks-at>=100&&dome&&display&&read<int>(hero()+0x10b8)==18&&ratchet_body()){
   check(own_case(dome),"additional_native304_dome_live");check(!display_retired(),"gold_display_live_before_purchase");
   unsigned cases=0;each_moby([&](unsigned moby){if(actor(moby,304))++cases;});
   check(cases==11,"eleven_live_cases_preserve_all_ten_original_gold_offers");
   check(std::abs(read<float>(display+0x2c)-.1458333283662796f*1.6f)<.00001f,"gold_display_matches_enlarged_native_shop_scale");
   check(std::abs(read<float>(display+0x48)+1.57079632679f)<.00001f,"gold_display_keeps_fixed_orientation");
   step=Step::Buy;at=ticks;triangle_pending=true;snapshot("triangle_armed");
  }break;
 case Step::Buy:
  if(u.golden){
   check(read<int>(global(0x15ed98))==10000,"purchase_deducts_exactly50000_normal_bolts");
   check(gold_bytes()==native_gold,"ten_native_gold_flags_and_collection_bits_preserved");
   check(inventory(REV_REVOLVERATOR_KEY).ammunition==baseline_ammo&&progress().xp==baseline_xp,"gold_purchase_preserves_ammo_and_xp");
   const auto wall=inventory(18);check(wall.owned==baseline_wall_owned&&wall.ammunition==baseline_wall_ammo&&wall_xp()==baseline_wall_xp,"native_walloper_state_independent");
   check(purchase_events==1&&purchase_transitions==1,"one_native_purchase_and_one_logical_transition");
   step=Step::AfterPurchase;at=ticks;snapshot("purchased");
  }else if(ticks-at>120){check(false,"native_triangle_purchase_completed");finish();}
  break;
 case Step::AfterPurchase:
  if(ticks-at>=150){
   check(acquisition_seen&&acquisition_finished&&acquisition_frames>30,"gold_purchase_plays_complete_native_acquisition");
   check(golden_scene_views>150,"native_acquisition_renders_the_gold_rotatore_instead_of_the_walloper");
   check(!duplicate_ratchet_frames&&!unlocked_acquisition_frames,"native_acquisition_has_one_visible_ratchet_and_locked_input");
   check(triangle_pulses==1&&purchase_events==1&&purchase_transitions==1,"purchase_event_not_replayed");
   check(display_retired()&&own_case(dome),"only_display_retires_sold_dome_remains");
   check(held_gold_views>0,"held_model_renders_gold_class2018");
   check(read<int>(global(0x15ed98))==10000,"purchase_currency_stable_after_input_release");
   check(gold_bytes()==native_gold,"native_gold_state_stable_after_purchase");
   if(mode==Mode::Shop){finish();break;}
   if(mode==Mode::Save){step=Step::SaveRequest;at=ticks;break;}
   check(teleport(spawn,mode==Mode::Rapid?-1.57079632679f:0.f),"return_to_original_native_combat_area");
   choose_enemies();if(!projectile){finish();break;}
   baseline_throws=s.throws;baseline_explosions=u.golden_explosions;baseline_hits=u.golden_hits;
   step=Step::CombatPrep;at=ticks;snapshot("combat_prepared");
  }break;
 case Step::CombatPrep:
  if(ticks-at>=60){seed_near_target();want_fire=true;step=Step::Capture;at=ticks;}
  break;
 case Step::Capture:
  if(s.phase==REV_IDLE||s.phase==REV_WINDUP)seed_near_target();
  if(s.phase==REV_SPINNING){
   each_moby([](unsigned moby){if(actor(moby,459)&&read<unsigned>(moby+0x94)==0&&read<unsigned>(moby+0x98)==1)projectile=moby;});
   spin_at=ticks;step=Step::Hold;snapshot("capture_confirmed");
  }else if(ticks-at>150){check(false,"native_held_capture_completed");finish();}
  break;
 case Step::Hold:
  if(mode!=Mode::Long&&ticks-spin_at>=19)want_fire=false;
  if(s.phase==REV_FLYING){
   want_fire=false;want_walk=true;fly_at=ticks;launch_blast=u.blast_charge;launch_charge=s.charge;
   check(s.throws==baseline_throws+1,"one_native_controller_throw");
   check(mode==Mode::Long?launch_blast>.98f:launch_blast>.08f&&launch_blast<.25f,"blast_charge_matches_hold_until_auto_cap");
   if(mode!=Mode::Rapid)seed_impact_targets();
   step=Step::Flight;snapshot("released");
   if(mode==Mode::Rapid){
    const auto point=read<Vec>(projectile+0x10);const float yaw=read<float>(hero()+0x98);
    position_enemy(enemies[2],add(point,{std::cos(yaw)*9.f,std::sin(yaw)*9.f,.6f}));
    position_enemy(enemies[3],add(point,{-std::cos(yaw)*12.f,-std::sin(yaw)*12.f,.6f}));
    want_walk=false;check(teleport(read<Vec>(hero()+0x80),1.57079632679f),"turn_in_place_for_an_independent_second_capture");
    projectile=enemies[1];step=Step::Reuse;at=ticks;
   }
   if(mode==Mode::Switch){
    const auto give=reinterpret_cast<void(*)(int,int)>(api->function("FUN_L00_00260860"));
    check(give!=nullptr,"native_wrench_equip_function_available");if(give){give(8,1);switched=true;snapshot("switch_requested_after_release");}
   }
  }else if(ticks-spin_at>220){check(false,"release_completed");finish();}
  break;
 case Step::Flight:
  if(u.golden_explosions>baseline_explosions){
   impact_at=ticks;step=Step::Settle;snapshot("exploded");
  }else if(ticks-fly_at>130){check(false,"real_collision_generates_golden_blast");finish();}
  break;
 case Step::Reuse:
  if(s.phase==REV_IDLE||s.phase==REV_WINDUP)seed_near_target();
  if(ticks-at>=3)want_fire=true;
  if(s.phase==REV_WINDUP&&!rapid_windup_tick){rapid_windup_tick=ticks;
   check(ticks-fly_at<=10,"second_press_buffer_starts_the_next_attack_within150ms");snapshot("rapid_windup");}
  if(s.phase==REV_SPINNING){
   unsigned owned=0;each_moby([&](unsigned moby){if(actor(moby,459)&&read<unsigned>(moby+0x94)==0&&read<unsigned>(moby+0x98)==1)++owned;});
   check(s.captures==2&&owned>=2,"native_first_projectile_and_second_captive_coexist");
   spin_at=ticks;step=Step::HoldAgain;snapshot("rapid_second_capture");
  }else if(ticks-at>90){check(false,"second_capture_completed_promptly");finish();}
  break;
 case Step::HoldAgain:
  if(ticks-spin_at>=19)want_fire=false;
  if(s.throws==baseline_throws+2){check(inventory(REV_REVOLVERATOR_KEY).ammunition+2==baseline_ammo,"two_captures_consume_exactly_two_rounds");
   // Deliberately miss with the second projectile: its timeout must remain
   // independent of the first collision, with no invented Golden explosion.
   position_enemy(enemies[3],add(read<Vec>(hero()+0x80),{50,50,0}));
   step=Step::AwaitBoth;at=ticks;snapshot("rapid_second_release");}
  else if(ticks-spin_at>90){check(false,"second_release_completed");finish();}
  break;
 case Step::AwaitBoth:
  if(ticks-at>=150){
   check(u.golden_explosions==baseline_explosions+1&&s.impacts==2,"collision_and_missed_projectile_complete_independently_without_a_false_timeout_explosion");
   check(native_hits>=2&&native_deaths>0&&progress().xp>baseline_xp,"rapid_reuse_preserves_accepted_native_damage_deaths_and_xp");
   check(wall_xp()==baseline_wall_xp&&gold_bytes()==native_gold,"rapid_reuse_preserves_native_walloper_and_gold_state");finish();
  }break;
 case Step::Settle:
  if(ticks-impact_at>=20)want_walk=false;
  if(ticks-impact_at>=150){
   check(u.golden_explosions==baseline_explosions+1,"exactly_one_native_explosion");
   check(u.golden_hits>=baseline_hits+2&&native_hits>=2,"area_damage_reaches_multiple_native_accepted_targets");
   check(native_deaths>0,"native_death_bookkeeping_from_golden_hit");
   check(inventory(REV_REVOLVERATOR_KEY).ammunition+1==baseline_ammo,"one_shot_costs_one_round_only");
   check(wall_xp()==baseline_wall_xp,"golden_kills_do_not_credit_native_walloper");
   if(require_xp)check(progress().xp>baseline_xp,"revolverator_earns_actual_native_bolt_based_xp");
   if(mode==Mode::Leveling){check(progress().level==2&&acquisition_scene_count==2,"actual_enemy_xp_triggers_exactly_one_additional_v2_acquisition");
    check(golden_scene_views>300&&!duplicate_ratchet_frames&&!unlocked_acquisition_frames,"levelup_keeps_gold_model_and_one_locked_visible_ratchet");}
   if(mode==Mode::Switch){check(switched&&read<int>(hero()+0x10b8)==8,"launched_enemy_completes_after_actual_wrench_equip");check(!u.release_recoil_active,"equip_switch_clears_recoil");}
   else{check(recoil_motion_samples>0&&recoil_violations==0&&max_recoil_speed<.0001f,"native_motion_hooks_stop_only_ratchet_during_recoil");
    check(throw_forward_alignment>.85f,"real_native_wrist_and_drill_sweep_forward_during_throw");
    check(recoil_seconds>=.1f&&recoil_seconds<=.17f&&max_recoil_displacement<.05f,"recoil_has_short_bounded_duration_with_no_horizontal_drift");
    check(moving_after_recoil>0,"native_locomotion_resumes_after_recoil");}
   check(gold_bytes()==native_gold,"native_gold_flags_preserved_through_combat");finish();
  }break;
 case Step::SaveRequest:{
  const auto mc=global(0x13d290);
  if(ticks-at>300){check(false,"native_save_becomes_ready");finish();break;}
  if(read<int>(mc+0xd4)>=3||read<int>(mc+0xdc)>=0)break;
  write<int>(global(0x13d428)+18*4,7);
  write<int>(mc+0x14,0);write<int>(mc+0xc0,0);write<int>(mc+0x1c,0);write<int>(mc+0xe4,0);
  const auto autosave=reinterpret_cast<int(*)(int,int)>(api->function("FUN_0020b178"));
  check(autosave&&autosave(1,-1)>0,"native_save_request_accepted");
  step=Step::SaveWait;at=ticks;snapshot("native_save_requested");break;
 }
 case Step::SaveWait:
  if(ticks-at>=360){
   const auto mc=global(0x13d290);
   check(read<int>(mc+0xd4)<3&&read<int>(mc+0xdc)<0&&read<int>(mc+0xe4)==0,"native_save_completed_without_card_error");
   check(u.golden&&inventory(REV_REVOLVERATOR_KEY).ammunition==7,"saved_gold_ammunition_snapshot_is_seven");
   check(read<int>(global(0x15ed98))==10000&&gold_bytes()==native_gold,"save_preserves_purchase_economy_and_native_gold");finish();
  }break;
 case Step::LoadWait:
  if(ticks-at>=420){
   const auto mc=global(0x13d290);const auto item=inventory(REV_REVOLVERATOR_KEY);
   check(read<int>(mc+0xd4)<3&&read<int>(mc+0xdc)<0&&read<int>(mc+0xe4)==0,"native_reload_completed_without_card_error");
   check(u.golden&&item.owned&&item.ammunition==7,"fresh_process_restores_golden_and_exact_ammo_from_sidecar");
   check(read<int>(global(0x15ed98))==10000,"native_bolt_balance_restored_with_golden_save");finish();
  }break;
 case Step::Done:break;
 }
 if(step==Step::Flight||step==Step::Settle){
  if(upgrade().release_recoil_active){
   ++recoil_frames;const float dt=read<float>(global(0x15ed7c));if(std::isfinite(dt))recoil_seconds+=dt;
   const auto position=read<Vec>(hero()+0x80);if(have_recoil_last)max_recoil_displacement=std::max(max_recoil_displacement,distance(position,recoil_last));
   recoil_last=position;have_recoil_last=true;
  }
 }
}
}
extern "C" {OPENRAC_MOD_DEFINE("golden-native-probe","0.1.0")}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* services,openrac_mod* handle){
 api=services;mod=handle;const char* scenario=api->setting_string(mod,"mode");
 if(!std::strcmp(scenario,"combatshort"))mode=Mode::Short;
 else if(!std::strcmp(scenario,"combatlong"))mode=Mode::Long;
 else if(!std::strcmp(scenario,"switch-flight"))mode=Mode::Switch;
 else if(!std::strcmp(scenario,"save"))mode=Mode::Save;
 else if(!std::strcmp(scenario,"reload"))mode=Mode::Reload;
 else if(!std::strcmp(scenario,"rapid"))mode=Mode::Rapid;
 else if(!std::strcmp(scenario,"leveling"))mode=Mode::Leveling;
 else if(std::strcmp(scenario,"shop")){api->fail(mod,"Unknown Golden QA scenario");return 1;}
 const auto setting=api->setting_int(mod,"deadline");if(setting>=300&&setting<=36000)deadline=static_cast<unsigned>(setting);
 require_xp=api->setting_bool(mod,"require_xp")!=0;
 log("GOLDEN_QA {\"event\":\"probe_loaded\",\"diagnostic_revision\":3}");
 struct Hook {const char* id;const char* signature;void* replacement;void** original;};
 const Hook hooks[]{
  {"rac1.level.load_gameplay","u32(i32)",reinterpret_cast<void*>(load_level),reinterpret_cast<void**>(&load_original)},
  {"rac1.pad.process_report","void(u32,u32,i32)",reinterpret_cast<void*>(pad),reinterpret_cast<void**>(&pad_original)},
  {"rac1.combat.get_hit","u32(u32,i32,i32)",reinterpret_cast<void*>(get_hit),reinterpret_cast<void**>(&hit_original)},
  {"rac1.moby.record_death","void(u32,i32,i32)",reinterpret_cast<void*>(death),reinterpret_cast<void**>(&death_original)},
  {"rac1.moby.moby180.update","void(u32)",reinterpret_cast<void*>(walloper),reinterpret_cast<void**>(&walloper_original)},
  {"rac1.moby.item_offer.update","void(u32)",reinterpret_cast<void*>(offer),reinterpret_cast<void**>(&offer_original)},
  {"rac1.player.update_motion_input","void()",reinterpret_cast<void*>(motion),reinterpret_cast<void**>(&motion_original)},
  {"rac1.player.set_planar_velocity","void(f32)",reinterpret_cast<void*>(velocity),reinterpret_cast<void**>(&velocity_original)}
 };
 for(const auto& hook:hooks)if(const int result=api->hook(mod,hook.id,hook.signature,hook.replacement,hook.original))return result;
 const int result=api->on_frame(mod,frame,nullptr);if(result)return result;
 const int pad_result=api->on_pad(mod,on_pad,nullptr);
 if(!pad_result)log("GOLDEN_QA {\"event\":\"callbacks_registered\",\"frame\":true,\"pad\":true}");
 return pad_result;
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){
 if(rules_overridden&&xp){weapon_xp_configuration_v3 cfg{};cfg.size=sizeof cfg;
  if(xp->v3.get_configuration(&cfg)==WEAPON_XP_OK)xp->v3.set_rules(&original_rules,1,cfg.revision);}
}

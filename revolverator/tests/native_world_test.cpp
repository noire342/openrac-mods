// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_world.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <set>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* what){++checks;if(!condition){std::fprintf(stderr,"native world: %s\n",what);std::exit(1);}}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
float distance(Vec3 a,Vec3 b){return std::sqrt(length_squared(a-b));}
constexpr unsigned hero=0x13f350,body=0x30000,weapon=0x31000,pool=0x40000,stack_base=0x1e00000;
struct Packet {unsigned target;std::array<unsigned char,48> payload;};
struct Fixture;
Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};GuestArena arena;NativeAmmunition ammunition;NativeRotor rotor;NativeWorld world;
 std::vector<Packet> packets;
 std::vector<float> burst_sizes;
 unsigned stack_cursor=stack_base,stack_fail_size{},burst_calls{},allocations{},ray_calls{};
 float burst_scale{},burst_flash{},wall_x=10000;
 int current_level=1;Vec3 burst_position{};
 std::function<void()> on_burst;
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 static unsigned relocate(unsigned at){return at;}
 static unsigned relocate_level(int,unsigned at){return at;}
 static int loaded(){return live->current_level;}
 static int setting(openrac_mod*,const char*){return 0;}
 static void log(openrac_mod*,int,const char*){}
 static unsigned stack_push(unsigned size,unsigned* saved){
  if(live->stack_fail_size==size)return 0;
  *saved=live->stack_cursor;const auto at=live->stack_cursor;live->stack_cursor+=(size+15u)&~15u;return at;
 }
 static void stack_pop(unsigned saved){live->stack_cursor=saved;}
 static unsigned allocate(unsigned){++live->allocations;return 0;}
 static int take_ammo(int,int){return 1;}
 static void bone(unsigned,int,unsigned){}
 static void basis(unsigned,unsigned){}
 static int set_state(int state,int){live->put(hero+0x2084,state);return 1;}
 static void animation(int,int,float){}
 static int ray(unsigned from,unsigned to,int mask,unsigned ignore,int){
  ++live->ray_calls;require(mask==0x24,"LOS uses the verified native collision mask");
  require(ignore==body||(ignore>=pool&&ignore<live->get<unsigned>(0x15ff20)),"ray ignores only a thrown actor or capturing hero");
  const auto a=live->get<Vec3>(from),b=live->get<Vec3>(to);
  if(a.x<live->wall_x&&b.x>=live->wall_x){
   const float u=(live->wall_x-a.x)/(b.x-a.x);
   live->put(0x173e60,a+(b-a)*u);live->put(0x173e58,0u);live->put(0x173e80,Vec3{-1,0,0});return 1;
  }
  return 0;
 }
 static void post_hit(unsigned target,unsigned payload){
  require((payload&15u)==0,"native damage payload is guest-aligned16");
  Packet packet{target,{}};std::memcpy(packet.payload.data(),live->ram.data()+payload,48);live->packets.push_back(packet);
 }
 static void burst(unsigned owner,unsigned position,int sound,float scale,float flash){
  require(owner>=pool&&owner<live->get<unsigned>(0x15ff20),"explosion glows belong to a thrown enemy, never a second Ratchet");
  require((position&15u)==0&&sound==-1,"visual effect uses aligned guest position and no arbitrary enemy sound index");
  ++live->burst_calls;live->burst_position=live->get<Vec3>(position);live->burst_scale=scale;live->burst_flash=flash;
  live->burst_sizes.push_back(scale);
  if(live->on_burst){auto callback=std::move(live->on_burst);callback();}
 }
 static void* function(const char* name){
  if(std::strcmp(name,"FUN_001ff288")==0)return reinterpret_cast<void*>(&allocate);
  if(std::strcmp(name,"FUN_L00_00233db8")==0)return reinterpret_cast<void*>(&take_ammo);
  if(std::strcmp(name,"FUN_0020cca8")==0)return reinterpret_cast<void*>(&bone);
  if(std::strcmp(name,"FUN_001fa030")==0)return reinterpret_cast<void*>(&basis);
  if(std::strcmp(name,"FUN_L00_00259a88")==0)return reinterpret_cast<void*>(&post_hit);
  if(std::strcmp(name,"FUN_001efa68")==0)return reinterpret_cast<void*>(&ray);
  if(std::strcmp(name,"FUN_L00_002223f8")==0)return reinterpret_cast<void*>(&set_state);
  if(std::strcmp(name,"FUN_L00_002323b8")==0)return reinterpret_cast<void*>(&animation);
  if(std::strcmp(name,"FUN_L00_0025f090")==0)return reinterpret_cast<void*>(&burst);
  return nullptr;
 }
 static const openrac_api* prepare(Fixture* fixture){
  live=fixture;auto& api=fixture->api;api.memory=fixture->ram.data();api.loaded_level=loaded;
  api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.function=function;
  api.setting_bool=setting;api.log=log;api.stack_push=stack_push;api.stack_pop=stack_pop;
  return &api;
 }
 Fixture():ammunition(prepare(this)),rotor(&api,arena),world(&api,nullptr,ammunition,rotor,arena){
  actor(body,0,{0,0,0},1);actor(weapon,180,{0,0,1},2);
  put(hero+0x2080,body);put(hero+0x1090,weapon);put(hero+0x10b8,18);put(hero+0x10b4,2);
  put(hero+0x22a8,4);put(hero+0x2084,0);put(hero+0x80,Vec3{});put(hero+0x98,0.f);
  put(0x15f5c4,0);put(0x15ed7c,1.f/60);put(0x15ff18,pool);put(0x15ff20,pool);
  arena.reserve(0x1000000,16384);world.reset();
 }
 Target actor(unsigned at,unsigned klass,Vec3 centre,unsigned stamp){
  put(at+0x10,centre);put(at,centre*1024.f);put(at+12,358.4f);
  put<unsigned char>(at+0x20,0);put<unsigned short>(at+0xa6,static_cast<unsigned short>(klass));
  put(at+0x78,0x80000u+(at&0xffff));put(at+0x38,stamp);put(at+0x94,7u);put(at+0x98,0u);
  return {at,get<unsigned>(at+0x78),klass,stamp};
 }
 Target enemy(unsigned index,unsigned klass,Vec3 centre){
  const auto at=pool+index*256;put(0x15ff20,std::max(get<unsigned>(0x15ff20),at+256));return actor(at,klass,centre,index+10);
 }
 Frame frame()const{Frame f;f.epoch=1;f.equipped=f.alive=f.playing=f.grounded=true;return f;}
 void launch(Target projectile,bool golden=true,float charge=1,float multiplier=1){
  if(world.flying(projectile))world.discarded(projectile); // Explicitly end a prior synthetic fixture shot.
  world.observe_frame(frame());world.set_golden(golden);world.set_damage_multiplier(multiplier);world.launched(projectile,1,charge);
 }
 ImpactContext impact(Vec3 point={0,0,0})const{return {ImpactReason::Collision,point,{},std::nullopt,1};}
 const Packet* packet(unsigned target)const{for(const auto& p:packets)if(p.target==target)return &p;return nullptr;}
 template<class T>static T field(const Packet& p,unsigned offset){T value{};std::memcpy(&value,p.payload.data()+offset,sizeof value);return value;}
};
}
int main(){
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});const auto boss=f.enemy(1,1106,{2,0,0});
  const auto crate=f.enemy(2,500,{1,1,0});const auto npc=f.enemy(3,13,{1,0,0});
  const auto enemy=f.enemy(4,573,{-1,0,0});const auto untouched=f.ram;
  f.launch(projectile);f.world.impact_result(projectile,f.impact());
  require(f.burst_calls==1&&f.world.golden_explosions()==1&&f.packets.size()==4,"one FX burst and one packet for each combat/crate actor");
  require(f.packet(boss.address)&&f.packet(crate.address)&&f.packet(enemy.address)&&!f.packet(npc.address)&&!f.packet(body),"native boss/crates included, NPC and player excluded");
  require(near(f.burst_scale,3)&&near(f.burst_flash,16),"full hold uses large native visual profile");
  for(const auto& p:f.packets){
   require(Fixture::field<unsigned short>(p,0x1a)==2017,"all Golden hits identify logical Revolverator for XP");
   require(Fixture::field<unsigned>(p,0x10)==weapon&&Fixture::field<unsigned>(p,0x14)==0x830000,"native source and explosive flags supplied");
   require(Fixture::field<unsigned char>(p,0x18)==2&&Fixture::field<unsigned char>(p,0x19)==1,"native blast reaction profile preserved");
   require(Fixture::field<unsigned>(p,0x20)==0x830000&&Fixture::field<float>(p,12)==5627.925f,"native radial packet marker and flag copied");
   require(finite(Fixture::field<Vec3>(p,0))&&Fixture::field<float>(p,0x1c)>0,"finite radial impulse and positive damage");
  }
  require(std::equal(f.ram.begin(),f.ram.begin()+stack_base,untouched.begin()),"explosion never writes HP, native death, inventory or world RAM outside temporary SDK stack");
  f.world.impact_result(projectile,f.impact());require(f.burst_calls==1&&f.packets.size()==4,"duplicate impact is inert");
  require(f.allocations==0&&f.stack_cursor==stack_base,"no actor/arena allocation and every temporary guest stack restored");
 }
 {
  Fixture f;auto projectile=f.enemy(0,459,{0,0,0});f.enemy(1,573,{1,0,0});
  f.launch(projectile);auto result=f.impact();result.reason=ImpactReason::FlightTimeout;f.world.impact_result(projectile,result);
  require(f.burst_calls==0&&f.packets.size()==1&&Fixture::field<unsigned>(f.packets[0],0x14)==0x30000,"timeout retains only legacy direct damage, no Golden blast");
  f.packets.clear();f.launch(projectile);f.world.cancelled();f.world.impact_result(projectile,f.impact());
  require(f.burst_calls==0&&f.packets.empty(),"cancel never detonates or damages");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});const auto other=f.enemy(1,573,{1,0,0});
  f.launch(projectile,false,1,2);f.world.set_golden(true);f.world.set_damage_multiplier(16);
  auto hit=f.impact();hit.other=other;hit.charge=.5f;f.world.impact_result(projectile,hit);
  require(f.burst_calls==0&&f.packets.size()==2,"base projectile cannot upgrade while flying");
  require(near(Fixture::field<float>(*f.packet(projectile.address),0x1c),11)&&near(Fixture::field<float>(*f.packet(other.address),0x1c),9),"base impact formula unchanged and launch multiplier retained");
  require(Fixture::field<unsigned char>(f.packets[0],0x19)==3,"base hit reaction retained");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});f.launch(projectile,true,.25f,2);
  f.world.set_golden(false);f.world.set_damage_multiplier(16);f.put(weapon+0x38,999u);
  f.put(hero+0x10b8,15);f.world.set_active(false);auto frame=f.frame();frame.equipped=false;f.world.observe_frame(frame);
  f.world.impact_result(projectile,f.impact());
  require(f.burst_calls==1&&near(Fixture::field<float>(f.packets[0],0x1c),20),"Golden variant and charge/multiplier survive weapon switch");
  require(Fixture::field<unsigned>(f.packets[0],0x10)==body&&Fixture::field<unsigned short>(f.packets[0],0x1a)==2017,"recycled held moby falls back to original hero with explicit custom origin");
  require(!f.world.release_recoil_active(),"switch cancels only hero recoil");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});f.launch(projectile);
  f.put(weapon+0x38,999u);f.put(body+0x38,998u);f.world.impact_result(projectile,f.impact());
  require(f.burst_calls==1&&f.packets.empty(),"stale weapon and stale hero never used as damage pointers");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});const auto visible=f.enemy(1,573,{1,0,0});
  const auto blocked=f.enemy(2,573,{4,0,0});const auto crate=f.enemy(3,500,{2,0,0});f.wall_x=2.5f;
  f.launch(projectile);f.world.impact_result(projectile,f.impact());
  require(f.packet(visible.address)&&f.packet(crate.address)&&!f.packet(blocked.address)&&f.packets.size()==3,"radial damage respects native wall LOS");
 }
 for(unsigned fail:{16u,32u,48u}){
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});f.enemy(1,573,{1,0,0});f.launch(projectile);f.stack_fail_size=fail;
  f.world.impact_result(projectile,f.impact());
  if(fail==16)require(f.burst_calls==0&&f.world.golden_explosions()==0&&f.packets.size()==2,"FX allocation failure cannot count fake visuals or corrupt damage");
  if(fail==32)require(f.burst_calls==1&&f.packets.size()==1&&f.packets[0].target==projectile.address,"LOS allocation failure is blocked for area targets");
  if(fail==48)require(f.burst_calls==1&&f.packets.empty()&&f.world.golden_hits()==0,"hit allocation failure cannot count fake accepted packets");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});const auto other=f.enemy(1,573,{1,0,0});
  f.launch(projectile);f.on_burst=[&]{f.put(other.address+0x38,999u);f.world.impact_result(projectile,f.impact());};
  f.world.impact_result(projectile,f.impact());require(f.burst_calls==1&&f.packets.size()==1,"native FX reentry cannot detonate twice or hit a recycled planned actor");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});
  for(unsigned i=1;i<150;++i)f.enemy(i,459,{.1f+float(i)*.02f,0,0});
  f.launch(projectile);f.world.impact_result(projectile,f.impact());
  require(f.packets.size()==GoldenBlastPlan::maximum_hits,"crowd cannot overflow native64 hit ring");
  std::set<unsigned> addresses;for(const auto& p:f.packets)addresses.insert(p.target);
  require(addresses.size()==f.packets.size()&&f.packet(projectile.address),"crowd keeps unique primary and nearest actors");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});const auto boss=f.enemy(1,1106,{1.6f,0,.5f});
  require(!f.world.valid(boss),"boss is never capturable");
  f.launch(projectile);const auto hit=f.world.sweep(projectile,{0,0,.5f},{2,0,.5f});
  require(hit&&hit->other==boss,"Golden flight collides with native boss");
  f.world.cancelled();f.launch(projectile,false);require(!f.world.sweep(projectile,{0,0,.5f},{2,0,.5f}),"base collision eligibility unchanged");
  f.launch(projectile);f.wall_x=.5f;const auto wall=f.world.sweep(projectile,{0,0,.5f},{2,0,.5f});
  require(wall&&!wall->other&&near(wall->point.x,.5f),"earlier terrain wins over actor behind it");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{1,0,.6f});
  const auto found=f.world.find_target({0,0,0},0,2.8f);require(found&&*found==projectile&&f.world.capture(projectile),"native lease fixture captures ordinary actor");
  f.launch(projectile);f.world.release(projectile,false);
  require(f.get<unsigned>(projectile.address+0x94)==7&&f.get<unsigned>(projectile.address+0x98)==0,"collision and AI restored before explosion routing");
  f.world.impact_result(projectile,f.impact());require(f.packets.size()==1,"released projectile receives one native packet");
 }
 {
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});
  for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u,0x1d20u,0x1d24u})f.put(hero+offset,2.5f);
  f.put(hero+0xe8,3.25f);f.put(hero+0xf0,Vec3{1,2,3});f.put(0x13c940+0x110,7.f);
  f.launch(projectile);require(f.world.release_recoil_active(),"base and Golden release arm immediate recoil");
  // launched already clamps; restore the native motion inputs to test hooks.
  for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u,0x1d20u,0x1d24u})f.put(hero+offset,2.5f);
  auto expected=f.ram;
  for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u,0x1d20u,0x1d24u})std::memset(expected.data()+hero+offset,0,4);
  f.world.enforce_release_recoil();require(f.ram==expected,"only seven verified hero locomotion floats may change");
  require(f.get<float>(hero+0xe8)==3.25f&&f.get<Vec3>(hero+0xf0).x==1&&f.get<float>(0x13c940+0x110)==7,"vertical physics/platform carry/right camera input preserved");
  f.put(0x15f5c4,3);auto paused=f.frame();paused.paused=true;paused.playing=false;
  for(unsigned i=0;i<90;++i)f.world.observe_frame(paused);
  require(!f.world.release_recoil_active(),"paused hook never modifies hero motion");
  f.put(0x15f5c4,0);require(f.world.release_recoil_active(),"pause leaves recoil duration intact");
  for(unsigned i=0;i<9;++i)f.world.observe_frame(f.frame());
  require(!f.world.release_recoil_active(),"nine active frames release locomotion");
  f.launch(projectile,false);require(f.world.release_recoil_active(),"base weapon receives same authorized recoil");
  auto switched=f.frame();switched.equipped=false;f.world.observe_frame(switched);f.world.observe_frame(f.frame());
  require(!f.world.release_recoil_active(),"equip switch cancels hold permanently");
  f.launch(projectile);f.put(hero+0x10b8,15);f.world.enforce_release_recoil();f.put(hero+0x10b8,18);
  require(!f.world.release_recoil_active(),"live hook equip switch cannot revive recoil before next frame");
  f.put<unsigned short>(body+0xa6,149);f.launch(projectile);require(!f.world.release_recoil_active(),"recoil never immobilizes a non-Ratchet hero avatar");
 }
 {
  Fixture f;const unsigned record=0x1d00000;
  f.put(record+0x20,body);f.put(record+0x24,0x30000u);f.put<unsigned short>(record+0x2a,0);
  require(f.world.blocks_native_hit(record),"ordinary carrier swing suppressed before capture");
  f.put<unsigned short>(record+0x2a,2017);require(!f.world.blocks_native_hit(record),"body fallback with explicit custom origin is never suppressed as Walloper");
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{0,0,0}),b=f.enemy(1,573,{30,0,0}),c=f.enemy(2,459,{60,0,0});
  f.launch(a,true,.1f,1);f.launch(b,true,.9f,2);f.launch(c,false,.4f,3);
  require(f.world.flight_count()==3&&f.world.flying(a)&&f.world.flying(b)&&f.world.flying(c),"three shots own distinct bounded native entries");
  f.world.set_golden(false);f.world.set_damage_multiplier(16);
  f.world.impact_result(b,f.impact({30,0,0}));f.world.impact_result(a,f.impact());f.world.impact_result(c,f.impact({60,0,0}));
  require(f.packets.size()==3&&f.burst_calls==2&&f.world.flight_count()==0,"out-of-order impacts retain two Golden shots and one base shot");
  require(near(Fixture::field<float>(*f.packet(a.address),0x1c),8.8f)&&near(Fixture::field<float>(*f.packet(b.address),0x1c),30.4f)&&near(Fixture::field<float>(*f.packet(c.address),0x1c),24),"each shot preserves its own variant, charge and multiplier");
  require(near(f.burst_sizes[0],2.82f)&&near(f.burst_sizes[1],1.38f),"each Golden impact preserves its own visible scale");
  f.world.impact_result(a,f.impact());f.world.impact_result(b,f.impact({30,0,0}));require(f.packets.size()==3&&f.burst_calls==2,"duplicate per-target impacts cannot replay damage or FX");
 }
 {
  Fixture f;std::array<Target,maximum_flights+1> targets{};
  for(unsigned i=0;i<targets.size();++i)targets[i]=f.enemy(i,459,{float(i)*10,0,0});
  for(unsigned i=0;i<maximum_flights;++i)f.launch(targets[i],i%2==0,.1f*float(i),1);
  require(f.world.flight_count()==maximum_flights&&!f.world.can_launch(),"native pool admits eight independent shots");
  f.world.launched(targets.back(),1,1);f.world.launched(targets[0],1,1);
  require(f.world.flight_count()==maximum_flights,"full or duplicate launch cannot overwrite an older shot");
  f.world.discarded(targets[2]);require(f.world.flight_count()==maximum_flights-1&&f.world.can_launch(),"one discarded flight frees one slot");
  f.world.launched(targets.back(),1,1);require(f.world.flight_count()==maximum_flights&&f.world.flying(targets.back())&&f.world.flying(targets[0]),"freed slot reused without replacing another shot");
  f.world.cancelled();require(f.world.flight_count()==0&&f.packets.empty()&&f.burst_calls==0,"full teardown consumes every pending variant without detonation");
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{1,0,.6f}),b=f.enemy(1,573,{1.8f,.2f,.6f});
  const auto first=f.world.find_target({0,0,0},0,2.8f);require(first&&*first==a&&f.world.capture(a),"first native capture owns its own lease");
  f.launch(a,false);require(!f.world.lease_valid()&&f.world.frozen(a.address)&&f.world.flying(a),"launch transfers carry lease to independent frozen flight");
  require(!f.world.capture(a),"currently flying enemy cannot be recaptured");
  const auto second=f.world.find_target({0,0,0},0,2.8f);require(second&&*second==b&&f.world.capture(b),"next native search skips closer flying actor and captures another");
  f.world.move(a,{10,2,4},0);const auto first_position=f.get<Vec3>(a.address+0x10);
  f.world.move(b,{0,0,4},1.2f);
  require(distance(first_position,f.get<Vec3>(a.address+0x10))<.0001f,"moving new carry cannot overwrite previous flight transform");
  require(f.world.frozen(a.address)&&f.world.frozen(b.address),"native updates frozen for both flight and current carry");
  f.world.release(a,false);f.world.impact_result(a,f.impact(first_position));
  require(f.get<unsigned>(a.address+0x94)==7&&f.get<unsigned>(a.address+0x98)==0&&f.world.frozen(b.address)&&f.world.lease_valid(),"first impact restores only its own collision while new capture remains frozen");
  f.launch(b,true,.75f,2);f.world.discarded(b);
  require(!f.world.frozen(b.address)&&f.get<unsigned>(b.address+0x94)==7&&f.get<unsigned>(b.address+0x98)==0,"discard restores only the selected lease");
  f.world.impact_result(b,f.impact());require(f.packets.size()==1&&f.burst_calls==0,"discarded projectile has no later impact");
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{1,0,.6f}),b=f.enemy(1,573,{1.8f,.2f,.6f});
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(a),"teardown first carry fixture");f.launch(a);
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(b),"teardown second carry fixture");f.launch(b);
  f.world.move(a,{8,2,3},0);const auto at=f.get<Vec3>(a.address+0x10);f.world.attack_cancelled();
  require(f.world.flight_count()==2&&f.world.frozen(a.address)&&f.world.frozen(b.address),"gesture/equip cancellation cannot retire launched leases");
  f.world.cancelled();require(f.world.flight_count()==0&&!f.world.frozen(a.address)&&!f.world.frozen(b.address),"full cancellation restores all launched leases");
  require(distance(at,f.get<Vec3>(a.address+0x10))<.0001f,"flight teardown restores native AI in place without teleporting to capture origin");
  require(f.get<unsigned>(a.address+0x94)==7&&f.get<unsigned>(a.address+0x98)==0&&f.get<unsigned>(b.address+0x94)==7&&f.get<unsigned>(b.address+0x98)==0,"all original collision and disable values restored");
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{1,0,.6f}),b=f.enemy(1,573,{1.8f,.2f,.6f});
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(a),"recycled flight lease fixture");f.launch(a);
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(b),"surviving flight lease fixture");f.launch(b);
  f.put(a.address+0x38,999u);f.put(a.address+0x94,88u);f.put(a.address+0x98,77u);f.world.discarded(a);
  require(f.get<unsigned>(a.address+0x94)==88&&f.get<unsigned>(a.address+0x98)==77&&f.world.frozen(b.address),"stale generation never restores over a recycled entity or disturbs another flight");
  f.world.discarded(b);require(f.world.flight_count()==0&&f.get<unsigned>(b.address+0x94)==7&&f.get<unsigned>(b.address+0x98)==0,"surviving generation remains independently restorable");
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{0,0,0}),b=f.enemy(1,573,{1,0,0});
  f.launch(a,true,1,1);f.launch(b,true,.5f,1);f.world.impact_result(a,f.impact());
  require(f.packets.size()==1&&f.packets[0].target==a.address&&f.world.flying(b),"one blast cannot damage or steal a sibling in-flight actor");
  f.world.impact_result(b,f.impact({1,0,0}));require(f.burst_calls==2&&f.world.flight_count()==0,"sibling Golden shot retains its later independent explosion");
 }
 for(unsigned scenario=0;scenario<4;++scenario){
  Fixture f;Target a{},b{};
  for(unsigned i=0;i<100;++i){
   const auto t=f.enemy(i,459,{(i<50?0.f:50.f)+float(i%50)*.02f,0,0});
   if(i==0)a=t;
   if(i==50)b=t;
  }
  f.launch(a);f.launch(b);f.world.impact_result(a,f.impact());f.world.impact_result(b,f.impact({50,0,0}));
  require(f.burst_calls==2&&f.packets.size()==48&&f.world.golden_hits()==48,"simultaneous crowded explosions share one48-record native budget");
  auto paused=f.frame();paused.paused=true;paused.playing=false;f.world.observe_frame(paused);
  require(f.packets.size()==48,"pause preserves deferred damage without dispatch");
  if(scenario==0){
   f.world.observe_frame(f.frame());require(f.packets.size()==96&&f.world.golden_hits()==96,"remaining burst packets dispatch next active frame without loss");
   f.world.observe_frame(f.frame());require(f.packets.size()==96,"drained damage queue cannot replay");
  }else if(scenario==1){
   f.world.cancelled();f.world.observe_frame(f.frame());require(f.packets.size()==48,"scene/death reset discards deferred packets");
  }else if(scenario==2){
   f.put(b.address+0x38,999u);f.world.observe_frame(f.frame());require(f.packets.size()==95&&!f.packet(b.address),"deferred packet token protects recycled target");
  }else{
   auto travel=f.frame();++travel.epoch;f.world.observe_frame(travel);require(f.packets.size()==48,"epoch change never dispatches old-scene queue before controller cancellation");
   f.world.cancelled();
  }
 }
 {
  Fixture f;const auto a=f.enemy(0,459,{0,0,0});f.launch(a);f.on_burst=[&]{f.world.cancelled();};
  f.world.impact_result(a,f.impact());require(f.burst_calls==1&&f.packets.empty(),"native FX reentrant full cancellation prevents subsequent damage dispatch");
 }
 std::printf("native world: %u checks passed\n",checks);
}

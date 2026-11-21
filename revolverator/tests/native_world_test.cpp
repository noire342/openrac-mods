// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_world.hpp"
#include "damage_targets.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
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
struct AudioWitness final:WeaponAudioSink {
 std::vector<WeaponAudioCue> cues;
 bool emit(WeaponAudioCue cue,float)override{cues.push_back(cue);return true;}
 unsigned count(WeaponAudioCue cue)const{return unsigned(std::count(cues.begin(),cues.end(),cue));}
};
struct Fixture;
Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 bool drop_counterattack;
 std::vector<std::array<int,2>> state_requests;
 openrac_api api{};GuestArena arena;NativeAmmunition ammunition;NativeRotor rotor;
 // Keep the flight and deferred-hit arrays off the test executable's stack.
 std::unique_ptr<NativeWorld> world_storage;
 NativeWorld& world;
 std::vector<Packet> packets;
 std::vector<openrac_hit> counterstrikes;
 std::vector<float> burst_sizes;
 unsigned stack_cursor=stack_base,stack_fail_size{},burst_calls{},allocations{},ray_calls{},crate_breaks{},crate_drops{};
 float burst_scale{},burst_flash{},wall_x=10000;
 std::optional<float> floor_z;
 Vec3 floor_normal{0,0,1};
 int current_level=1;Vec3 burst_position{};
 std::vector<std::array<unsigned,3>> gun_updates;
 std::function<void()> on_burst;
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 static unsigned relocate(unsigned at){return at;}
 static unsigned relocate_level(int,unsigned at){return at;}
 static int loaded(){return live->current_level;}
 static int setting(openrac_mod*,const char* key){return std::strcmp(key,"drop_counterattack")==0&&live->drop_counterattack;}
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
 static void update_gun(unsigned owner,unsigned gun,int joint){live->gun_updates.push_back({owner,gun,unsigned(joint)});}
 static void break_crate(unsigned target,int explosive,unsigned contact){
  require(!explosive&&!contact,"void break uses the ordinary isolated crate routine");
  ++live->crate_breaks;live->put<unsigned char>(target+0x20,3);live->put(target+0x94,0u);
 }
 static void drop_crate(unsigned){++live->crate_drops;}
 static int set_state(int state,int param){
  live->state_requests.push_back({state,param});
  live->put(hero+0x2084,state);
  live->put(hero+0x208c,state==0x20?6:0);
  live->put<unsigned char>(body+0x53,state==0x20?0x67:0);
  return 1;
 }
 static int ray(unsigned from,unsigned to,int mask,unsigned ignore,int){
  ++live->ray_calls;require(mask==0x24,"LOS uses the verified native collision mask");
  require(ignore==body||(ignore>=pool&&ignore<live->get<unsigned>(0x15ff20)),"ray ignores only a thrown actor or capturing hero");
  const auto a=live->get<Vec3>(from),b=live->get<Vec3>(to);
  if(a.x<live->wall_x&&b.x>=live->wall_x){
   const float u=(live->wall_x-a.x)/(b.x-a.x);
   live->put(0x173e60,a+(b-a)*u);live->put(0x173e58,0u);live->put(0x173e80,Vec3{-1,0,0});return 1;
  }
  if(live->floor_z&&a.z>*live->floor_z&&b.z<=*live->floor_z){
   const float u=(*live->floor_z-a.z)/(b.z-a.z);
   live->put(0x173e60,a+(b-a)*u);live->put(0x173e58,0u);live->put(0x173e80,live->floor_normal);return 1;
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
  if(std::strcmp(name,"FUN_L00_0025f090")==0)return reinterpret_cast<void*>(&burst);
  if(std::strcmp(name,"FUN_L00_002d09b8")==0)return reinterpret_cast<void*>(&break_crate);
  if(std::strcmp(name,"FUN_L00_002d0538")==0)return reinterpret_cast<void*>(&drop_crate);
  if(std::strcmp(name,"FUN_L12_0027b9c0")==0)return reinterpret_cast<void*>(&update_gun);
  return nullptr;
 }
 static const openrac_api* prepare(Fixture* fixture){
  live=fixture;auto& api=fixture->api;api.size=sizeof api;api.memory=fixture->ram.data();api.loaded_level=loaded;
  api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.function=function;
  api.setting_bool=setting;api.log=log;api.stack_push=stack_push;api.stack_pop=stack_pop;
  api.moby_slot_of=+[](unsigned address)->int{return address==body?0:address>=pool&&address<live->get<unsigned>(0x15ff20)?int(1+(address-pool)/256):-1;};
  api.moby_hit=+[](openrac_mod*,const openrac_hit* hit)->int{live->counterstrikes.push_back(*hit);return OPENRAC_HOOK_OK;};
  return &api;
 }
 explicit Fixture(bool counterattack=false)
  :drop_counterattack(counterattack),ammunition(prepare(this)),rotor(&api,arena),
   world_storage(std::make_unique<NativeWorld>(&api,reinterpret_cast<openrac_mod*>(1),ammunition,rotor,arena)),
   world(*world_storage){
  actor(body,0,{0,0,0},1);actor(weapon,180,{0,0,1},2);
  put(hero+0x2080,body);put(hero+0x1090,weapon);put(hero+0x10b8,18);put(hero+0x10b4,2);
  put(hero+0x22a8,4);put(hero+0x2084,0);put(hero+0x80,Vec3{});put(hero+0x98,0.f);
  put(0x15f5c4,0);put(0x15ed7c,1.f/60);put(0x15ff18,pool);put(0x15ff20,pool);
  arena.reserve(0x1000000,16384);world.reset();
 }
 Target actor(unsigned at,unsigned klass,Vec3 centre,unsigned stamp){
  put(at+0x10,centre);put(at,centre*1024.f);put(at+12,358.4f);
  put<unsigned char>(at+0x20,0);put<unsigned short>(at+0xa6,static_cast<unsigned short>(klass));
  put<std::uint8_t>(at+0x23,0x80);
  put(at+0x78,0x80000u+(at&0xffff));put(at+0x38,stamp);put(at+0x94,7u);put(at+0x98,0u);
  return {at,get<unsigned>(at+0x78),klass,stamp};
 }
 Target enemy(unsigned index,unsigned klass,Vec3 centre){
  const auto at=pool+index*256;put(0x15ff20,std::max(get<unsigned>(0x15ff20),at+256));return actor(at,klass,centre,index+10);
 }
 Target crate(unsigned index,unsigned klass,Vec3 centre){
  const auto t=enemy(index,klass,centre);put<unsigned char>(t.address+0x20,1);return t;
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
  Fixture f;
  constexpr unsigned moby_callback=0x74,native_callback=0x2b9500,foreign_callback=0x2b9580;
  constexpr unsigned class_record=0x100000,health=0x110000;
  const auto enemy=f.enemy(0,612,{1.3f,0,0}); // Constructorbot, absent from the old capture and blast tables.
  f.put(enemy.address+0x24,class_record);
  f.put<std::uint16_t>(class_record+0x46,5);
  f.put(enemy.address+moby_callback,native_callback);
  require(!f.world.valid(enemy),"a creature tag alone does not make an NPC capturable");
  f.put<std::uint16_t>(enemy.address+0x34,0x20);
  f.put(enemy.pvars,health);
  f.put(health,2.f);
  f.put<std::uint16_t>(enemy.address+0x34,0x21);
  require(!f.world.find_target({},0,2),"an invisible actor cannot be latched through native aim metadata");
  f.put<std::uint16_t>(enemy.address+0x34,0x20);
  f.stack_fail_size=32;
  require(!f.world.find_target({},0,2),"target acquisition requires a successful native visibility trace");
  f.stack_fail_size=0;
  require(f.world.find_target({},0,2)==enemy&&f.world.capture(enemy),"native enemy metadata admits regular enemies without class-specific hooks");
  require(f.get<unsigned>(enemy.address+moby_callback)==0,"hold suspends only the captured actor callback");
  f.world.release(enemy,true);
  require(f.get<unsigned>(enemy.address+moby_callback)==native_callback,"release restores the captured actor callback");
  require(f.world.find_target({},0,2)==enemy&&f.world.capture(enemy),"the dying enemy fixture begins with an owned lease");
  f.put(health,0.f);
  require(!f.world.valid(enemy)&&f.world.frozen(enemy.address),"death cleanup still recognizes ownership after health reaches zero");
  f.world.discarded(enemy);
  require(f.get<unsigned>(enemy.address+moby_callback)==native_callback&&!f.world.frozen(enemy.address),
          "death cleanup releases the original actor callback and lease");
  f.put(health,2.f);
  f.launch(enemy);
  f.world.impact_result(enemy,f.impact());
  require(f.packet(enemy.address)!=nullptr,"a Golden throw damages a regular enemy admitted by native aim metadata");
  require(f.world.find_target({},0,2)==enemy&&f.world.capture(enemy),"released regular enemies can be captured again");
  f.put(enemy.address+moby_callback,foreign_callback);
  f.world.release(enemy,true);
  require(f.get<unsigned>(enemy.address+moby_callback)==foreign_callback,"release preserves a newer actor callback");
  require(f.world.find_target({},0,2)==enemy&&f.world.capture(enemy),"the same actor can be captured with its new callback");
  const auto boss=f.actor(enemy.address,1051,{1.3f,0,0},80);
  f.put(boss.address+0x34,std::uint16_t{0x20});
  f.put(boss.address+moby_callback,native_callback);
  f.world.discarded(enemy);
  require(!f.world.valid(boss)&&f.get<unsigned>(boss.address+moby_callback)==native_callback,
          "capture exceptions win over native aim metadata and pool reuse is untouched");
  for(const unsigned klass:{36u,282u,455u,541u,568u,668u,857u,871u,1632u,1906u}){
   const auto scripted=f.actor(enemy.address,klass,{1.3f,0,0},80+klass);
   require(!f.world.valid(scripted),"friendly NPCs, linked actors and scripted props remain uncapturable even with aim metadata");
  }
  const auto player=f.actor(enemy.address,0,{1.3f,0,0},2000);
  f.put(hero+0x2080,player.address);
  require(!f.world.valid(player),"the player cannot be considered an enemy through aim metadata");
  require(capturable_class(hoven_anklebiter_class),"the Hoven anklebiter is capturable");
 }
 {
  Fixture f;f.current_level=12;f.world.reset();
  const auto trooper=f.enemy(0,hoven_blarg_trooper_class,{1,0,.6f});
  f.enemy(1,499,{1,.5f,.6f});
  const unsigned gun=pool+256;
  f.put(trooper.pvars+0x248,gun);
  f.put(gun+0x38,trooper.stamp);
  require(f.world.find_target({},0,2)==trooper&&f.world.capture(trooper),"Hoven Trooper acquires its original attached gun");
  f.world.move(trooper,{1,2,3},0);
  f.launch(trooper,false);
  f.world.move(trooper,{2,3,4},0);
  require(f.gun_updates==std::vector<std::array<unsigned,3>>(2,{trooper.address,gun,1}),
          "carry and flight drive the original gun through the Trooper's native joint");
  f.put(gun+0x38,trooper.stamp+1);
  f.world.move(trooper,{3,4,5},0);
  f.world.release(trooper,true);
  require(f.gun_updates.size()==2,"a recycled gun slot is untouched by flight and teardown");
 }
 {
  Fixture f;auto frame=f.frame();f.world.set_active(true);f.world.observe_frame(frame);
  f.put(hero+0x2084,0x20);
  f.world.drill_overheat();
  f.world.observe_frame(frame);
  require(f.counterstrikes.empty(),"the strike must finish before the burn hits Ratchet");
  f.put(hero+0x2084,0);
  frame.paused=true;f.world.observe_frame(frame);
  require(f.counterstrikes.empty(),"a pending burn waits through pause");
  frame.paused=false;f.world.observe_frame(frame);
  require(f.counterstrikes.size()==1,"overheat is posted as a single native hit");
  const auto& hit=f.counterstrikes[0];
  require(hit.victim==0&&hit.attacker==-1&&hit.mask==1&&hit.damage==1.f,
          "overheat targets only Ratchet for one point of environmental damage");
  f.world.observe_frame(frame);
  require(f.counterstrikes.size()==1,"the heat event is consumed exactly once");
  f.put(hero+0x2084,0x20);
  f.world.drill_overheat();
  for(int tick=0;tick<60;++tick)f.world.observe_frame(frame);
  f.put(hero+0x2084,0);f.world.observe_frame(frame);
  require(f.counterstrikes.size()==1,"interruptions cannot leave a burn waiting indefinitely");
 }
 {
  Fixture f;f.world.enable_crate_capture();
  const Vec3 origin{1,0,20};
  const auto crate=f.crate(0,501,origin),orb=f.enemy(1,806,origin);
  f.put<unsigned char>(orb.address+0x20,1);f.put(orb.pvars+0x0c,crate.address);
  f.put(crate.address+0xe0,Vec3{0,1,0});
  require(f.world.find_target({0,0,20},0,2.8f)==crate&&f.world.capture(crate),"Nanotech fixture is held by the Rotatore");
  constexpr unsigned particles=0x120000;
  f.put(0x1600ac,particles);f.put(0x1600b4,4);
  const Vec3 offset{0,.12f,.56f};
  for(unsigned i=0;i<5;++i){
   const auto part=particles+i*64;f.put<unsigned char>(part,62);
   f.put<short>(part+0x22,i==2?1:2);f.put(part+0x24,i==3?body:crate.address);
   f.put(part+0x30,offset);f.put(part+0x10,origin+offset);
  }
  f.put<signed char>(particles+4*64+1,-1);
  f.world.update_nanotech(orb.address,+[](unsigned m){
   const auto source=live->get<unsigned>(live->get<unsigned>(m+0x78)+0x0c);
   live->put(m+0x10,live->get<Vec3>(source+0x10));
  });
  const Vec3 adjusted=origin+Vec3{0,.5f,-.5f};
  require(length_squared(f.get<Vec3>(crate.address+0x10)-origin)<.00001f,"temporary orb sampling restores the held crate");
  for(unsigned i=0;i<2;++i){
   const auto part=particles+i*64;
   require(f.get<unsigned>(part+0x24)==orb.address&&length_squared(f.get<Vec3>(part+0x10)-(adjusted+offset))<.00001f,
           "live attached trails switch immediately to the adjusted orb coordinate frame");
  }
  for(unsigned i=2;i<5;++i){
   const auto part=particles+i*64;
   require(f.get<unsigned>(part+0x24)==(i==3?body:crate.address)&&
           length_squared(f.get<Vec3>(part+0x10)-(origin+offset))<.00001f,
           "persistent slots, foreign trails and retired particles are untouched");
  }
 }
 {
  Fixture f;f.world.enable_crate_capture();f.floor_z=20.f;
  const auto crate=f.crate(0,500,{1,0,21});
  const auto upper=f.crate(1,500,{1,0,22});
  f.put(crate.pvars+0xa0,upper.address);f.put(upper.pvars+0xa4,crate.address);
  auto frame=f.frame();frame.position={0,0,20};f.world.observe_frame(frame);
  require(f.world.find_target({0,0,21},0,2.8f)==crate&&f.world.capture(crate),"stack crate is captured before its neighbour settles");
  f.world.move(crate,{0,0,23},0);
  const Vec3 vacated{1,0,21};f.put(upper.address+0x10,vacated);f.put(upper.address,vacated*1024.f);
  f.world.release(crate,true);
  const auto placed=f.get<Vec3>(crate.address+0x10);
  require(near(placed.z,20.375f)&&length_squared(placed-vacated)>.65f*.65f&&
          f.get<unsigned>(crate.address+0x94)==7u&&near(f.get<Vec3>(upper.address+0x10).z,vacated.z),
          "cancelling a carry does not restore a crate inside its settled stack neighbour");
 }
 {
  Fixture f(true);
  const auto lower=f.crate(0,500,{1.1f,0,20});
  const auto crate=f.crate(1,511,{.8f,0,21});
  const auto upper=f.crate(2,505,{1.1f,0,22});
  f.put(lower.pvars+0xa0,crate.address);f.put(crate.pvars+0xa4,lower.address);
  f.put(crate.pvars+0xa0,upper.address);f.put(upper.pvars+0xa4,crate.address);
  require(!f.world.find_target({0,0,21},0,2.8f),"crate capture requires native capability");
  require(f.world.enable_crate_capture(),"configured native crate functions enable capture");
  require(!f.world.valid(upper)&&!f.world.capture(upper),"explosive crates stay under native impact rules");
  require(f.world.find_target({0,0,21},0,2.8f)==crate&&f.world.capture(crate),"hold captures one ordinary crate");
  require(f.world.frozen(crate.address)&&!f.world.frozen(lower.address)&&!f.world.frozen(upper.address),"only the selected crate is frozen");
  require(f.get<unsigned>(crate.pvars+0xa0)==0&&f.get<unsigned>(crate.pvars+0xa4)==0&&
          f.get<unsigned>(lower.pvars+0xa0)==upper.address&&f.get<unsigned>(upper.pvars+0xa4)==lower.address,
          "removing the middle crate preserves both reciprocal stack links");
  constexpr unsigned contact=0x70000;
  f.put(contact+0x20,body);f.put(contact+0x24,0x30000u);f.put<unsigned short>(contact+0x2a,180);
  require(!f.world.blocks_native_hit(lower,contact)&&!f.world.blocks_native_hit(upper,contact),"other crates retain the native melee hit");
  f.launch(crate,true);f.world.move(crate,{5,0,11},0);
  const auto tip=Vec3{5,0,11};
  const float root=f.get<Vec3>(crate.address+0x10).z;
  require(f.world.flight_duration(crate,1.4f)>1.4f,"a crate can fall past the ordinary enemy shot timeout");
  const auto hit=f.world.sweep(crate,tip,tip+Vec3{0,0,8-root});
  require(hit&&hit->reason==ImpactReason::Void,"collision-free crate detects the native fall height while flying");
  f.world.move(crate,hit->point,0);f.world.release(crate,false);
  require(near(f.get<Vec3>(crate.address+0x10).z,NativeCrates::void_height),"void boundary is based on crate root rather than attachment tip");
  f.world.impact_result(crate,{hit->reason,hit->point,{},{},1});
  f.world.impact_result(crate,{hit->reason,hit->point,{},{},1});
  require(f.crate_breaks==1&&f.crate_drops==1&&f.burst_calls==0&&!f.world.frozen(crate.address),"void native break and contents are each applied once with no blast");
 }
 {
  Fixture f(true);f.world.enable_crate_capture();f.floor_z=20.f;
  const auto crate=f.crate(0,500,{1,0,21});const auto unrelated=f.enemy(1,459,{2.3f,0,21});
  f.put(crate.pvars+0xa0,unrelated.address);
  require(f.world.find_target({0,0,21},0,2.8f)==unrelated,"a malformed crate stack cannot write a noncrate neighbour");
  f.put(crate.pvars+0xa0,0u);f.put<unsigned char>(crate.pvars+0xc8,1);
  require(f.world.find_target({0,0,21},0,2.8f)==unrelated,"scripted respawning crates stay owned by the level");
  f.put<unsigned char>(crate.pvars+0xc8,0);
  auto frame=f.frame();frame.position={0,0,20};f.world.observe_frame(frame);
  require(f.world.find_target({0,0,21},0,2.8f)==crate&&f.world.capture(crate),"ordinary crate can also be released early");
  f.world.drop(crate);
  require(!f.world.frozen(crate.address)&&f.get<unsigned>(crate.address+0x94)==7u&&f.counterstrikes.empty(),
          "undercharged crate returns to the ground without a counterstrike");
 }
 for(bool interrupted:{false,true}){
  Fixture f;
  const auto enemy=f.enemy(0,459,{1,0,.6f});
  f.world.observe_frame(f.frame());
  require(f.world.find_target({0,0,0},0,2.8f)==enemy&&f.world.capture(enemy),
          "staged throw starts with a native enemy lease");
  f.world.animate(Motion::ThrowStrike);
  require(f.state_requests==std::vector<std::array<int,2>>{{0x20,1}}&&
          f.world.throw_strike_active()&&f.world.lease_valid()&&!f.world.attack_input_ready(),
          "charged release immediately starts the actual Walloper state and owns the enemy until contact");
  if(interrupted){
   f.put<unsigned char>(body+0x52,0x67);f.put<unsigned char>(body+0x53,0x06);
   f.world.attack_cancelled();
   require(!f.world.throw_strike_active()&&f.get<int>(hero+0x2084)==0x20&&f.state_requests.size()==1,
           "a clip left in the source pose does not own a subsequent native action");
  }else{
   f.put(hero+0x198,17);require(!f.world.throw_contact_reached(),"the target stays attached through the lunge");
   f.put(hero+0x198,18);require(f.world.throw_contact_reached(),"the end of the stock forward drive allows detachment");
   const auto crate=f.enemy(1,500,{2,0,0});constexpr unsigned hit=0x70000;
   f.put(hit+0x20,body);f.put(hit+0x24,0x30000u);
   f.put<unsigned char>(hit+0x29,3);f.put<unsigned short>(hit+0x2a,2017);f.put(hit+0x2c,3.f);
   require(f.world.blocks_native_hit(crate,hit),"throw suppresses its redundant melee contact even through the render alias");
   f.put(hit+0x2c,8.f);
   require(!f.world.blocks_native_hit(crate,hit),"projectile damage survives any original-body fallback");
   f.world.launched(enemy,1.f,.6f);f.world.animate(Motion::Release);
   require(f.world.flying(enemy)&&!f.world.release_recoil_active()&&f.world.throw_strike_active(),
           "detachment leaves the native lunge free to complete");
   for(int i=0;i<12;++i)f.world.observe_frame(f.frame());
   require(f.world.flying(enemy)&&!f.world.attack_input_ready()&&f.get<int>(hero+0x2084)==0x20,
           "Circle cannot repeat until the native follow-through ends");
   Fixture::set_state(0,0);f.world.observe_frame(f.frame());
   require(!f.world.throw_strike_active()&&f.world.attack_input_ready()&&f.world.flying(enemy),
           "the native reset admits a new press while the previous flight continues");
  }
 }
 for(bool golden:{false,true})for(bool counterattack:{false,true}){
  Fixture f(counterattack);AudioWitness audio;f.world.set_audio(&audio);f.floor_z=0.f;const auto victim=f.enemy(0,459,{1,0,.6f});
  f.world.set_golden(golden);f.world.observe_frame(f.frame());
  require(f.world.find_target({0,0,0},0,2.8f)==victim&&f.world.capture(victim),"drop fixture captures an ordinary enemy");
  f.world.move(victim,{0,0,4},1);
  f.world.drop(victim);
  const auto position=f.get<Vec3>(victim.address+0x10),angles=f.get<Vec3>(victim.address+0x40);
  require(position.x>.5f&&position.x<1.8f&&near(position.y,0)&&position.z>.35f&&position.z<.4f,
          "early release puts the actor on actual ground in front of Ratchet");
  require(std::abs(std::abs(angles.z)-3.14159265f)<.0001f,"dropped enemy faces Ratchet instead of the old capture heading");
  require(!f.world.frozen(victim.address)&&!f.world.lease_valid()&&f.world.flight_count()==0,
          "drop restores native updates immediately without allocating a flight");
  require(f.get<unsigned>(victim.address+0x94)==7&&f.get<unsigned>(victim.address+0x98)==0,
          "drop restores the original collision and AI state");
  require(f.packets.empty()&&f.burst_calls==0&&f.world.golden_explosions()==0&&!f.world.release_recoil_active(),
          "drop never posts weapon damage, Golden explosions or throw recoil");
  const auto expected_hits=std::size_t(counterattack);
  require(f.counterstrikes.size()==expected_hits,"only the opt-in early drop posts a counterstrike");
  if(counterattack){
   require(f.counterstrikes[0].victim==0&&f.counterstrikes[0].attacker==1&&
          f.counterstrikes[0].mask==1&&near(f.counterstrikes[0].damage,1.f),
          "one native light-enemy melee hit names the released enemy and Ratchet");
   require(near(f.counterstrikes[0].position[2],.75f)&&near(f.counterstrikes[0].velocity[0],-.2f),
           "the native SDK receives the contact and impulse");
  }
  require(f.get<int>(hero+0x22a8)==4,"early drop never writes Ratchet's health directly");
  const auto after=f.ram;f.world.drop(victim);require(f.ram==after,"repeating an early-release callback is inert");
  require(f.counterstrikes.size()==expected_hits,"repeating a drop never posts a second counterstrike");
  require(audio.count(WeaponAudioCue::Capture)==1&&audio.count(WeaponAudioCue::Drop)==1&&
          audio.count(WeaponAudioCue::GroundContact)==0&&audio.count(WeaponAudioCue::Throw)==0&&
          audio.count(WeaponAudioCue::GoldenImpact)==0,"early release cues are distinct from throwing and landing waits for native update");
  f.world.enemy_updated(victim.address);f.world.enemy_updated(victim.address);
  require(audio.count(WeaponAudioCue::GroundContact)==1,"ground contact sounds once after a native enemy update confirms its live collision sphere");
  require(f.stack_cursor==stack_base,"ground probes restore all SDK stack temporaries");
 }
 {
  Fixture f;f.floor_z=0.f;f.wall_x=.8f;const auto victim=f.enemy(0,459,{.5f,0,.6f});
  f.world.observe_frame(f.frame());require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"blocked-front drop fixture captures");
  f.world.move(victim,{0,0,4},1);f.world.drop(victim);
  const auto position=f.get<Vec3>(victim.address+0x10);
  require(position.x>0&&position.x<f.wall_x&&position.z<.4f,"a wall chooses supported ground on Ratchet's side instead of crossing it");
 }
 {
  Fixture f;const auto victim=f.enemy(0,459,{1,0,.6f});const auto origin=f.get<Vec3>(victim.address+0x10);
  f.world.observe_frame(f.frame());require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"no-floor drop fixture captures");
  f.world.move(victim,{0,0,4},1);f.world.drop(victim);
  require(distance(origin,f.get<Vec3>(victim.address+0x10))<.0001f&&!f.world.frozen(victim.address),
          "a void falls back safely to the capture location rather than inventing ground or a projectile");
 }
 {
  Fixture f;f.floor_z=0.f;const auto victim=f.enemy(0,459,{1,0,.6f});f.world.observe_frame(f.frame());
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"stale-drop fixture captures");
  f.put(victim.address+0x38,victim.stamp+1);f.put(victim.address+0x94,88u);f.put(victim.address+0x98,77u);
  const auto before=f.ram;f.world.drop(victim);
  require(f.ram==before&&!f.world.lease_valid(),"a recycled captive is never moved or re-enabled by drop");
 }
 for(unsigned safety=0;safety<4;++safety){
  Fixture f(true);const auto victim=f.enemy(0,459,{1,0,.6f});f.world.observe_frame(f.frame());
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"counterstrike safety fixture captures");
  if(safety==0){auto frame=f.frame();frame.position={10,0,0};f.put(hero+0x80,frame.position);f.world.observe_frame(frame);}
  if(safety==1)f.api.size=offsetof(openrac_api,moby_slot_of);
  if(safety==2)f.stack_fail_size=32;
  if(safety==3)f.put<unsigned short>(body+0xa6,149);
  f.world.drop(victim);
  require(f.counterstrikes.empty()&&!f.world.frozen(victim.address)&&f.world.flight_count()==0,
          "distant fallback, missing SDK prefix, failed LOS stack or another avatar cannot receive a phantom counterstrike");
 }
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
  Fixture f;const auto projectile=f.enemy(0,459,{0,0,0});
  f.enemy(1,573,{4,0,0});const auto nearby=f.enemy(2,573,{2,0,0});
  f.launch(projectile,false);
  auto hit=f.world.sweep(projectile,{0,0,.5f},{5,0,.5f});
  require(hit&&hit->other==nearby,"Base flight contacts the nearest enemy regardless of native pool order");
  f.wall_x=1.5f;
  hit=f.world.sweep(projectile,{0,0,.5f},{5,0,.5f});
  require(hit&&!hit->other&&near(hit->point.x,1.5f),"Base flight cannot damage an enemy through terrain");
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
 for(bool golden:{false,true}){
  Fixture f;f.world.set_golden(golden);
  const auto victim=f.enemy(0,459,{1,0,.6f});
  auto motion=[&]{f.put(hero+0x194,10.f);f.put(hero+0xe0,Vec3{6,8,3.25f});};
  motion();auto expected=f.ram;f.world.enforce_carry_speed();
  require(f.ram==expected,"empty attack never slows base or Gold movement");
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"carry-speed fixture owns a real native capture lease");
  f.put(hero+0xf0,Vec3{1,2,3});f.put(0x13c940+0x110,7.f);f.put(hero+0x160,12.f);
  motion();expected=f.ram;
  const float x=3,y=4;
  std::memcpy(expected.data()+hero+0xe0,&x,4);std::memcpy(expected.data()+hero+0xe4,&y,4);
  f.world.enforce_carry_speed();
  require(f.ram==expected,"carrying changes only the two horizontal velocity components at half native speed");
  require(f.get<float>(hero+0xe8)==3.25f&&f.get<float>(hero+0x194)==10.f&&f.get<float>(hero+0x160)==12.f&&
          f.get<Vec3>(hero+0xf0).x==1&&f.get<float>(0x13c940+0x110)==7,"carry preserves gravity, native speed requests, platform carry and camera input");
  for(unsigned i=0;i<120;++i)f.world.enforce_carry_speed();
  require(f.ram==expected,"multiple native motion hooks cannot compound the slowdown toward zero");
  f.put(hero+0xe0,Vec3{1,2,3.25f});expected=f.ram;f.world.enforce_carry_speed();
  require(f.ram==expected,"native acceleration below the carry ceiling is preserved");
  for(unsigned reason=0;reason<7;++reason){
   motion();
   switch(reason){
   case 0:f.put(0x15f5c4,3);break;
   case 1:f.put(0x15f5c4,5);break;
   case 2:f.put(hero+0x22a8,0);break;
   case 3:f.put(hero+0x10b8,15);break;
   case 4:f.world.set_active(false);break;
   case 5:f.put<unsigned short>(body+0xa6,149);break;
   case 6:f.put(hero+0x2084,0x3d);break;
   }
   expected=f.ram;f.world.enforce_carry_speed();
   require(f.ram==expected,"carry limit does not leak into pause, scenes, death, switched equipment or other avatars");
   f.put(0x15f5c4,0);f.put(hero+0x22a8,4);f.put(hero+0x10b8,18);f.world.set_active(true);
   f.put<unsigned short>(body+0xa6,0);f.put(hero+0x2084,0);
  }
  motion();f.put(hero+0x194,std::numeric_limits<float>::quiet_NaN());expected=f.ram;f.world.enforce_carry_speed();
  require(f.ram==expected,"malformed native speed cannot contaminate velocity");
  motion();f.launch(victim,golden);motion();expected=f.ram;f.world.enforce_carry_speed();
  require(f.world.flying(victim)&&!f.world.lease_valid()&&f.ram==expected,"launch immediately removes the carry cost while the enemy keeps flying");
 }
 {
  Fixture f;const auto victim=f.enemy(0,459,{1,0,.6f});
  require(f.world.find_target({0,0,0},0,2.8f)&&f.world.capture(victim),"cancelled carry fixture");
  f.world.cancelled();f.put(hero+0x194,10.f);f.put(hero+0xe0,Vec3{6,8,2});
  const auto expected=f.ram;f.world.enforce_carry_speed();
  require(f.ram==expected,"cancelled capture restores unrestricted movement without saved tuning to undo");
 }
 {
  Fixture f;const auto victim=f.enemy(0,459,{1,0,.6f});const unsigned record=0x1d00000;
  f.put(record+0x20,body);f.put(record+0x24,0x30000u);f.put<unsigned short>(record+0x2a,0);
  require(f.world.blocks_native_hit(victim,record),"ordinary carrier swing suppressed before capture");
  f.put<unsigned short>(record+0x2a,2017);require(!f.world.blocks_native_hit(victim,record),"body fallback with explicit custom origin is never suppressed as Walloper");
  f.put<unsigned short>(record+0x2a,0);f.put(victim.address+0x38,victim.stamp+1);
  require(!f.world.blocks_native_hit(victim,record),"stale target generations never suppress native damage");
 }
 for(bool golden:{false,true})for(unsigned klass:{253u,625u,1048u,1202u,1454u}){
  Fixture f;f.world.set_golden(golden);const auto heavy=f.enemy(0,klass,{1,0,.6f});
  const unsigned record=0x1d00000;f.put(record+0x20,body);f.put(record+0x24,0x30000u);
  const auto before=f.ram;
  require(!f.world.valid(heavy)&&!f.world.find_target({0,0,0},0,2.8f),"heavy actors are excluded from capture targeting even at point blank range");
  require(!f.world.capture(heavy)&&!f.world.frozen(heavy.address)&&!f.world.lease_valid(),"base and Gold never acquire, freeze or move a heavy actor");
  require(!f.world.blocks_native_hit(heavy,record),"heavy actors retain the original Walloper damage packet");
  require(f.ram==before,"rejected heavy capture and hit filtering preserve all native actor state");
  require(golden_damage_class(klass),"capture immunity does not grant immunity to Golden impact damage");
  const auto light=f.enemy(1,459,{2,0,.6f});
  require(f.world.find_target({0,0,0},0,2.8f)==light&&f.world.capture(light),"a closer heavy actor does not hide an eligible light enemy");
 }
 for(bool golden:{false,true}){
  Fixture f;f.world.set_golden(golden);const auto commander=f.enemy(0,1371,{1,0,.6f});
  const unsigned record=0x1d00000;f.put(record+0x20,body);f.put(record+0x24,0x30000u);
  require(f.world.valid(commander)&&f.world.blocks_native_hit(commander,record),"Blarg commanders remain capturable and protected from the carrier's pre-capture hit");
  require(f.world.find_target({0,0,0},0,2.8f)==commander&&f.world.capture(commander),"base and Gold can select and capture a Blarg commander");
  require(f.world.frozen(commander.address)&&f.world.lease_valid(),"a captured commander receives the ordinary owned carry lease");
  f.world.release(commander,true);
  require(!f.world.frozen(commander.address)&&!f.world.lease_valid()&&f.get<unsigned>(commander.address+0x94)==7&&f.get<unsigned>(commander.address+0x98)==0,
          "releasing a commander restores its native collision and AI flags");
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

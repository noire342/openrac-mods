// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_world.hpp"
#include "builtin_catalog.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <vector>
using namespace salvage;
namespace {
unsigned checks;void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"salvage native world: %s\n",message);std::exit(1);}}
struct Fixture;Fixture* live;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};GuestArena arena;Catalog catalog;World fragments;NativeWorld world;
 unsigned creates{},removes{},queries{},create_failures{},next_actor=0x41000;int level=1;
 std::vector<unsigned> native_slots;
 bool split_collision_output=false,ray_floor=false;unsigned ray_owner{},interior_relocations{};
 unsigned stack_address=0x1e00000;Vec3 last_ray_from{},last_ray_to{};
 unsigned collision_output()const{return split_collision_output?0x50000u:0x173f40u;}
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 static unsigned relocate(unsigned x){return x;}
 static unsigned relocate_level(int,unsigned x){
  if(live->split_collision_output){
   if(x==0x173f40)return live->collision_output();
   // Relocation pairs are symbol-based, not guaranteed affine inside a struct.
   // Only relocating the root and then adding field offsets is correct.
   if(x>0x173f40&&x<0x173fc0){++live->interior_relocations;return 0x60000+(x-0x173f40);}
  }
  return x;
 }
 static int loaded(){return live->level;}
 static void log(openrac_mod*,int,const char*){}
 static unsigned stack(unsigned,unsigned* saved){*saved=0;return live->stack_address;}static void pop(unsigned){}
 static int ray(unsigned from_at,unsigned to_at,int mask,unsigned,int){
  ++live->queries;live->last_ray_from=live->get<Vec3>(from_at);live->last_ray_to=live->get<Vec3>(to_at);
  if(mask&0x20)return 0;const auto out=live->collision_output();
  if(live->ray_owner){live->put(out+0x18,live->ray_owner);return 1;}
  const auto from=live->get<Vec3>(from_at),to=live->get<Vec3>(to_at);
  if(live->ray_floor&&from.z>0&&to.z<0){
   const float t=from.z/(from.z-to.z);live->put(out+0x20,from+(to-from)*t);live->put(out+0x40,Vec3{0,0,1});live->put(out+0x18,0u);return 1;
  }
  return 0;
 }
 static int sphere(float radius,unsigned at,int mask,unsigned){
  if(mask&0x20)return 0;
  ++live->queries;const auto centre=live->get<Vec3>(at);if(centre.z>=radius)return 0;
  const auto out=live->collision_output();live->put(out+0x30,Vec3{centre.x,centre.y,radius});live->put(out+0x40,Vec3{0,0,1});return 1;
 }
 static int capsule(float,float,unsigned,int,unsigned){return 0;}
 static void matrix(unsigned at){
  // Static fragment sequence bounds, transformed by the native draw scale.
  const auto pc=live->get<unsigned>(at+0x24);const auto seq=live->get<unsigned>(pc+0x48);
  const auto sphere=live->get<std::array<float,4>>(seq);const auto pos=live->get<Vec3>(at+0x10);const auto scale=live->get<float>(at+0x2c);
  const auto rows=live->get<std::array<float,12>>(at+0xc0);
  live->put(at,std::array<float,4>{{pos.x*1024+(rows[0]*sphere[0]+rows[4]*sphere[1]+rows[8]*sphere[2])*scale,pos.y*1024+(rows[1]*sphere[0]+rows[5]*sphere[1]+rows[9]*sphere[2])*scale,pos.z*1024+(rows[2]*sphere[0]+rows[6]*sphere[1]+rows[10]*sphere[2])*scale,sphere[3]*scale}});
 }
 static void basis(unsigned at,unsigned angles){const auto r=live->get<Vec3>(angles);const float sx=std::sin(r.x),cx=std::cos(r.x),sy=std::sin(r.y),cy=std::cos(r.y),sz=std::sin(r.z),cz=std::cos(r.z);live->put(at,std::array<float,12>{{cz*cy,sz*cy,-sy,0,cz*sy*sx-sz*cx,sz*sy*sx+cz*cx,cy*sx,0,cz*sy*cx+sz*sx,sz*sy*cx-cz*sx,cy*cx,0}});}
 static void remove(unsigned at){++live->removes;live->put<unsigned char>(at+0x20,0xfe);}
 static unsigned create(int klass){
  ++live->creates;if(live->create_failures){--live->create_failures;return 0;}
  if(!live->native_slots.empty()){
   for(const auto slot:live->native_slots)if(live->get<unsigned char>(slot+0x20)>=0xfd){live->actor(slot,unsigned(klass),{});matrix(slot);return slot;}
   return 0;
  }
  const auto at=live->next_actor;live->next_actor+=256;live->actor(at,unsigned(klass),{});matrix(at);return at;
 }
 static void* function(const char* name){
  if(!std::strcmp(name,"func_001EFE10"))return reinterpret_cast<void*>(ray);
  if(!std::strcmp(name,"func_L00_001F10E0"))return reinterpret_cast<void*>(sphere);
  if(!std::strcmp(name,"func_L00_001F1D20"))return reinterpret_cast<void*>(capsule);
  if(!std::strcmp(name,"func_L00_00251E30"))return reinterpret_cast<void*>(matrix);
  if(!std::strcmp(name,"func_001FA1F8"))return reinterpret_cast<void*>(basis);
  if(!std::strcmp(name,"func_0020D678"))return reinterpret_cast<void*>(remove);
  if(!std::strcmp(name,"func_0020D348"))return reinterpret_cast<void*>(create);return nullptr;
 }
 static const openrac_api* prepare(Fixture* f){live=f;auto& a=f->api;a.memory=f->ram.data();a.loaded_level=loaded;a.relocate_data=relocate;a.relocate_level_data=relocate_level;a.function=function;a.stack_push=stack;a.stack_pop=pop;a.log=log;return &a;}
 Fixture():world(prepare(this),nullptr,arena,catalog,fragments){
  arena.reserve(0x1000000,0x80000);world.after_load(1);
  put(NativeMemory::Hero+0x2080,0x30000u);put(NativeMemory::Hero+0x22a8,4);put(NativeMemory::Hero+0x80,Vec3{10,10,1});
  put(NativeMemory::Delta,1.f/60);put(NativeMemory::MobyBegin,0x40000u);put(NativeMemory::MobyEnd,0x40200u);
  put(0x90024,1.f);put(0x90048,0x91000u);put(0x91000,std::array<float,4>{{0,0,0,204.8f}});
  put<unsigned char>(0x1b3e40+0x6c8,1);put(0x160000,2);put<unsigned short>(0x1b3c80+2,0x6c8);put(0x1b3580+4,0x90000u);
  actor(0x40000,459,{12,10,1});actor(0x40100,0x6c8,{12,10,1});
 }
 void actor(unsigned at,unsigned klass,Vec3 pos){
  std::memset(ram.data()+at,0,256); // InitMobyInstance clears every recycled slot.
  put<unsigned char>(at+0x20,1);put<unsigned short>(at+0xa6,static_cast<unsigned short>(klass));put(at+0x10,pos);
  put(at,std::array<float,4>{{pos.x*1024,pos.y*1024,pos.z*1024,204.8f}});put(at+0x24,0x90000u);put(at+0x78,0x92000u+(at&0xfff));
  put(at+0x2c,1.f);basis(at+0xc0,0);
 }
 void pool(unsigned base,unsigned count){
  native_slots.clear();for(unsigned i=0;i<count;++i){const auto at=base+i*256;native_slots.push_back(at);put<unsigned char>(at+0x20,0xfe);}
 }
};
bool near(Vec3 a,Vec3 b){return length(a-b)<.0001f;}
}
int main(){
 {
  Fixture f;f.put(NativeMemory::Hero+0x22a8,0);
  const Vec3 source_origin{20,30,20},local_offset{8,-6,4},rotation{.45f,-.35f,1.f};const float scale=1.75f;
  f.put(0x40110,source_origin);f.put(0x40140,rotation);f.put(0x4012c,scale);
  f.put(0x91000,std::array<float,4>{{local_offset.x*1024/scale,local_offset.y*1024/scale,local_offset.z*1024/scale,.35f*1024/scale}});
  Fixture::basis(0x401c0,0x40140);Fixture::matrix(0x40100);const auto native_center=f.get<Vec3>(0x40100)*(1.f/1024);
  check(length(native_center-source_origin)>5&&finite(native_center),"fixture has a legitimate finite rotated native mesh center farther than five units from its pivot");
  f.put(0x40100,std::array<float,4>{{777,888,999,1024}}); // stale cache must be rebuilt from real sequence bounds
  check(f.world.adopt(0x40000,0x40100),"large-offset original native fragment is adopted after basis and matrix rebuild");const auto id=f.fragments.all().begin()->first;
  check(near(f.fragments.find(id)->position,native_center),"finite native sequence sphere center above five units is preserved as logical physical center");
  check(near(f.get<Vec3>(0x40110),source_origin)&&near(f.get<Vec3>(0x40100)*(1.f/1024),native_center),"native visual pivot stays at its original position while the bounds center matches the logical body");
  const auto collision=f.get<unsigned>(0x40194);
  check(near(f.get<Vec3>(collision+0x20)*(scale/1024),local_offset),"native collider stores the true unrotated local offset rather than a fabricated zero");
  f.world.before_load(1);f.arena.reserve(0x1100000,0x80000);f.world.after_load(1);f.world.frame();const auto actor=f.world.native_actor(id);
  check(actor&&near(f.get<Vec3>(actor+0x10),source_origin)&&near(f.get<Vec3>(actor)*(1.f/1024),native_center),"same-visit rematerialization preserves the large offset and original model pivot");
  check(near(f.get<Vec3>(f.get<unsigned>(actor+0x94)+0x20)*(scale/1024),local_offset),"rematerialized collider retains the same native local offset");
 }
 {
  Fixture f;f.put(NativeMemory::Hero+0x22a8,0);const Vec3 origin=f.get<Vec3>(0x40110);
  f.put(0x91000,std::array<float,4>{{std::numeric_limits<float>::quiet_NaN(),0,0,204.8f}});
  check(f.world.adopt(0x40000,0x40100),"malformed native cached center retains the existing safe fallback");
  check(near(f.fragments.all().begin()->second.position,origin),"nonfinite native sphere center never reaches the logical world");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"real mechanical-source fragment is adopted");
  check(f.fragments.all().size()==1&&f.catalog.entries().size()==1,"one catalog type and one logical world instance");
  const auto id=f.fragments.all().begin()->first;const auto collider=f.get<unsigned>(0x40194);
  check(f.arena.owns(collider,48)&&f.get<int>(collider+4)==32&&f.get<unsigned char>(collider+16)==1,"fragment owns a native sphere collision primitive");
  check(f.get<unsigned short>(collider+18)==0x8003&&f.get<unsigned>(0x40198)==0,"native collision terminator and query groups are valid");
  check(f.world.owns(0x40100)&&f.world.native_actor(id)==0x40100,"native callback routes only owned actor");
  check(f.world.adopt(0x40000,0x40100)&&f.fragments.all().size()==1,"duplicate spawn observation is idempotent");
  for(unsigned i=0;i<900;++i)f.world.frame();
  check(f.fragments.all().size()==1&&f.world.owns(0x40100)&&f.removes==0,"fragment outlives native timed explosion without deleting or hiding the actor");
  const auto* fragment=f.fragments.find(id);check(fragment&&fragment->position.z>=.19f&&fragment->sleeping,"fragment falls onto collision surface and sleeps");
  check(f.get<unsigned char>(0x40123)==0x80&&f.get<unsigned>(0x40194)==collider,"native visual alpha and solid collider remain present");
  const auto count=f.fragments.all().size();f.world.before_load(1);check(f.fragments.all().size()==count,"same-planet death unload preserves logical visit records");
  check(f.removes==1,"old native handles retire before another level heap is used");
  f.arena.reserve(0x1100000,0x80000);f.world.after_load(1);check(f.fragments.all().size()==count,"same-planet reload retains records for rematerialization");
  f.world.explicit_load();check(f.fragments.all().empty(),"explicit native save restore clears the visit");
 }
 {
  Fixture f;f.put<unsigned short>(0x400a6,35);const auto expected=f.ram;
  check(!f.world.adopt(0x40000,0x40100)&&f.ram==expected&&f.fragments.all().empty(),"organic debris is never converted or modified");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"collection retirement fixture");const auto id=f.fragments.all().begin()->first;
  f.fragments.erase(id);f.world.retire_collected();check(f.removes==1&&!f.world.owns(0x40100),"logical successful collection retires exactly its physical actor");
  check(f.get<unsigned>(0x40174)==0&&f.world.owns_callback(0x40100),"a final queued relay from a retired actor is absorbed without entering the real level loader");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"stale actor fixture");f.put(0x40174,123u);f.put(0x40194,456u);f.world.before_load(2);
  check(f.removes==0&&f.get<unsigned>(0x40194)==456&&f.fragments.all().empty(),"recycled actor is untouched and planet travel ends old visit");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"native pool exhaustion fixture");
  const auto id=f.fragments.all().begin()->first;f.world.before_load(1);f.arena.reserve(0x1100000,0x80000);f.world.after_load(1);
  f.create_failures=5;for(unsigned i=0;i<5;++i)f.world.frame();
  check(f.creates==5&&f.fragments.all().size()==1&&f.world.native_actor(id)==0,"repeated native pool exhaustion preserves logical component identity");
  check(!f.world.visible(*f.fragments.find(id)),"unmaterialized record cannot be collected through an invisible native actor");
  f.world.frame();check(f.creates==6&&f.world.native_actor(id)!=0&&f.fragments.all().size()==1,"native pool recovery materializes the original instance exactly once");
  f.world.frame();check(f.creates==6,"materialized instance does not allocate again every frame");
 }
 {
  Fixture f;f.arena.reserve(0x1000000,48);check(f.world.adopt(0x40000,0x40100),"one-collider budget fixture");
  const auto first=f.fragments.all().begin()->first;f.actor(0x40200,0x6c8,{13,10,1});
  check(f.world.adopt(0x40000,0x40200),"collider exhaustion still accepts the recoverable component");
  const auto second=f.fragments.all().rbegin()->first;
  check(first!=second&&f.fragments.all().size()==2&&f.world.native_actor(second)==0&&f.removes==1,"collider budget pressure preserves both identities and retires only timed native debris");
  f.fragments.erase(first);f.world.retire_collected();f.world.frame();
  check(f.fragments.all().size()==1&&f.fragments.find(second)&&f.world.native_actor(second)!=0,"successful collection frees collision storage for the deferred component");
 }
 {
  Fixture f;f.put(NativeMemory::Hero+0x22a8,0); // Hold simulation still while checking presentation continuity.
  f.put(0x4012c,1.75f);const Vec3 original{12,10,1},offset{.35f,-.2f,.45f};
  f.put(0x91000,std::array<float,4>{{offset.x*1024/1.75f,offset.y*1024/1.75f,offset.z*1024/1.75f,.2f*1024}});
  f.put(0x40100,std::array<float,4>{{(original.x+offset.x)*1024,(original.y+offset.y)*1024,(original.z+offset.z)*1024,358.4f}});
  check(f.world.adopt(0x40000,0x40100),"nonzero model-center and source-scale fixture");const auto id=f.fragments.all().begin()->first;
  f.world.before_load(1);f.arena.reserve(0x1100000,0x80000);f.world.after_load(1);f.world.frame();const auto actor=f.world.native_actor(id);
  check(actor&&std::abs(f.get<float>(actor+0x2c)-1.75f)<.0001f,"same-planet reload retains the source-adjusted native draw scale");
  check(near(f.get<Vec3>(actor+0x10),original)&&near(f.get<Vec3>(actor)*(1.f/1024),original+offset),"same-planet reload keeps model origin aligned with the persistent physical center");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"pause-context fixture");
  const auto first=f.fragments.all().begin()->first;const auto part=f.fragments.find(first)->part;const auto second=f.fragments.spawn(part,{{14,10,1},{},{},{}});
  f.fragments.erase(first);f.put(0x15f6a8,1);const auto before=f.ram;f.world.frame();
  check(second&&f.creates==0&&f.removes==0&&f.ram==before,"paused native menu context performs neither world retirement nor materialization");
  f.put(0x15f6a8,0);f.world.frame();check(f.creates==1&&f.removes==1&&f.world.native_actor(second)!=0,"resuming gameplay retires collected actors and materializes deferred records");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"recycled address persistence fixture");const auto old=f.fragments.all().begin()->first;
  const auto old_collider=f.get<unsigned>(0x40194);const auto used=f.arena.used();
  f.actor(0x40100,0x6c8,{13,10,1});check(f.world.adopt(0x40000,0x40100),"a new fragment can reuse a previous native slot");
  const auto added=f.fragments.all().rbegin()->first;
  check(old!=added&&f.fragments.find(old)&&f.fragments.all().size()==2&&f.world.native_actor(old)==0,"native slot reuse retains the old logical component and assigns a fresh identity");
  check(f.arena.used()==used&&f.get<unsigned>(0x40194)==old_collider&&f.removes==0,"recycled native occupant is preserved and its former collision storage reclaimed safely");
  f.world.frame();check(f.world.native_actor(old)!=0&&f.world.native_actor(added)==0x40100&&f.world.native_actor(old)!=0x40100,"old logical component rematerializes in a distinct native slot");
 }
 {
  Fixture f;f.put(NativeMemory::Hero+0x22a8,0);
  const Vec3 class_center{.4f,-.2f,.6f},world_center{12,10,1};
  f.put(0x91000,std::array<float,4>{{class_center.x*1024,class_center.y*1024,class_center.z*1024,.2f*1024}});
  const auto descriptor=builtin_part(459,0x6c8,.5f);check(descriptor&&f.catalog.register_part(*descriptor)==Status::Ok,"custom level can register a known actual component before a native death");
  const auto id=f.fragments.spawn(descriptor->id,{world_center,{},{},{}});f.world.frame();const auto actor=f.world.native_actor(id);
  check(actor&&std::abs(f.get<float>(actor+0x2c)-2.5f)<.0001f,"API-created part scales actual native sequence bounds to its catalog collision radius");
  check(near(f.get<Vec3>(actor+0x10),world_center-class_center*2.5f)&&near(f.get<Vec3>(actor)*(1.f/1024),world_center),"API-created part derives its model origin from actual class bounds rather than centering its pivot");
 }
 {
  Fixture f;f.split_collision_output=true;f.ray_floor=true;
  check(f.world.adopt(0x40000,0x40100),"non-affine collision relocation fixture");const auto id=f.fragments.all().begin()->first;
  std::array<unsigned char,104> previous{};for(unsigned i=0;i<previous.size();++i)previous[i]=static_cast<unsigned char>(i+1);
  std::memcpy(f.ram.data()+f.collision_output()+0x18,previous.data(),previous.size());
  const auto line=f.world.sweep_sphere({12,10,.1f},{12,10,-.1f},.2f,id);
  check(line&&near(line->point,{12,10,.2f})&&near(line->normal,{0,0,1}),"line contact reads relocated-root point and normal and includes ordinary floor surfaces");
  check(std::memcmp(f.ram.data()+f.collision_output()+0x18,previous.data(),previous.size())==0,"line query restores the complete previous native collision result");
  const auto sphere=f.world.sweep_sphere({12,10,.1f},{12,10,.1f},.2f,id);
  check(sphere&&near(sphere->point,{12,10,.2f})&&near(sphere->normal,{0,0,1}),"sphere overlap reads relocated-root corrected center instead of independently relocating interior fields");
  f.ray_floor=false;f.ray_owner=f.world.native_actor(id);
  check(f.world.visible(*f.fragments.find(id)),"collection line of sight recognizes its own materialized native part through relocated output ownership");
  f.ray_owner=0x42000;check(!f.world.visible(*f.fragments.find(id)),"a different native obstacle blocks collection line of sight");
  check(f.interior_relocations==0&&std::memcmp(f.ram.data()+f.collision_output()+0x18,previous.data(),previous.size())==0,"all native queries relocate only the collision root and preserve the previous result");
 }
 {
  Fixture f;f.stack_address=0x2000;f.ray_floor=true;
  check(!NativeMemory::valid(f.stack_address,32),"SDK stack fixture deliberately lies outside the native actor RAM-validity range");
  const auto contact=f.world.sweep_sphere({12,10,.1f},{12,10,-.1f},.2f,0);
  check(contact&&near(contact->point,{12,10,.2f})&&near(f.last_ray_from,{12,10,.1f})&&near(f.last_ray_to,{12,10,-.1f}),"controlled SDK stack collision endpoints are written even outside actor RAM range");
  check(f.world.adopt(0x40000,0x40100),"SDK stack collection-visibility fixture");const auto& fragment=f.fragments.all().begin()->second;
  f.ray_floor=false;f.ray_owner=f.world.native_actor(fragment.id);
  check(f.world.visible(fragment)&&near(f.last_ray_from,{10,10,2.05f})&&near(f.last_ray_from,f.world.input(false).player)&&near(f.last_ray_to,fragment.position),"collection visibility and pickup share the character torso origin and write both controlled SDK stack endpoints");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"cannon description fixture");const auto id=f.fragments.all().begin()->first;
  const auto description=f.world.cannon_describe(0x40100);
  check(description&&description->fragment.id==id&&description->fragment.part==f.fragments.find(id)->part&&description->actor==0x40100&&description->pclass==0x90000&&description->pvars==f.get<unsigned>(0x40178),"cannon description copies the managed instance and exact native body handles");
  check(!description->lease.generation&&description->collision==f.get<unsigned>(0x40194)&&!f.world.cannon_describe(0x40000),"description neither claims an instance nor accepts an unmanaged source actor");
  std::memcpy(f.ram.data()+0xa0000,f.ram.data()+0x90000,0x4c);
  const auto previous=f.ram;
  check(!f.world.cannon_replace_class(0x40100,0x90001,0xa0000)&&f.ram==previous,"cannon class replacement compares expected class before writing native memory");
  check(f.world.cannon_replace_class(0x40100,0x90000,0xa0000)&&f.get<unsigned>(0x40124)==0xa0000&&f.world.native_actor(id)==0x40100,"class replacement updates native pointer and world expected owner together");
  check(!f.world.cannon_replace_class(0x40100,0x90000,0xa0000),"a stale class expectation cannot repeat class replacement");
  const auto body=f.world.cannon_claim(0x40100);check(body&&body->pclass==0xa0000,"cannon claim observes the replacement class without cloning the native actor");
  check(f.world.cannon_replace_class(0x40100,0xa0000,0x90000)&&f.world.cannon_cancel(*body,true)&&f.world.native_actor(id)==0x40100,"class replacement also tracks detached native actor ownership");
 }
 {
  Fixture f;f.arena.reserve(0x1000000,48);check(f.world.adopt(0x40000,0x40100),"atomic cannon claim fixture");const auto id=f.fragments.all().begin()->first;
  const auto before=f.ram;const auto used=f.arena.used();const auto original=*f.fragments.find(id);
  const auto body=f.world.cannon_claim(0x40100);
  check(body&&body->lease.instance==id&&body->lease.visit==original.visit&&body->lease.generation&&f.fragments.owns(body->lease),"claim atomically acquires exactly the original world lease");
  check(f.world.materialized()==0&&f.world.native_actor(id)==0&&f.fragments.find(id)&&f.ram==before&&f.arena.used()==used&&f.removes==0,"claim detaches only the world actor map without deleting native body, freeing collision, or erasing component");
  check(!f.world.cannon_claim(0x40100)&&!f.world.cannon_describe(0x40100),"a claimed native actor cannot be described or claimed again");
  for(unsigned frame=0;frame<120;++frame)f.world.frame();
  const auto retained=f.fragments.find(id);
  check(retained&&near(retained->position,original.position)&&near(retained->velocity,original.velocity)&&near(retained->rotation,original.rotation)&&f.creates==0&&f.removes==0&&f.ram==before,"claimed component neither simulates nor rematerializes while the native cannon owns its transform");
  const auto second=f.fragments.spawn(original.part,{{14,10,1},{},{},{}});f.put(NativeMemory::Hero+0x22a8,0);f.world.frame();
  check(second&&f.world.materialized()==0&&f.world.native_actor(id)==0&&f.creates==1&&f.removes==1,"leased collider remains unavailable to another world part until cancellation or retirement");
  check(f.world.cannon_cancel(*body,true)&&!f.fragments.claimed(id)&&f.world.native_actor(id)==0x40100,"valid cancellation restores the original native body and releases its lease");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"cannon transform cancellation fixture");const auto id=f.fragments.all().begin()->first;const auto body=f.world.cannon_claim(0x40100);
  const Vec3 position{15,13,4},rotation{.2f,.3f,.4f};
  f.put(0x40110,position);f.put(0x40140,rotation);Fixture::matrix(0x40100);const auto before=f.ram;
  check(body&&f.world.cannon_cancel(*body,true),"native-valid cancellation succeeds");const auto restored=f.fragments.find(id);
  check(restored&&near(restored->position,position)&&near(restored->rotation,rotation)&&near(restored->velocity,{})&&!restored->sleeping&&!restored->suspended,"cancellation copies the current native transform and reactivates the component");
  check(f.world.native_actor(id)==0x40100&&f.ram==before&&f.removes==0,"cancellation restores mapping without overwriting native render/collision state");
  check(f.world.cannon_cancel(*body,true)&&!f.world.cannon_capture(*body),"completed cancellation acknowledges retries without capturing through its stale lease");
 }
 {
  Fixture f;f.arena.reserve(0x1000000,48);check(f.world.adopt(0x40000,0x40100),"recycled cannon body cancellation fixture");const auto id=f.fragments.all().begin()->first;const auto original=*f.fragments.find(id);const auto body=f.world.cannon_claim(0x40100);
  f.actor(0x40100,0x6c8,{20,20,5});f.put(0x40194,0x99000u);std::array<unsigned char,256> replacement{};std::memcpy(replacement.data(),f.ram.data()+0x40100,replacement.size());
  check(body&&f.world.cannon_cancel(*body,true)&&f.fragments.find(id)&&!f.fragments.claimed(id)&&f.world.native_actor(id)==0,"stale native cancellation retains the logical component while releasing the old lease");
  check(std::memcmp(replacement.data(),f.ram.data()+0x40100,replacement.size())==0&&near(f.fragments.find(id)->position,original.position)&&f.removes==0,"recycled native actor is neither restored nor deleted and cannot supply another component's transform");
  f.put(NativeMemory::Hero+0x22a8,0);f.world.frame();
  check(f.world.native_actor(id)&&f.world.native_actor(id)!=0x40100&&f.world.materialized()==1&&f.arena.used()==48,"stale cancellation reclaims exactly the former collider budget for rematerialization");
  check(std::memcmp(replacement.data(),f.ram.data()+0x40100,replacement.size())==0,"rematerialization leaves the recycled occupant byte-for-byte intact");
 }
 {
  Fixture f;f.arena.reserve(0x1000000,48);check(f.world.adopt(0x40000,0x40100),"cannon acknowledged capture fixture");const auto id=f.fragments.all().begin()->first;const auto part=f.fragments.find(id)->part;const auto body=f.world.cannon_claim(0x40100);const auto before=f.ram;
  check(body&&f.world.cannon_capture(*body)&&!f.fragments.find(id)&&!f.fragments.claimed(id),"acknowledged capture atomically takes the leased world record");
  check(f.removes==0&&f.ram==before&&f.arena.used()==48,"capture keeps the original native projectile and leased collision storage alive");
  check(!f.world.cannon_capture(*body)&&!f.world.cannon_cancel(*body,true),"acknowledged capture cannot duplicate or restore the consumed world record");
  f.world.frame();check(f.creates==0&&f.removes==0,"captured native projectile is outside world rematerialization and retirement");
  f.put<unsigned char>(0x40120,0xfe);f.world.cannon_retire(*body);f.world.cannon_retire(*body);
  const auto a=f.fragments.spawn(part,{{14,10,1},{},{},{}}),b=f.fragments.spawn(part,{{16,10,1},{},{},{}});f.put(NativeMemory::Hero+0x22a8,0);f.world.frame();
  check(a&&b&&f.world.materialized()==1&&f.arena.used()==48&&f.creates==2&&f.removes==1,"finalization returns collider budget once; repeated retirement cannot alias the same collider to two parts");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"cannon malformed retirement fixture");const auto body=f.world.cannon_claim(0x40100);auto stale=*body;++stale.lease.visit;
  f.world.cannon_retire(stale);
  check(f.world.cannon_cancel(*body,true),"wrong-visit retirement cannot discard a valid detached body or its leased collider");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"same-planet pending cannon reload fixture");const auto id=f.fragments.all().begin()->first;const auto body=f.world.cannon_claim(0x40100);
  f.world.before_load(1);f.arena.reserve(0x1100000,48);f.world.after_load(1);
  check(f.fragments.find(id)&&!f.fragments.claimed(id)&&!f.fragments.owns(body->lease),"same-planet reload releases pending cannon claims while retaining visit components");
  check(f.world.cannon_cancel(*body,false)&&!f.world.cannon_capture(*body),"old cancellation is acknowledged without restoring or capturing after reload");
  f.put(NativeMemory::Hero+0x22a8,0);f.world.frame();check(f.world.native_actor(id)!=0,"same-planet death can rematerialize a component interrupted during suction");
 }
 {
  Fixture f;f.arena.reserve(0x1000000,48);check(f.world.adopt(0x40000,0x40100),"cross-visit cannon collision-budget fixture");const auto part=f.fragments.all().begin()->second.part;const auto old=f.world.cannon_claim(0x40100);
  f.world.before_load(2);f.level=2;f.arena.reserve(0x1000000,48);f.world.after_load(2);f.put(NativeMemory::Hero+0x22a8,0);
  const auto a=f.fragments.spawn(part,{{14,10,1},{},{},{}});f.world.frame();const auto actor=f.world.native_actor(a);const auto collision=f.get<unsigned>(actor+0x94);
  check(actor&&f.world.cannon_cancel(*old,true)&&!f.world.cannon_capture(*old),"different visit acknowledges old cancellation without reviving its pending body identity");
  f.world.cannon_retire(*old);const auto b=f.fragments.spawn(part,{{16,10,1},{},{},{}});f.world.frame();
  check(b&&f.world.materialized()==1&&f.world.native_actor(a)==actor&&f.get<unsigned>(actor+0x94)==collision,"stale-visit retirement cannot free a collider address already allocated in the new level arena");
 }
 {
  Fixture f;check(f.world.adopt(0x40000,0x40100),"same-type recycled cannon actor fixture");const auto old_id=f.fragments.all().begin()->first;const auto old=f.world.cannon_claim(0x40100);
  f.actor(0x40100,0x6c8,{20,20,5});check(f.world.adopt(0x40000,0x40100),"same-class recycled slot can be adopted as a fresh component");const auto newer=f.fragments.all().rbegin()->first;
  check(old_id!=newer&&f.fragments.find(old_id)&&!f.fragments.claimed(old_id)&&f.world.native_actor(newer)==0x40100,"re-adopting a detached address releases only its old handle and preserves both logical component identities");
  const auto before=f.ram;check(f.world.cannon_cancel(*old,true)&&f.ram==before,"a stale cannon cancellation is acknowledged without attaching the old identity to a new same-class native body");
 }
 {
  Fixture f;f.pool(0x600000,100);f.put(NativeMemory::Hero+0x22a8,0); // Keep positions fixed while checking native budgeting.
  std::vector<InstanceId> ids;std::vector<unsigned> original_actors;
  for(unsigned i=0;i<100;++i){const auto actor=f.native_slots[i];f.actor(actor,0x6c8,{10+float(i)*.2f,10,1});
   if(i==99)f.put(actor+0x2c,1.75f);
   check(f.world.adopt(0x40000,actor),"large accumulation adopts each actual mechanical native model");ids.push_back(f.fragments.all().rbegin()->first);original_actors.push_back(actor);
  }
  const auto arena_used=f.arena.used();f.world.frame();
  check(f.fragments.all().size()==100&&f.world.materialized()==96&&f.removes==4,"one hundred real adoptions retain one hundred records while only nearest96 stay materialized");
  for(unsigned i=0;i<ids.size();++i)check((f.world.native_actor(ids[i])!=0)==(i<96),"native budget gives nearer records priority instead of factory arrival overflow");
  std::vector<Fragment> saved;for(const auto id:ids)saved.push_back(*f.fragments.find(id));
  f.put(NativeMemory::Hero+0x80,Vec3{1000,1000,1});f.world.frame();
  check(f.world.materialized()==0&&f.fragments.all().size()==100&&f.removes==100,"leaving the accumulation retires native actors while retaining all logical components");
  for(unsigned i=0;i<ids.size();++i){const auto* part=f.fragments.find(ids[i]);check(part&&part->suspended&&near(part->position,saved[i].position)&&near(part->rotation,saved[i].rotation),"distant dematerialization preserves original transform and marks suspension");}
  for(const auto actor:original_actors)check(f.world.owns_callback(actor)&&!f.world.owns(actor)&&f.get<unsigned>(actor+0x74)==0,"every queued callback of a budget-retired actor is absorbed without the real level loader");
  const auto queries=f.queries,creates=f.creates;for(unsigned i=0;i<900;++i)f.world.frame();
  check(f.fragments.all().size()==100&&f.world.materialized()==0&&f.creates==creates&&f.removes==100&&f.queries==queries,"distant virtual records neither expire nor allocate, collide or repeatedly retire native objects");
  f.put(NativeMemory::Hero+0x80,Vec3{10,10,1});f.world.frame();
  check(f.world.materialized()==96&&f.fragments.all().size()==100&&f.creates==96&&f.arena.used()==arena_used,"returning reuses bounded collider storage to rematerialize nearest96 original identities");
  std::set<unsigned> colliders;
  for(unsigned i=0;i<ids.size();++i){const auto actor=f.world.native_actor(ids[i]);check((actor!=0)==(i<96),"returning selects the same nearest logical identities");if(actor){check(f.world.owns_callback(actor)&&!f.fragments.find(ids[i])->suspended&&!f.fragments.find(ids[i])->sleeping,"rematerialization reactivates its record and absorbs callbacks as an active actor");colliders.insert(f.get<unsigned>(actor+0x94));}}
  check(colliders.size()==96&&f.arena.used()==arena_used,"reused colliders remain unique; no two materialized parts share a freed allocation");
  f.put(NativeMemory::Hero+0x80,Vec3{30,10,1});f.world.frame();const auto scaled=f.world.native_actor(ids.back());
  check(scaled&&std::abs(f.get<float>(scaled+0x2c)-1.75f)<.0001f&&f.arena.used()==arena_used,"native source draw-scale metadata survives over-budget dematerialization and later nearest selection");
 }
 {
  Fixture f;f.pool(0x700000,100);f.put(NativeMemory::Hero+0x22a8,0);std::vector<InstanceId> ids;
  for(unsigned i=0;i<100;++i){const auto actor=f.native_slots[i];f.actor(actor,0x6c8,{10+float(i)*.2f,10,1});check(f.world.adopt(0x40000,actor),"held-custody budget fixture adopts actual native models");ids.push_back(f.fragments.all().rbegin()->first);}
  f.world.frame();const auto held_actor=f.world.native_actor(ids[0]),held_collision=f.get<unsigned>(held_actor+0x94);const auto held=f.world.cannon_claim(held_actor);
  check(held&&f.fragments.owns(held->lease),"the nearest materialized component can enter exclusive cannon custody");
  std::array<unsigned char,256> held_bytes{};std::memcpy(held_bytes.data(),f.ram.data()+held_actor,held_bytes.size());
  f.world.frame();check(f.world.materialized()==96&&f.fragments.find(ids[0])&&f.fragments.claimed(ids[0])&&f.world.native_actor(ids[0])==0,"claimed instance is excluded while the freed world slot is assigned to another nearest part");
  f.put(NativeMemory::Hero+0x80,Vec3{1000,1000,1});f.world.frame();
  check(f.world.materialized()==0&&f.fragments.all().size()==100&&f.fragments.owns(held->lease)&&std::memcmp(held_bytes.data(),f.ram.data()+held_actor,held_bytes.size())==0,"distance eviction never deletes, mutates or releases a held native cannon body");
  f.put(NativeMemory::Hero+0x80,Vec3{10,10,1});f.world.frame();
  bool separate=true;for(const auto id:ids){const auto actor=f.world.native_actor(id);if(actor&&f.get<unsigned>(actor+0x94)==held_collision)separate=false;}
  check(f.world.materialized()==96&&separate&&f.fragments.owns(held->lease)&&std::memcmp(held_bytes.data(),f.ram.data()+held_actor,held_bytes.size())==0,"returning cannot reuse a collider or native actor still leased to the cannon");
  check(f.world.cannon_cancel(*held,true),"held cancellation remains valid after a complete dematerialization and return cycle");f.world.frame();
  check(f.world.materialized()==96&&f.world.native_actor(ids[0])==held_actor&&!f.fragments.claimed(ids[0]),"released nearest component rejoins the budget without duplicating its actor or record");
 }
 {
  Fixture f;f.pool(0x900000,100);std::map<InstanceId,Fragment> original;
  for(unsigned i=0;i<100;++i){const auto actor=f.native_slots[i];f.actor(actor,0x6c8,{15+float(i%10)*.6f,10+float(i/10)*.6f,3});check(f.world.adopt(0x40000,actor),"enabled-physics budget fixture adopts one hundred separated native models");const auto id=f.fragments.all().rbegin()->first;original.emplace(id,*f.fragments.find(id));}
  f.world.frame();unsigned virtual_count=0,moving_count=0;
  for(const auto& [id,previous]:original){const auto* current=f.fragments.find(id);if(!f.world.native_actor(id)){++virtual_count;check(current&&current->suspended&&near(current->position,previous.position)&&near(current->velocity,previous.velocity)&&!f.world.simulation_enabled(id),"nearby over-budget logical record stays suspended without falling through absent native collision");}
   else if(current&&current->position.z<previous.position.z)++moving_count;
  }
  check(virtual_count==4&&f.world.materialized()==96&&moving_count>0&&f.queries>0,"physical96 simulate while four virtual near records skip terrain queries and integration");
 }
 {
  Fixture f;f.pool(0x800000,96);f.put(NativeMemory::Hero+0x22a8,0);
  const auto supported=builtin_part(459,0x6c8),unsupported=builtin_part(459,0x6c9);
  check(supported&&unsupported&&f.catalog.register_part(*supported)==Status::Ok&&f.catalog.register_part(*unsupported)==Status::Ok,"loaded and unloaded source-backed models are both valid logical catalog entries");
  std::vector<InstanceId> absent,present;
  for(unsigned i=0;i<100;++i){absent.push_back(f.fragments.spawn(unsupported->id,{{10,10,1},{},{},{}}));present.push_back(f.fragments.spawn(supported->id,{{15+float(i)*.1f,10,1},{},{},{}}));}
  f.world.frame();bool none_absent=true;for(const auto id:absent)none_absent&=f.world.native_actor(id)==0;
  check(f.fragments.all().size()==200&&f.world.materialized()==96&&f.creates==96&&none_absent,"one hundred nearer unloaded models spend no materialization budget and lose no records");
  for(unsigned i=0;i<present.size();++i)check((f.world.native_actor(present[i])!=0)==(i<96),"loaded eligible resources receive the complete nearest96 budget");
 }
 for(bool claimed:{false,true}){
  Fixture f;check(f.world.adopt(0x40000,0x40100),"loading cleanup fixture uses a genuinely adopted actor");
  const auto id=f.fragments.all().begin()->first;
  if(claimed)check(bool(f.world.cannon_claim(0x40100)),"loading cleanup also covers incoming cannon custody");
  f.put<int>(0x15f6a8,6);const auto before=f.ram;const auto removed=f.removes;
  f.world.before_load(1);
  check(f.ram==before&&f.removes==removed,"loading retirement never writes or deletes old overlay actors or compressed input");
  check(f.world.materialized()==0&&f.fragments.find(id)&&!f.fragments.claimed(id),"loading cleanup drops native handles and releases host custody without losing the same-visit record");
 }
 std::printf("salvage native world: %u checks passed\n",checks);
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "native_world.hpp"
#include "damage_targets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
namespace revolverator {
namespace {
struct Stack {
 const openrac_api* api;unsigned saved{},at{},size{};
 Stack(const openrac_api* a,unsigned n):api(a),size(n){at=api->stack_push(n,&saved);}
 ~Stack(){if(at)api->stack_pop(saved);}
 template<class T> void put(unsigned offset,const T& value)const{
  if(at&&offset<=size&&sizeof(T)<=size-offset)std::memcpy(api->memory+at+offset,&value,sizeof value);
 }
 template<class T> T get(unsigned offset)const{
  T value{};if(at&&offset<=size&&sizeof(T)<=size-offset)std::memcpy(&value,api->memory+at+offset,sizeof value);return value;
 }
};
}
NativeWorld::NativeWorld(const openrac_api* api,openrac_mod* mod,NativeAmmunition& ammunition,NativeRotor& rotor,GuestArena& arena):Memory(api),arm_(api,arena),ammunition_(ammunition),rotor_(rotor),
 allocate_(function<Allocate>("FUN_001ff288")),post_hit_(function<Hit>("FUN_L00_00259a88")),
 ray_(function<Ray>("FUN_001efa68")),basis_(function<Basis>("FUN_001fa030")),
 state_(function<State>("FUN_L00_002223f8")),animation_(function<Animation>("FUN_L00_002323b8")),
 mod_(mod),trace_(api->setting_bool(mod,"trace")!=0){}
NativeWorld::~NativeWorld(){reset();}
void NativeWorld::log(const char* s)const{api_->log(mod_,OPENRAC_LOG_INFO,s);}
Target NativeWorld::token(unsigned m)const{
 if(!Memory::valid(m,256))return {};
 return {m,read<unsigned>(m+0x78),unsigned(read<unsigned short>(m+0xa6)),read<unsigned>(m+0x38)};
}
bool NativeWorld::identity_valid(Target t)const{
 return t&&api_->loaded_level()==level_&&Memory::valid(t.address,256)&&token(t.address)==t
  &&read<unsigned char>(t.address+0x20)<0x80;
}
bool NativeWorld::valid(Target t)const{return identity_valid(t)&&capturable_class(int(t.klass));}
void NativeWorld::each_enemy(const void* context,void(*fn)(const void*,Target),bool include_crates)const{
 const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
 if(!Memory::valid(first,256)||last<first||last-first>4096*256||!Memory::valid(last-256,256))return;
 for(auto m=first;m<last;m+=256){const auto t=token(m);if(identity_valid(t)&&(capturable_class(int(t.klass))||(include_crates&&destructible_crate_class(int(t.klass)))))fn(context,t);}
}
std::optional<Vec3> NativeWorld::ray(Vec3 from,Vec3 to,unsigned ignore)const{
 Stack stack(api_,32);if(!stack.at||!finite(from)||!finite(to))return std::nullopt;
 const std::array<float,4> start{from.x,from.y,from.z,0},end{to.x,to.y,to.z,0};
 std::memcpy(api_->memory+stack.at,start.data(),16);std::memcpy(api_->memory+stack.at+16,end.data(),16);
 if(!ray_(stack.at,stack.at+16,0x24,ignore,0))return std::nullopt;
 const auto point=read<Vec3>(level(0x173e60));
 return finite(point)?std::optional<Vec3>{point}:std::nullopt;
}
std::optional<Target> NativeWorld::find_target(Vec3 origin,float yaw,float reach){
 site_={};
 struct Search{NativeWorld* self;Vec3 origin;float yaw,reach;std::optional<Target> best;float score;};
 Search search{this,origin,yaw,reach,{},std::numeric_limits<float>::max()};
 each_enemy(&search,+[](const void* ctx,Target target){
  auto& s=*const_cast<Search*>(static_cast<const Search*>(ctx));auto& w=*s.self;
  const auto pos=w.read<Vec3>(target.address+0x10);const auto delta=pos-s.origin;
  const float distance=delta.x*delta.x+delta.y*delta.y;
  if(!finite(pos)||std::abs(delta.z)>2.f||distance>s.reach*s.reach)return;
  const float cosine=distance<.04f?1.f:(delta.x*std::cos(s.yaw)+delta.y*std::sin(s.yaw))/std::sqrt(distance);
  if(cosine<(distance<2.25f?-.4f:.15f))return;
  const float score=distance+(1-cosine)*2;
  if(score>=s.score)return;
  const Vec3 start=s.origin+Vec3{0,0,.9f};
  Vec3 centre=w.read<Vec3>(target.address)*(1.f/1024.f);
  if(!finite(centre)||length_squared(centre-pos)>16)centre=pos+Vec3{0,0,.6f};
  const auto obstruction=w.ray(start,centre,w.body());
  const auto hit_owner=obstruction?w.read<unsigned>(w.level(0x173e58)):0;
  const bool hit_target=obstruction&&hit_owner==target.address;
  const bool clear=!obstruction||hit_target||hit_owner==w.weapon();
  if(w.trace_){char msg[240];std::snprintf(msg,sizeof msg,"candidate class=%u dist=%.2f z=%.2f cosine=%.2f visible=%u hit=%08x target-contact=%d",target.klass,std::sqrt(distance),delta.z,cosine,unsigned(w.read<unsigned char>(target.address+0x31)),hit_owner,int(hit_target));w.log(msg);}
  if(score<s.score&&clear){
   s.score=score;s.best=target;
   Vec3 approach=unit(centre-start);
   if(hit_target){
    const Vec3 normal=w.read<Vec3>(w.level(0x173e80));
    if(finite(normal)&&length_squared(normal)>.25f)approach=unit(normal)*-1;
   }
   const float radius=std::clamp(w.read<float>(target.address+12)/1024.f,.15f,.65f);
   const Vec3 contact=hit_target?*obstruction:centre-approach*radius;
   w.site_={target,contact,approach};
  }
 });
 return search.best;
}
bool NativeWorld::capture(Target t){
 if(lease_.target||!valid(t))return false;
 if(!(site_.target==t)||!finite(site_.point))return false;
 lease_={};lease_.target=t;lease_.collision=read<unsigned>(t.address+0x94);
 lease_.disabled=read<unsigned>(t.address+0x98);lease_.origin=read<Vec3>(t.address+0x10);
 lease_.angles=read<Vec3>(t.address+0x40);lease_.spin_angle=read<float>(hero()+0x98);
 lease_.grip=capture_grip(lease_.origin,lease_.angles,site_.point,site_.approach,lease_.spin_angle);
 const auto q=lease_.grip.original;
 lease_.local_sphere=rotate({-q[0],-q[1],-q[2],q[3]},read<Vec3>(t.address)*(1.f/1024.f)-lease_.origin);
 write<unsigned>(t.address+0x94,0);write<unsigned>(t.address+0x98,1);
 if(trace_){char s[240];std::snprintf(s,sizeof s,"captured class=%u moby=%08x local-grip=(%.3f,%.3f,%.3f)",t.klass,t.address,lease_.grip.local_point.x,lease_.grip.local_point.y,lease_.grip.local_point.z);log(s);}
 return true;
}
bool NativeWorld::lease_valid()const{return lease_.target&&valid(lease_.target);}
bool NativeWorld::frozen(unsigned moby)const{return lease_valid()&&lease_.target.address==moby;}
void NativeWorld::move(Target t,Vec3 p,float yaw){
 if(!lease_valid()||!(lease_.target==t)||!finite(p)||!std::isfinite(yaw))return;
 if(!lease_.flying)lease_.spin_angle=yaw;
 const auto pose=attach_to_tip(lease_.grip,p,lease_.spin_angle,std::clamp(lease_.hold_time/.25f,0.f,1.f));
 move_origin(t,pose.origin,pose.angles);
}
void NativeWorld::move_origin(Target t,Vec3 p,Vec3 angles){
 const Vec3 centre=p+rotate(rotation_from_euler(angles),lease_.local_sphere);
 write(t.address+0x10,p);write(t.address,centre*1024.f);write(t.address+0x40,angles);
 basis_(t.address+0xc0,t.address+0x40);
}
void NativeWorld::release(Target t,bool cancelled){
 if(!(lease_.target==t))return;
 if(valid(t)){
  move_origin(t,cancelled?lease_.origin:read<Vec3>(t.address+0x10),lease_.angles);
  if(read<unsigned>(t.address+0x94)==0)write(t.address+0x94,lease_.collision);
  if(read<unsigned>(t.address+0x98)==1)write(t.address+0x98,lease_.disabled);
 }
 lease_={};
 if(trace_)log(cancelled?"capture cancelled; native ownership restored":"target released; native ownership restored");
}
std::optional<Contact> NativeWorld::sweep(Target t,Vec3 from,Vec3 to){
 struct Search{NativeWorld* self;Target target;Vec3 from,to;std::optional<Contact> hit;};
 Search search{this,t,from,to,{}};
 each_enemy(&search,+[](const void* ctx,Target candidate){
  auto& s=*const_cast<Search*>(static_cast<const Search*>(ctx));if(s.hit||candidate==s.target)return;
  const Vec3 p=s.self->read<Vec3>(candidate.address+0x10)+Vec3{0,0,.5f};
  const Vec3 d=s.to-s.from;const float l=length_squared(d);
  const Vec3 a=p-s.from;const float u=l>.00001f?std::clamp((a.x*d.x+a.y*d.y+a.z*d.z)/l,0.f,1.f):0;
  if(length_squared(p-(s.from+d*u))<1.1f*1.1f)s.hit=Contact{s.from+d*u,candidate};
 },true);
 if(search.hit)return search.hit;
 if(auto point=ray(from,to,t.address))return Contact{*point,{}};
 return std::nullopt;
}
void NativeWorld::hit(Target t,float damage,Vec3 direction){
 if(!identity_valid(t)||(!capturable_class(int(t.klass))&&!destructible_crate_class(int(t.klass)))||!finite(direction)||!std::isfinite(damage))return;
 Stack stack(api_,48);if(!stack.at)return;
 std::memset(api_->memory+stack.at,0,48);
 // SDK stack addresses are mapped above console RAM. Only the owning Stack
 // may access this span; ordinary entity-memory bounds must stay conservative.
 stack.put(0,direction);stack.put(12,5627.925f);
 stack.put(0x10,weapon());stack.put<unsigned>(0x14,0x30000);
 stack.put<unsigned char>(0x19,3);stack.put<unsigned short>(0x1a,180);
 stack.put(0x1c,damage);stack.put<int>(0x20,1);
 post_hit_(t.address,stack.at);
}
void NativeWorld::impact(Target t,std::optional<Target> other,float charge){
 const float yaw=read<float>(hero()+0x98);const Vec3 direction{std::cos(yaw)*2,std::sin(yaw)*2,1.3f};
 hit(t,3.f+charge*5.f,direction);if(other)hit(*other,3.f+charge*3.f,direction);
 if(trace_)log("native impact posted");
}
bool NativeWorld::blocks_native_hit(unsigned hit)const{
 // Built-in Walloper swing remains the attack animation; its legacy impact must
 // not kill the enemy before capture. Our posted impacts name the weapon moby.
 return Memory::valid(hit,0x40)&&read<unsigned>(hit+0x20)==body()
  &&(read<unsigned>(hit+0x24)&0x30000)!=0&&read<int>(hero()+0x10b8)==18;
}
Frame NativeWorld::snapshot(bool primary,unsigned epoch)const{
 Frame f;f.epoch=epoch;f.seconds=read<float>(global(0x15ed7c));
 f.equipped=read<int>(hero()+0x10b8)==18&&read<int>(hero()+0x10b4)==2&&read<unsigned char>(hero()+0x20a4)==0;
 f.playing=read<int>(mode())==0;f.paused=read<int>(mode())==3;
 const int state=read<int>(hero()+0x2084);
 f.alive=read<int>(hero()+0x22a8)>0&&state!=0x3d&&state!=0x82&&state!=0x77;
 f.primary=primary;f.grounded=state==0||state==1||state==2||state==3||state==0x20;
 f.position=read<Vec3>(hero()+0x80);f.yaw=read<float>(hero()+0x98);return f;
}
void NativeWorld::animate(Motion motion){
 if(motion==Motion::DryFire){rotor_.dry_fire();return;}
 if(api_->loaded_level()<0||api_->loaded_level()>18)return;
 if(motion==Motion::Strike){state_(0x20,1);if(trace_)log("native Walloper strike");}
 if(motion==Motion::Release){
  lease_.flying=true;
  // The wrench's boomerang recovery waits for a returning wrench. This weapon
  // never enters that state: the arm follows through while locomotion continues.
  arm_.release();
  if(read<int>(hero()+0x2084)==0x20)state_(0,1);
  if(trace_)log("throw released; locomotion free; arm follow-through 0.18 seconds");
 }
 if(motion==Motion::Recover)arm_.reset();
}
void NativeWorld::pose(Phase,float){arm_.settle(read<float>(global(0x15ed7c)));}
void NativeWorld::align_weapon(float angle){
 if(auto tip=arm_.align_weapon(weapon()))if(lease_valid())move(lease_.target,*tip,angle);
}
std::optional<Vec3> NativeWorld::carry_anchor(float angle){
 if(lease_valid())lease_.hold_time+=std::clamp(read<float>(global(0x15ed7c)),0.f,.1f);
 (void)angle;
 return arm_.hold(body(),weapon(),read<float>(hero()+0x98),read<float>(global(0x15ed7c)));
}
void NativeWorld::reset(){
 if(lease_.target)release(lease_.target,true);
 arm_.reset(true);level_=api_->loaded_level();
}
}

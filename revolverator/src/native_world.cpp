// SPDX-License-Identifier: GPL-3.0-or-later
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
 burst_(function<Burst>("FUN_L00_0025f090")),
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
void NativeWorld::each_damageable(const void* context,void(*fn)(const void*,Target))const{
 const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
 if(!Memory::valid(first,256)||last<first||last-first>4096*256||!Memory::valid(last-256,256))return;
 for(auto m=first;m<last;m+=256){
  const auto t=token(m);
  if(identity_valid(t)&&golden_damage_class(t.klass))fn(context,t);
 }
}
float NativeWorld::collision_radius(Target t)const{
 const float radius=read<float>(t.address+12)/1024.f;
 return std::isfinite(radius)&&radius>0?std::clamp(radius,.15f,4.f):.35f;
}
Vec3 NativeWorld::damage_centre(Target t)const{
 const Vec3 origin=read<Vec3>(t.address+0x10);
 const Vec3 centre=read<Vec3>(t.address)*(1.f/1024.f);
 const float radius=read<float>(t.address+12);
 return std::isfinite(radius)&&radius>0&&finite(centre)&&length_squared(centre-origin)<4096.f?
  centre:origin+Vec3{0,0,.5f};
}
bool NativeWorld::visible_from(Vec3 from,Target target,unsigned ignore)const{
 const Vec3 to=damage_centre(target);
 if(!finite(from)||!finite(to)||!identity_valid(target))return false;
 if(length_squared(to-from)<.0001f)return true;
 Stack stack(api_,32);if(!stack.at)return false; // Failure is not clear line of sight.
 stack.put(0,std::array<float,4>{from.x,from.y,from.z,0});
 stack.put(16,std::array<float,4>{to.x,to.y,to.z,0});
 if(!ray_(stack.at,stack.at+16,0x24,ignore,0))return true;
 return read<unsigned>(level(0x173e58))==target.address;
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
  if(w.flying(target))return;
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
 if(lease_.target||!valid(t)||flying(t)||!can_launch())return false;
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
NativeWorld::Flight* NativeWorld::flight(Target target){
 for(auto& entry:flights_)if(entry.target&&entry.target==target)return &entry;
 return nullptr;
}
const NativeWorld::Flight* NativeWorld::flight(Target target)const{
 for(const auto& entry:flights_)if(entry.target&&entry.target==target)return &entry;
 return nullptr;
}
bool NativeWorld::flying(Target target)const{return target&&flight(target)!=nullptr;}
std::size_t NativeWorld::flight_count()const{
 std::size_t count=0;for(const auto& entry:flights_)if(entry.target)++count;return count;
}
bool NativeWorld::can_launch()const{return flight_count()<maximum_flights;}
NativeWorld::Lease* NativeWorld::owned_lease(Target target){
 if(lease_.target&&lease_.target==target)return &lease_;
 auto* entry=flight(target);return entry&&entry->lease.target?&entry->lease:nullptr;
}
bool NativeWorld::frozen(unsigned moby)const{
 if(lease_valid()&&lease_.target.address==moby)return true;
 for(const auto& entry:flights_)if(entry.lease.target.address==moby&&valid(entry.lease.target))return true;
 return false;
}
void NativeWorld::move(Target t,Vec3 p,float yaw){
 auto* lease=owned_lease(t);
 if(!lease||!valid(t)||!finite(p)||!std::isfinite(yaw))return;
 if(!lease->flying)lease->spin_angle=yaw;
 const auto pose=attach_to_tip(lease->grip,p,lease->spin_angle,std::clamp(lease->hold_time/.25f,0.f,1.f));
 move_origin(*lease,pose.origin,pose.angles);
}
void NativeWorld::move_origin(const Lease& lease,Vec3 p,Vec3 angles){
 const auto t=lease.target;
 const Vec3 centre=p+rotate(rotation_from_euler(angles),lease.local_sphere);
 write(t.address+0x10,p);write(t.address,centre*1024.f);write(t.address+0x40,angles);
 basis_(t.address+0xc0,t.address+0x40);
}
void NativeWorld::restore_lease(Lease& lease,bool cancelled){
 const auto t=lease.target;if(!t)return;
 if(valid(t)){
  // A launched enemy is already in the world. Teardown restores its native
  // AI/collision in place; only cancelling a carry returns it to its origin.
  move_origin(lease,cancelled&&!lease.flying?lease.origin:read<Vec3>(t.address+0x10),lease.angles);
  if(read<unsigned>(t.address+0x94)==0)write(t.address+0x94,lease.collision);
  if(read<unsigned>(t.address+0x98)==1)write(t.address+0x98,lease.disabled);
 }
 lease={};
}
void NativeWorld::release(Target t,bool cancelled){
 if(lease_.target&&lease_.target==t){restore_lease(lease_,cancelled);if(cancelled)attack_cancelled();}
 else if(auto* entry=flight(t)){
  restore_lease(entry->lease,cancelled);
  if(cancelled)*entry={};
 }else return;
 if(trace_)log(cancelled?"target cancelled; native ownership restored":"target released; native ownership restored");
}
void NativeWorld::discarded(Target t){
 if(lease_.target&&lease_.target==t){restore_lease(lease_,true);attack_cancelled();return;}
 if(auto* entry=flight(t)){restore_lease(entry->lease,true);*entry={};}
}
std::optional<Contact> NativeWorld::sweep(Target t,Vec3 from,Vec3 to){
 const auto* active_flight=flight(t);
 if(active_flight&&active_flight->impact.golden_flight(t)){
  // Golden can collide with bosses and other damageable actors, but never
  // acquire them. Respect the nearest terrain contact before testing actors.
  struct Search{NativeWorld* self;Target target;Vec3 from,to;std::optional<Contact> hit;float first=1.001f;};
  Search search{this,t,from,to,{}};
  const Vec3 direction=to-from;const float square=length_squared(direction);
  if(!finite(from)||!finite(to)||!std::isfinite(square))return std::nullopt;
  if(auto point=ray(from,to,t.address)){
   search.hit=Contact{*point,{},read<Vec3>(level(0x173e80))};
   search.first=square>.000001f?std::clamp(dot(*point-from,direction)/square,0.f,1.f):0;
  }
  each_damageable(&search,+[](const void* ctx,Target candidate){
   auto& s=*const_cast<Search*>(static_cast<const Search*>(ctx));auto& w=*s.self;
   if(candidate==s.target||w.flying(candidate)||w.frozen(candidate.address))return;
   const Vec3 p=w.damage_centre(candidate),d=s.to-s.from,offset=s.from-p;
   const float radius=w.collision_radius(candidate)+.4f,l=length_squared(d),c=length_squared(offset)-radius*radius;
   if(!finite(p)||!std::isfinite(c))return;
   float u=0;
   if(c>0){
    if(l<=.000001f)return;
    const float b=dot(offset,d),discriminant=b*b-l*c;
    if(!std::isfinite(discriminant)||discriminant<0)return;
    u=(-b-std::sqrt(discriminant))/l;
   }
   if(u<0||u>1||u>=s.first||!w.visible_from(s.from,candidate,s.target.address))return;
   s.first=u;s.hit=Contact{s.from+d*u,candidate,{}};
  });
  return search.hit;
 }
 struct Search{NativeWorld* self;Target target;Vec3 from,to;std::optional<Contact> hit;};
 Search search{this,t,from,to,{}};
 const auto visit=+[](const void* ctx,Target candidate){
  auto& s=*const_cast<Search*>(static_cast<const Search*>(ctx));if(s.hit||candidate==s.target||s.self->flying(candidate)||s.self->frozen(candidate.address))return;
  const Vec3 p=s.self->read<Vec3>(candidate.address+0x10)+Vec3{0,0,.5f};
  if(!finite(p))return;
  const Vec3 d=s.to-s.from;const float l=length_squared(d);
  const Vec3 a=p-s.from;const float u=l>.00001f?std::clamp((a.x*d.x+a.y*d.y+a.z*d.z)/l,0.f,1.f):0;
  const float radius=1.1f;
  if(length_squared(p-(s.from+d*u))<radius*radius)s.hit=Contact{s.from+d*u,candidate,{}};
 };
 each_enemy(&search,visit,true);
 if(search.hit)return search.hit;
 if(auto point=ray(from,to,t.address))return Contact{*point,{},read<Vec3>(level(0x173e80))};
 return std::nullopt;
}
bool NativeWorld::hit(Target t,float damage,Vec3 direction,unsigned attacker,bool explosive){
 const bool allowed=explosive?golden_damage_class(t.klass):capturable_class(int(t.klass))||destructible_crate_class(int(t.klass));
 if(!identity_valid(t)||!allowed||!finite(direction)||!std::isfinite(damage)||damage<=0||
    !Memory::valid(attacker,256)||t.address==body())return false;
 Stack stack(api_,48);if(!stack.at)return false;
 std::memset(api_->memory+stack.at,0,48);
 // SDK stack addresses are mapped above console RAM. Only the owning Stack
 // may access this span; ordinary entity-memory bounds must stay conservative.
 stack.put(0,direction);stack.put(12,5627.925f);
 const unsigned flags=explosive?0x830000u:0x30000u;
 stack.put(0x10,attacker);stack.put(0x14,flags);
 stack.put<unsigned char>(0x18,static_cast<unsigned char>(explosive?2:0));
 stack.put<unsigned char>(0x19,static_cast<unsigned char>(explosive?1:3));stack.put<unsigned short>(0x1a,2017);
 stack.put(0x1c,damage);stack.put<unsigned>(0x20,explosive?flags:1u);
 post_hit_(t.address,stack.at);
 return true;
}
void NativeWorld::impact(Target t,std::optional<Target> other,float charge){
 base_impact(t,other,charge,{token(weapon()),token(body()),damage_multiplier_,read<float>(hero()+0x98)});
}
unsigned NativeWorld::shot_attacker(const Shot& shot)const{
 if(identity_valid(shot.weapon))return shot.weapon.address;
 // Switching item removes/recycles the old held moby. Keep the original hero
 // generation as a safe attacker, with explicit source class2017 in the packet.
 return identity_valid(shot.body)?shot.body.address:0;
}
void NativeWorld::base_impact(Target t,std::optional<Target> other,float charge,const Shot& shot){
 if(!std::isfinite(charge)||!std::isfinite(shot.yaw))return;
 const Vec3 direction{std::cos(shot.yaw)*2,std::sin(shot.yaw)*2,1.3f};
 submit_hit(t,(3.f+charge*5.f)*shot.multiplier,direction,shot);
 if(other&&other->address!=t.address&&!frozen(other->address))submit_hit(*other,(3.f+charge*3.f)*shot.multiplier,direction,shot);
 if(trace_)log("native impact posted");
}
bool NativeWorld::submit_hit(Target target,float damage,Vec3 direction,const Shot& shot,bool explosive){
 if(!identity_valid(target)||frozen(target.address)||!finite(direction)||!std::isfinite(damage)||damage<=0)return false;
 if(deferred_size_||frame_hits_>=GoldenBlastPlan::maximum_hits){
  if(deferred_size_==deferred_.size())return false;
  deferred_[(deferred_head_+deferred_size_)%deferred_.size()]={target,shot,damage,direction,explosive};++deferred_size_;return true;
 }
 const auto attacker=shot_attacker(shot);
 if(!attacker||!hit(target,damage,direction,attacker,explosive))return false;
 ++frame_hits_;if(explosive)++golden_hits_;return true;
}
void NativeWorld::drain_hits(){
 while(deferred_size_&&frame_hits_<GoldenBlastPlan::maximum_hits){
  const auto pending=deferred_[deferred_head_];deferred_[deferred_head_]={};
  deferred_head_=(deferred_head_+1)%deferred_.size();--deferred_size_;
  if(!identity_valid(pending.target)||frozen(pending.target.address))continue;
  const auto attacker=shot_attacker(pending.shot);
  if(attacker&&hit(pending.target,pending.damage,pending.direction,attacker,pending.explosive)){
   ++frame_hits_;if(pending.explosive)++golden_hits_;
  }
 }
}
void NativeWorld::launched(Target target,float,float blast_charge){
 if(!valid(target)||flying(target)||!std::isfinite(blast_charge))return;
 Flight* entry=nullptr;for(auto& item:flights_)if(!item.target){entry=&item;break;}
 if(!entry)return; // Controller preflights capacity before spending attack ammo.
 const Shot shot{token(weapon()),token(body()),damage_multiplier_,last_frame_.yaw};
 *entry={};entry->target=target;entry->shot=shot;
 entry->impact.launch(target,golden_,blast_charge,damage_multiplier_);
 if(lease_.target&&lease_.target==target){entry->lease=lease_;entry->lease.flying=true;lease_={};}
 recoil_body_=shot.body;recoil_.start(last_frame_);enforce_release_recoil();
}
void NativeWorld::impact_result(Target target,const ImpactContext& result){
 auto* entry=flight(target);if(!entry)return;
 const Shot shot=entry->shot;
 const auto blast=entry->impact.consume(target,result.reason);
 restore_lease(entry->lease,false);*entry={}; // Clear before native FX/damage reentry.
 if(blast)explode(target,result,*blast,shot);
 else base_impact(target,result.other,result.charge,shot);
}
void NativeWorld::explode(Target target,const ImpactContext& result,const GoldenBlast& blast,const Shot& shot){
 if(!finite(result.point)||!identity_valid(target))return;
 const auto generation=cancellation_generation_;
 Vec3 centre=result.point;
 if(finite(result.normal)&&length_squared(result.normal)>.25f)
  centre=centre+unit(result.normal)*.06f; // Lift terrain contacts out of the surface.
 GoldenBlastPlan plan(blast,centre);
 plan.consider(target,centre,0,true,true);
 if(result.other&&identity_valid(*result.other)&&!flying(*result.other)&&!frozen(result.other->address))
  plan.consider(*result.other,damage_centre(*result.other),collision_radius(*result.other),true,true);
 struct Search {NativeWorld* self;GoldenBlastPlan* plan;Vec3 centre;float radius;unsigned ignore;};
 Search search{this,&plan,centre,blast.radius,target.address};
 each_damageable(&search,+[](const void* ctx,Target candidate){
  const auto& s=*static_cast<const Search*>(ctx);auto& w=*s.self;
  if(w.flying(candidate)||w.frozen(candidate.address))return;
  const auto position=w.damage_centre(candidate);const float radius=w.collision_radius(candidate);
  if(!finite(position)||length_squared(position-s.centre)>(s.radius+radius)*(s.radius+radius))return;
  s.plan->consider(candidate,position,radius,w.visible_from(s.centre,candidate,s.ignore));
 });
 // cart_burst emits particles/shake/light only, unlike the all-in-one native
 // explosion helper whose positive radius would also damage players and NPCs.
 Stack position(api_,16);
 if(position.at){
  position.put(0,std::array<float,4>{centre.x,centre.y,centre.z,0});
  burst_(target.address,position.at,-1,blast.visual_scale,blast.flash);
  ++golden_explosions_;
 }
 if(generation!=cancellation_generation_)return;
 const unsigned attacker=shot_attacker(shot);
 for(std::size_t i=0;i<plan.size();++i){
  const auto& hit=plan.hits()[i];
  if(!identity_valid(hit.target)||!attacker)continue;
  submit_hit(hit.target,hit.damage,hit.direction,shot,true);
 }
 if(trace_){char text[240];std::snprintf(text,sizeof text,"Golden impact: charge=%.3f radius=%.2f damage=%.2f scale=%.2f targets=%zu source=%08x",blast.charge,blast.radius,blast.damage,blast.visual_scale,plan.size(),attacker);log(text);}
}
void NativeWorld::observe_frame(const Frame& frame){
 const bool same_epoch=last_frame_.epoch==frame.epoch;
 last_frame_=frame;recoil_.tick(frame);frame_hits_=0;
 if(same_epoch&&frame.playing&&!frame.paused&&frame.alive&&finite(frame.position)&&
    std::isfinite(frame.yaw)&&std::isfinite(frame.seconds)&&frame.seconds>0&&frame.seconds<=.1f)drain_hits();
 if(!frame.equipped)arm_.reset();
}
bool NativeWorld::release_recoil_active()const{
 if(!recoil_.active()||!active_||recoil_body_.klass!=0||api_->loaded_level()!=level_||!identity_valid(recoil_body_)||
    recoil_body_.address!=body()||!Memory::valid(hero(),0x22ac))return false;
 const int state=read<int>(hero()+0x2084);
 return read<int>(mode())==0&&read<int>(hero()+0x22a8)>0&&state!=0x3d&&state!=0x82&&state!=0x77&&
  read<int>(hero()+0x10b8)==18&&read<int>(hero()+0x10b4)==2&&read<unsigned char>(hero()+0x20a4)==0;
}
void NativeWorld::enforce_release_recoil(){
 if(!recoil_.active()||read<int>(mode())==3)return;
 if(!release_recoil_active()){recoil_.cancel();recoil_body_={};return;}
 // Native HeroMotion / Hero from rnc/gameplay/hero.h. Preserve velocity.z and
 // vertical integration, platform carry, position, camera input and all AI.
 for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u,0x1d20u,0x1d24u})write<float>(hero()+offset,0);
}
void NativeWorld::attack_cancelled(){recoil_.cancel();recoil_body_={};arm_.reset();}
void NativeWorld::cancelled(){
 ++cancellation_generation_;
 restore_lease(lease_,true);
 for(auto& entry:flights_){restore_lease(entry.lease,true);entry={};}
 deferred_={};deferred_head_=deferred_size_=frame_hits_=0;attack_cancelled();
}
void NativeWorld::set_damage_multiplier(float value){damage_multiplier_=std::isfinite(value)?std::clamp(value,1.f,16.f):1.f;}
bool NativeWorld::blocks_native_hit(unsigned hit)const{
 // Built-in Walloper swing remains the attack animation; its legacy impact must
 // not kill the enemy before capture. Our posted impacts name the weapon moby.
 return Memory::valid(hit,0x40)&&read<unsigned>(hit+0x20)==body()
  &&read<unsigned short>(hit+0x2a)!=2017
  &&(read<unsigned>(hit+0x24)&0x30000)!=0&&read<int>(hero()+0x10b8)==18;
}
Frame NativeWorld::snapshot(bool primary,unsigned epoch)const{
 Frame f;f.epoch=epoch;f.seconds=read<float>(global(0x15ed7c));
 f.equipped=active_&&read<int>(hero()+0x10b8)==18&&read<int>(hero()+0x10b4)==2&&read<unsigned char>(hero()+0x20a4)==0;
 f.playing=read<int>(mode())==0;f.paused=read<int>(mode())==3;
 const int state=read<int>(hero()+0x2084);
 f.alive=read<int>(hero()+0x22a8)>0&&state!=0x3d&&state!=0x82&&state!=0x77;
 f.primary=primary;f.grounded=state==0||state==1||state==2||state==3||state==0x20;
 f.position=read<Vec3>(hero()+0x80);f.yaw=read<float>(hero()+0x98);return f;
}
void NativeWorld::animate(Motion motion){
 if(motion==Motion::DryFire){rotor_.dry_fire();return;}
 if(api_->loaded_level()<0||api_->loaded_level()>18)return;
 if(motion==Motion::Strike){arm_.reset();state_(0x20,1);if(trace_)log("native Walloper strike");}
 if(motion==Motion::Release){
  // Do not enter wrench-boomerang recovery: its returning-weapon wait would
  // delay the enemy. Only our short horizontal recoil hold accompanies release.
  arm_.release();
  if(read<int>(hero()+0x2084)==0x20)state_(0,1);
  enforce_release_recoil();
  if(trace_)log("throw released immediately; Ratchet recoil 0.15 seconds; independent flight");
 }
 if(motion==Motion::Recover&&!recoil_.active()&&!arm_.releasing())arm_.reset();
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
 cancelled();arm_.reset(true);level_=api_->loaded_level();last_frame_={};
}
}

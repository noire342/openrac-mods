// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_world.hpp"
#include "builtin_catalog.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace salvage {
namespace {
constexpr std::array<unsigned,19> Relays{{0x2422d8,0x256268,0x243dc8,0x230518,0x234c48,0x26b298,0x250f68,0x2698b0,0x24be40,0x2612c8,0x234cd0,0x2675f8,0x25a448,0x24d4d8,0x249e68,0x231868,0x23d918,0x23ca48,0x244550}};
struct Stack {
 const openrac_api* api;unsigned saved{},at{},bytes{};
 Stack(const openrac_api* a,unsigned size):api(a),bytes(size){at=api->stack_push(size,&saved);}
 ~Stack(){if(at)api->stack_pop(saved);}
 // SDK-owned stack frames live in OpenRAC's mapped host region (E0000000),
 // outside the console's32MiB actor RAM. Bound these writes to this frame.
 template<class T>void put(unsigned offset,const T& value){if(at&&offset<=bytes&&sizeof(T)<=bytes-offset)std::memcpy(api->memory+at+offset,&value,sizeof value);}
};
// Collision queries may run between native actors. Preserve the previous hit
// result, but keep the kernel's monotonically advancing query stamp intact.
struct CollisionResultScope {
 const openrac_api* api;unsigned at{};std::array<unsigned char,104> old{};
 CollisionResultScope(const openrac_api* a,unsigned base):api(a),at(base+0x18){if(NativeMemory::valid(at,old.size()))std::memcpy(old.data(),api->memory+at,old.size());else at=0;}
 ~CollisionResultScope(){if(at)std::memcpy(api->memory+at,old.data(),old.size());}
};
Vec3 rotated(const std::array<float,12>& r,Vec3 v){return {r[0]*v.x+r[4]*v.y+r[8]*v.z,r[1]*v.x+r[5]*v.y+r[9]*v.z,r[2]*v.x+r[6]*v.y+r[10]*v.z};}
Vec3 unrotated(const std::array<float,12>& r,Vec3 v){return {r[0]*v.x+r[1]*v.y+r[2]*v.z,r[4]*v.x+r[5]*v.y+r[6]*v.z,r[8]*v.x+r[9]*v.y+r[10]*v.z};}
}
NativeWorld::NativeWorld(const openrac_api* api,openrac_mod* mod,GuestArena& arena,Catalog& catalog,World& world,Events* events)
 :NativeMemory(api),mod_(mod),arena_(arena),catalog_(catalog),world_(world),events_(events){
 ray_=fn<Ray>("func_001EFE10");sphere_=fn<Sphere>("func_L00_001F10E0");capsule_=fn<Capsule>("func_L00_001F1D20");
 matrix_=fn<void(*)(unsigned)>("func_L00_00251E30");basis_=fn<void(*)(unsigned,unsigned)>("func_001FA1F8");
 remove_=fn<void(*)(unsigned)>("func_0020D678");create_=fn<unsigned(*)(int)>("func_0020D348");
}
void NativeWorld::log(const char* text)const{if(api_->log)api_->log(mod_,OPENRAC_LOG_INFO,text);}
void NativeWorld::before_load(int next){detach(true);retired_callbacks_.clear();level_=-1;relay_=0;free_colliders_.clear();if(next>=0&&next<=18)world_.begin_visit(next);else world_.end_visit();if(world_.all().empty())visuals_.clear();}
void NativeWorld::after_load(int level){level_=level;if(level>=0&&level<=18){relay_=Relays[unsigned(level)];world_.begin_visit(level);}else relay_=0;}
void NativeWorld::explicit_load(){detach(true);visuals_.clear();world_.begin_visit(api_->loaded_level(),true);}
bool NativeWorld::callback_valid(const Actor& a)const{
 return a.address&&api_->loaded_level()==level_&&valid(a.address,256)&&read<unsigned char>(a.address+0x20)<0xfe&&
  read<unsigned>(a.address+0x24)==a.pclass&&read<unsigned>(a.address+0x78)==a.pvars&&
  read<unsigned>(a.address+0x74)==relay_&&arena_.owns(a.collision,48);
}
bool NativeWorld::actor_valid(const Actor& a)const{
 return callback_valid(a)&&
  (read<unsigned>(a.address+0x94)==a.collision||(collision_suppressed_&&read<unsigned>(a.address+0x94)==0));
}
bool NativeWorld::owns(unsigned moby)const{for(const auto& [id,a]:actors_){(void)id;if(a.address==moby&&actor_valid(a))return true;}return false;}
bool NativeWorld::owns_callback(unsigned moby)const{
 for(const auto& [id,a]:actors_){(void)id;if(a.address==moby&&callback_valid(a))return true;}
 for(const auto& [generation,d]:detached_){(void)generation;if(d.actor.address==moby&&callback_valid(d.actor))return true;}
 const auto retired=retired_callbacks_.find(moby);
 if(retired!=retired_callbacks_.end()&&valid(moby,256)&&read<unsigned char>(moby+0x20)>=0xfd&&read<unsigned>(moby+0x74)==0&&read<unsigned>(moby+0x24)==retired->second.pclass&&read<unsigned>(moby+0x78)==retired->second.pvars)return true;
 return false;
}
unsigned NativeWorld::native_actor(InstanceId id)const{auto i=actors_.find(id);return i!=actors_.end()&&actor_valid(i->second)?i->second.address:0;}
unsigned NativeWorld::collider(){if(!free_colliders_.empty()){const auto at=free_colliders_.back();free_colliders_.pop_back();return at;}return arena_.allocate(48);}
void NativeWorld::release_actor(Actor& a,bool remove){
 // During disc loading the previous arena can already hold compressed WAD
 // input. Retire only the host handle; even an apparently matching actor or
 // callback no longer grants ownership of those guest bytes.
 if(level_transitioning()){a={};return;}
 if(remove&&callback_valid(a)){
  // An engine dispatch may already have fetched this relay when input retires
  // the actor. Clear future callbacks and absorb that final queued invocation.
  retired_callbacks_[a.address]=a;write<unsigned>(a.address+0x74,0);remove_(a.address);
 }else if(valid(a.address,256)&&read<unsigned char>(a.address+0x20)>=0xfd&&read<unsigned>(a.address+0x74)==relay_&&read<unsigned>(a.address+0x24)==a.pclass&&read<unsigned>(a.address+0x78)==a.pvars){retired_callbacks_[a.address]=a;write<unsigned>(a.address+0x74,0);}
 // The collision allocation belongs to this instance even after its native
 // slot has been recycled. Never touch the new native occupant in that case.
 if(arena_.owns(a.collision,48))free_colliders_.push_back(a.collision);a={};
}
void NativeWorld::detach(bool remove){
 for(auto& [id,a]:actors_){(void)id;release_actor(a,remove);}actors_.clear();
 for(auto& [generation,d]:detached_){(void)generation;world_.release(d.lease);release_actor(d.actor,remove);}detached_.clear();
}
std::optional<CannonNativeBody> NativeWorld::cannon_describe(unsigned actor)const{
 for(const auto& [id,a]:actors_)if(a.address==actor&&actor_valid(a)){
  const auto* fragment=world_.find(id);if(!fragment||world_.claimed(id))return {};
  return CannonNativeBody{{},*fragment,a.address,a.pclass,a.pvars,a.collision,relay_,a.scale,a.local_offset};
 }return {};
}
bool NativeWorld::cannon_replace_class(unsigned actor,unsigned expected,unsigned replacement){
 if(!valid(replacement,0x48))return false;
 auto change=[&](Actor& a){if(a.address!=actor||a.pclass!=expected||!actor_valid(a))return false;write(actor+0x24,replacement);a.pclass=replacement;return true;};
 for(auto& [id,a]:actors_){(void)id;if(change(a))return true;}
 for(auto& [generation,d]:detached_){(void)generation;if(change(d.actor))return true;}
 return false;
}
std::optional<CannonNativeBody> NativeWorld::cannon_claim(unsigned actor){
 auto body=cannon_describe(actor);if(!body)return {};
 if(world_.claim(body->fragment.id,body->fragment.visit,body->lease)!=Status::Ok)return {};
 const auto found=actors_.find(body->fragment.id);
 try{detached_.emplace(body->lease.generation,Detached{body->lease,found->second});}catch(...){world_.release(body->lease);throw;}
 actors_.erase(found);return body;
}
bool NativeWorld::cannon_cancel(const CannonNativeBody& body,bool native_valid){
 const auto found=detached_.find(body.lease.generation);
 // A new factory observation or visit reset can already have cancelled this
 // exact lease. Acknowledge it idempotently without touching the replacement.
 if(found==detached_.end())return body.lease.generation&&!world_.owns(body.lease);
 if(found->second.actor.address!=body.actor||found->second.lease.instance!=body.lease.instance||found->second.lease.visit!=body.lease.visit||!world_.owns(body.lease))return false;
 auto* fragment=world_.find(body.lease.instance);auto actor=found->second.actor;
 if(native_valid&&actor_valid(actor)){
  const auto centre=read<Vec3>(body.actor)*(1.f/1024.f),rotation=read<Vec3>(body.actor+0x40);
  if(finite(centre)&&finite(rotation)){fragment->position=centre;fragment->rotation=rotation;fragment->velocity={};fragment->sleeping=fragment->suspended=false;fragment->quiet_seconds=0;}
  actors_.emplace(body.lease.instance,actor);detached_.erase(found);world_.release(body.lease);return true;
 }
 // A recycled native slot is never restored or deleted. Retain the logical
 // part so it can materialize again, and return only our old collision budget.
 release_actor(actor,false);detached_.erase(found);world_.release(body.lease);return true;
}
bool NativeWorld::cannon_capture(const CannonNativeBody& body){
 const auto found=detached_.find(body.lease.generation);if(found==detached_.end()||found->second.actor.address!=body.actor||found->second.lease.instance!=body.lease.instance||found->second.lease.visit!=body.lease.visit)return false;
 if(!world_.take(body.lease))return false;visuals_.erase(body.lease.instance);return true;
}
void NativeWorld::cannon_retire(const CannonNativeBody& body){
 const auto found=detached_.find(body.lease.generation);if(found==detached_.end()||found->second.actor.address!=body.actor||found->second.lease.instance!=body.lease.instance||found->second.lease.visit!=body.lease.visit)return;
 auto actor=found->second.actor;release_actor(actor,false);detached_.erase(found);
}
bool NativeWorld::resources(unsigned klass)const{
 if(klass>=0x10000)return false;
 const auto slot=read<unsigned char>(g(0x1b3e40)+klass);
 return slot<224&&slot<read<int>(g(0x160000))&&read<unsigned short>(g(0x1b3c80)+unsigned(slot)*2)==klass&&valid(read<unsigned>(g(0x1b3580)+unsigned(slot)*4),0x48);
}
NativeWorld::Actor NativeWorld::attach(unsigned moby,const Fragment& fragment,const PartDescriptor& part,Vec3 offset){
 if(!valid(moby,256)||!relay_)return {};
 const auto pc=read<unsigned>(moby+0x24),pv=read<unsigned>(moby+0x78);const float scale=read<float>(moby+0x2c);
 if(!valid(pc,0x48)||!valid(pv,1)||!std::isfinite(scale)||scale<=0)return {};
 const auto coll=collider();if(!coll)return {};
 Actor actor{moby,pc,pv,coll,scale,offset};
 std::memset(api_->memory+coll,0,48);write<int>(coll+4,32); // one primitive; no triangle mesh
 write<unsigned char>(coll+0x10,1);write<unsigned short>(coll+0x12,0x8003); // sphere, both query groups, terminal
 write(coll+0x20,offset*(1024.f/scale));write<float>(coll+0x2c,part.radius*1024.f/scale);
 write<unsigned>(moby+0x74,relay_);write<unsigned>(moby+0x94,coll);write<unsigned>(moby+0x98,0);
 write<unsigned char>(moby+0x20,1);write<unsigned char>(moby+0x23,0x80);write<unsigned char>(moby+0x30,0xff);
 write<short>(moby+0x32,0x7fff);write<unsigned char>(moby+0xb0,0xff);write<unsigned short>(moby+0xb2,0xffff);
 write<unsigned short>(moby+0x34,read<unsigned short>(moby+0x34)&~0x4001u); // no loot damage/hidden flag
 sync(actor,fragment,part);return actor;
}
bool NativeWorld::adopt(unsigned source,unsigned moby){
 if(!valid(source,256)||!valid(moby,256))return false;
 const auto pv=read<unsigned>(moby+0x78);const float dt=read<float>(g(Delta));
 if(!valid(pv,0x40)||!std::isfinite(dt)||dt<=0||dt>.1f)return false;
 const auto velocity=read<Vec3>(pv)*(1.f/dt),angular=read<Vec3>(pv+0x10)*(1.f/dt);
 return adopt({read<unsigned short>(source+0xa6),read<unsigned short>(moby+0xa6)},moby,{finite(velocity)?velocity:Vec3{},finite(angular)?angular:Vec3{}});
}
bool NativeWorld::adopt(FragmentOrigin origin,unsigned moby,const FragmentMotion& motion){
 if(level_<0||level_>18||api_->loaded_level()!=level_||!valid(moby,256)||!relay_||!arena_.begin()||!finite(motion.velocity)||!finite(motion.angular_velocity))return false;
 if(owns(moby))return true;
 retired_callbacks_.erase(moby);
 const auto source_class=origin.source_class,fragment_class=origin.fragment_class;
 if(read<unsigned short>(moby+0xa6)!=fragment_class||!valid(read<unsigned>(moby+0x24),0x48))return false;
 if(!mechanical_source(source_class)||!builtin_part(source_class,fragment_class))return false;
 // A new native factory observation is a new allocation generation. The same
 // address can replace a part that was in transit: release only that old
 // handle/lease, retaining its logical component for rematerialization.
 for(auto i=detached_.begin();i!=detached_.end();){if(i->second.actor.address==moby){world_.release(i->second.lease);release_actor(i->second.actor,false);i=detached_.erase(i);}else ++i;}
 for(auto i=actors_.begin();i!=actors_.end();){if(i->second.address==moby){release_actor(i->second,false);i=actors_.erase(i);}else ++i;}
 // Alternative factories can return before their first normal update has
 // built the class/animation bounds. Fit to the actual mesh at this transform.
 basis_(moby+0xc0,moby+0x40);matrix_(moby);
 const float radius=read<float>(moby+12)/1024.f;
 const float fitted=std::isfinite(radius)&&radius>.02f?std::clamp(radius,.06f,20.f):.2f;
 auto descriptor=builtin_part(source_class,fragment_class,fitted,0,std::max(.1f,fitted*fitted*fitted*8));
 if(!descriptor)return false;
 const auto id=descriptor->id;
 if(!catalog_.find(id)&&catalog_.register_part(*descriptor)!=Status::Ok)return false;
 const auto part=catalog_.find(id);if(!part)return false;
 const auto pos=read<Vec3>(moby+0x10);auto centre=read<Vec3>(moby)*(1.f/1024.f);
 // The native matrix rebuild supplied these bounds. Long, off-centre models
 // can legitimately have a pivot farther than five world units from them.
 if(!finite(centre))centre=pos;
 if(!finite(pos))return false;
 const auto instance=world_.spawn(id,{centre,motion.velocity,read<Vec3>(moby+0x40),motion.angular_velocity});
 if(!instance)return false;
 const auto offset=unrotated(read<std::array<float,12>>(moby+0xc0),centre-pos);
 visuals_[instance]={read<float>(moby+0x2c),offset};
 const auto actor=attach(moby,*world_.find(instance),*part,offset);
 if(actor.address){actors_[instance]=actor;if(materialized_)materialized_(moby);}
 else {
  // Budget pressure must not destroy the logical component. Retire the
  // unowned timed fragment and rematerialize the record when space returns.
  remove_(moby);
 }
 ++adoptions_;
 if(events_)events_->publish({0,EventKind::Spawned,id,instance,1,0,0,world_.visit()});
 char text[192];std::snprintf(text,sizeof text,"salvage adopted source=%u fragment=%u instance=%llu radius=%.3f",unsigned(source_class),unsigned(fragment_class),static_cast<unsigned long long>(instance),part->radius);log(text);
 return true;
}
void NativeWorld::sync(Actor& actor,const Fragment& fragment,const PartDescriptor& part){
 if(!actor.address)return;
 write(actor.address+0x40,fragment.rotation);basis_(actor.address+0xc0,actor.address+0x40);
 const auto offset=rotated(read<std::array<float,12>>(actor.address+0xc0),actor.local_offset);
 write(actor.address+0x10,fragment.position-offset);
 matrix_(actor.address);
 std::array<float,4> sphere{{fragment.position.x*1024,fragment.position.y*1024,fragment.position.z*1024,part.radius*1024}};
 write(actor.address,sphere);write<unsigned char>(actor.address+0x23,0x80);
}
PlayerBody NativeWorld::player()const{
 const auto hero=g(Hero);PlayerBody result;
 result.position=read<Vec3>(hero+0x80);
 const auto dt=read<float>(g(Delta));result.velocity=read<Vec3>(hero+0xe0)*(dt>0?1.f/dt:0.f);
 const int state=read<int>(hero+0x2084);
 result.enabled=valid(read<unsigned>(hero+0x2080),256)&&read<int>(hero+0x22a8)>0&&read<int>(mode())==0&&
  read<unsigned char>(hero+0x20a4)==0&&state!=0x3d&&state!=0x82&&state!=0x77;
 return result;
}
// Native interaction priority belongs to the presentation adapter, which reads
// the actual live Triangle prompt and dialog channel rather than guessed NPC
// class/radius lists. World supplies only character and gameplay eligibility.
CollectionInput NativeWorld::input(bool triangle)const{const auto p=player();return {triangle,read<int>(mode())==0,p.enabled,false,p.position+Vec3{0,0,p.radius+p.half_height}};}
bool NativeWorld::visible(const Fragment& f)const{
 const auto p=player();if(!p.enabled||!finite(f.position)||!native_actor(f.id))return false;
 const Vec3 from=p.position+Vec3{0,0,p.radius+p.half_height};Stack stack(api_,32);if(!stack.at)return false;
 stack.put(0,std::array<float,4>{{from.x,from.y,from.z,0}});stack.put(16,std::array<float,4>{{f.position.x,f.position.y,f.position.z,0}});
 const auto output=l(0,0x173f40);CollisionResultScope saved(api_,output);
 if(!ray_(stack.at,stack.at+16,4,read<unsigned>(g(Hero)+0x2080),0))return true;
 return read<unsigned>(output+0x18)==native_actor(f.id);
}
std::optional<Contact> NativeWorld::sweep_sphere(Vec3 from,Vec3 to,float radius,InstanceId ignore){
 if(!finite(from)||!finite(to)||!std::isfinite(radius)||radius<=0)return std::nullopt;
 Stack stack(api_,32);if(!stack.at)return Contact{from,{0,0,1},{},0};
 stack.put(0,std::array<float,4>{{from.x,from.y,from.z,0}});stack.put(16,std::array<float,4>{{to.x,to.y,to.z,0}});
 const auto output=l(0,0x173f40);CollisionResultScope saved(api_,output);const auto actor=native_actor(ignore);
 // Bit0x20 filters out surface type0, including normal ground and walls.
 // Physical parts and collection visibility must include those surfaces.
 const int line_hit=length(to-from)>.00001f?ray_(stack.at,stack.at+16,4,actor,0):0;
 if(line_hit){
  auto n=read<Vec3>(output+0x40);const float d=length(n);if(d>.001f)n=n*(1.f/d);else n={0,0,1};
  const auto hit=read<Vec3>(output+0x20);const auto center=hit+n*radius;
  const float fraction=std::clamp(length(center-from)/std::max(.00001f,length(to-from)),0.f,1.f);
  if(finite(center))return Contact{center,n,{},fraction};
 }
 const int sphere_hit=sphere_(radius,stack.at+16,4,actor);
 if(sphere_hit){
  auto n=read<Vec3>(output+0x40);const float d=length(n);if(d>.001f)n=n*(1.f/d);else n={0,0,1};
  const auto center=read<Vec3>(output+0x30);if(finite(center))return Contact{center,n,{},1};
 }
 return std::nullopt;
}
void NativeWorld::correct_player(Vec3 delta){
 if(!finite(delta)||length(delta)<.00001f)return;const auto p=player();if(!p.enabled)return;
 const float size=length(delta);if(size>.25f)delta=delta*(.25f/size);
 const auto next=p.position+delta;Stack stack(api_,16);if(!stack.at)return;
 auto at=next+Vec3{0,0,p.radius};stack.put(0,std::array<float,4>{{at.x,at.y,at.z,0}});
 CollisionResultScope saved(api_,l(0,0x173f40));
 if(capsule_(p.radius,2*p.half_height,stack.at,4,read<unsigned>(g(Hero)+0x2080)))return;
 write(g(Hero)+0x80,next);const auto body=read<unsigned>(g(Hero)+0x2080);if(valid(body,256)){write(body+0x10,next);matrix_(body);}++pushes_;
}
void NativeWorld::retire_collected(){
 for(auto it=actors_.begin();it!=actors_.end();){if(!world_.find(it->first)){visuals_.erase(it->first);release_actor(it->second,true);it=actors_.erase(it);}else ++it;}
}
void NativeWorld::frame(){
 const int current=api_->loaded_level();if(current<0||current>18)return;
 if(current!=level_)after_load(current);if(!arena_.begin())return;
 // Pausing switches the engine's active Moby globals to its menu actor pool.
 // World allocation and retirement must run only in the gameplay context.
 if(read<int>(mode())!=0)return;
 retire_collected();
 for(auto it=actors_.begin();it!=actors_.end();){if(!actor_valid(it->second)){release_actor(it->second,true);it=actors_.erase(it);}else ++it;}
 const auto p=player();std::vector<InstanceId> current_ids;current_ids.reserve(actors_.size());for(const auto& [id,a]:actors_){(void)a;current_ids.push_back(id);}
 const auto desired=materialization_.select(world_,p.position,current_ids,[this](const Fragment& fragment){
  const auto existing=actors_.find(fragment.id);if(existing!=actors_.end()&&actor_valid(existing->second))return true;
  const auto* part=catalog_.find(fragment.part);return part&&resources(part->fragment_class);
 });
 const std::set<InstanceId> desired_set(desired.begin(),desired.end());
 for(auto it=actors_.begin();it!=actors_.end();){if(!desired_set.count(it->first)){
  if(auto* fragment=world_.find(it->first))fragment->suspended=true;
  release_actor(it->second,true);it=actors_.erase(it);
 }else ++it;}
 // Retry optional native attachments after a slot was recycled or resource
 // pressure eased. Consumers must make an already-attached actor idempotent.
 if(materialized_)for(const auto& [id,a]:actors_){(void)id;if(actor_valid(a))materialized_(a.address);}
 // API-created parts and same-planet respawn records materialize only when the
 // actual source mesh exists in this map. Native pool exhaustion retains data.
 for(const auto id:desired)if(!actors_.count(id)&&!world_.claimed(id)){
  const auto* record=world_.find(id);if(!record)continue;const auto fragment=*record;
  const auto part=catalog_.find(fragment.part);if(!part||!resources(part->fragment_class))continue;
  const auto moby=create_(int(part->fragment_class));if(!valid(moby,256))break;
  auto visual=visuals_.find(id);
  if(visual==visuals_.end()){
   // A level script can create a part before any native death has supplied
   // its visual transform. Derive it from the actual class/animation bounds.
   write(moby+0x40,fragment.rotation);basis_(moby+0xc0,moby+0x40);matrix_(moby);
   const float radius=read<float>(moby+12)/1024.f,scale=read<float>(moby+0x2c);
   if(std::isfinite(radius)&&radius>.0001f&&std::isfinite(scale)&&scale>0){write(moby+0x2c,scale*part->radius/radius);matrix_(moby);}
   const auto centre=read<Vec3>(moby)*(1.f/1024.f),origin=read<Vec3>(moby+0x10);
   const auto offset=finite(centre)&&finite(origin)?unrotated(read<std::array<float,12>>(moby+0xc0),centre-origin):Vec3{};
   visual=visuals_.emplace(id,Visual{read<float>(moby+0x2c),offset}).first;
  }
  if(visual!=visuals_.end())write(moby+0x2c,visual->second.scale);
  const auto offset=visual!=visuals_.end()?visual->second.local_offset:Vec3{};
  const auto actor=attach(moby,fragment,*part,offset);if(actor.address){
   actors_[id]=actor;if(auto* resumed=world_.find(id)){resumed->sleeping=false;resumed->suspended=false;resumed->quiet_seconds=0;}
   if(materialized_)materialized_(moby);
  }else {remove_(moby);break;}
 }
 if(!p.enabled)return;const float dt=read<float>(g(Delta));
 // Core resolves part/part contacts itself. Hide only our own collision blobs
 // for these world queries, then restore them before any native actor runs.
 std::vector<std::pair<unsigned,unsigned>> collisions;collisions.reserve(actors_.size());
 for(auto& [id,a]:actors_){(void)id;if(actor_valid(a)){collisions.push_back({a.address,a.collision});write<unsigned>(a.address+0x94,0);}}
 collision_suppressed_=true;
 PhysicsResult result;
 try{result=physics_.step(world_,catalog_,*this,dt,p);}catch(...){for(const auto [m,c]:collisions)if(read<unsigned>(m+0x74)==relay_)write(m+0x94,c);collision_suppressed_=false;throw;}
 for(const auto [m,c]:collisions)if(read<unsigned>(m+0x74)==relay_)write(m+0x94,c);
 collision_suppressed_=false;
 for(auto& [id,a]:actors_){const auto f=world_.find(id);const auto part=f?catalog_.find(f->part):nullptr;if(f&&part&&actor_valid(a))sync(a,*f,*part);}
 correct_player(result.player_correction);
}
}

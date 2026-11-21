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
// Verified against retail Hero/Moby layouts and hero_set_state/update_movement.
constexpr unsigned hero_state_offset=0x2084,hero_control_mode_offset=0x208c,
 hero_state_timer_offset=0x198,hero_item_locked_offset=0x20ac;
constexpr unsigned moby_destination_sequence_offset=0x53;
constexpr int hero_idle_state=0,hero_held_attack_state=0x20;
constexpr int walloper_hero_sequence=0x67,walloper_release_tick=18;
constexpr int hoven_level=12,blarg_gun_joint=1;
constexpr unsigned blarg_gun_class=499,trooper_attachment_offset=0x248;
constexpr unsigned moby_spawn_frame_offset=0x38;
constexpr unsigned moby_alpha_offset=0x23,moby_class_pointer=0x24,moby_flags_offset=0x34;
constexpr unsigned moby_update_offset=0x74,moby_collision_offset=0x94,moby_collision_disable_offset=0x98;
constexpr unsigned class_type_offset=0x46,creature_class_type=5,aim_target_flag=0x20;
constexpr unsigned hidden_moby_flags=0x81;
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
NativeWorld::NativeWorld(const openrac_api* api,openrac_mod* mod,NativeAmmunition& ammunition,NativeRotor& rotor,GuestArena& arena):Memory(api),arm_(api,arena),ammunition_(ammunition),rotor_(rotor),sparks_(api),crates_(api),
 gag_(api,mod,snagglebeast_crush),gag_camera_(api,mod,this,
  +[](const void* scene,Vec3 from,Vec3 to){
   const auto& world=*static_cast<const NativeWorld*>(scene);
   return world.ray(from,to,world.body());
  }),
 allocate_(function<Allocate>("FUN_001ff288")),post_hit_(function<Hit>("FUN_L00_00259a88")),
 ray_(function<Ray>("FUN_001efa68")),basis_(function<Basis>("FUN_001fa030")),
 state_(function<State>("FUN_L00_002223f8")),scale_ticks_(reinterpret_cast<ScaleTicks>(find_function("FUN_001f96f8"))),
 burst_(function<Burst>("FUN_L00_0025f090")),
 green_burst_(reinterpret_cast<GreenBurst>(find_function("FUN_L00_0025f3e8"))),
 sphere_visible_(reinterpret_cast<SphereVisible>(find_function("FUN_001fa728"))),
 mod_(mod),trace_(api->setting_bool(mod,"trace")!=0){drop_counterattack_=api->setting_bool(mod,"drop_counterattack")!=0;}
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
bool NativeWorld::capturable_enemy(Target t)const{
 if(capture_excluded_class(t.klass)||t.address==body())return false;
 if(capturable_class(int(t.klass)))return true;
 if(!(read<std::uint16_t>(t.address+moby_flags_offset)&aim_target_flag))return false;
 // The same target contract the Morph-o-Ray uses: a creature class, the
 // native aim flag and live health metadata. Creature type alone includes NPCs.
 const auto klass=read<unsigned>(t.address+moby_class_pointer);
 if(!Memory::valid(klass,class_type_offset+sizeof(std::uint16_t))||
    read<std::uint16_t>(klass+class_type_offset)!=creature_class_type)return false;
 const auto health=read<unsigned>(t.pvars);
 const auto hp=read<float>(health);
 return std::isfinite(hp)&&hp>0.f;
}
bool NativeWorld::valid(Target t)const{return identity_valid(t)&&(capturable_enemy(t)||crates_.live_crate(t));}
bool NativeWorld::enemy_can_be_latched(Target t)const{
 return read<unsigned>(t.address+moby_collision_offset)&&!read<unsigned>(t.address+moby_collision_disable_offset)&&
        read<std::uint8_t>(t.address+moby_alpha_offset)&&
        !(read<std::uint16_t>(t.address+moby_flags_offset)&hidden_moby_flags);
}
bool NativeWorld::golden_damage_target(Target t)const{
 return golden_damage_class(t.klass)||capturable_enemy(t);
}
bool NativeWorld::try_capture_gag(const Frame& frame){
 if(flight_count())return false;
 const auto boss=gag_.candidate(frame);
 if(!boss||!visible_from(frame.position+Vec3{0,0,.85f},boss,body())||
    !gag_.start(frame,token(body()),boss))return false;
 gag_camera_.begin(frame.position,gag_.origin(),frame.yaw,gag_.subject_height());
 return true;
}
void NativeWorld::each_enemy(const void* context,void(*fn)(const void*,Target),bool include_crates)const{
 const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
 if(!Memory::valid(first,256)||last<first||last-first>4096*256||!Memory::valid(last-256,256))return;
 for(auto m=first;m<last;m+=256){const auto t=token(m);if(identity_valid(t)&&(capturable_enemy(t)||(include_crates&&destructible_crate_class(int(t.klass)))))fn(context,t);}
}
void NativeWorld::each_damageable(const void* context,void(*fn)(const void*,Target))const{
 const auto first=read<unsigned>(global(0x15ff18)),last=read<unsigned>(global(0x15ff20));
 if(!Memory::valid(first,256)||last<first||last-first>4096*256||!Memory::valid(last-256,256))return;
 for(auto m=first;m<last;m+=256){
  const auto t=token(m);
  if(identity_valid(t)&&golden_damage_target(t))fn(context,t);
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
 return identity_valid(target)&&clear_path(from,damage_centre(target),target.address,ignore);
}
bool NativeWorld::clear_path(Vec3 from,Vec3 to,unsigned accepted_target,unsigned ignore)const{
 if(!finite(from)||!finite(to))return false;
 if(length_squared(to-from)<.0001f)return true;
 Stack stack(api_,32);if(!stack.at)return false; // Failure is not clear line of sight.
 stack.put(0,std::array<float,4>{from.x,from.y,from.z,0});
 stack.put(16,std::array<float,4>{to.x,to.y,to.z,0});
 if(!ray_(stack.at,stack.at+16,0x24,ignore,0))return true;
 return read<unsigned>(level(0x173e58))==accepted_target;
}
std::optional<Vec3> NativeWorld::ray(Vec3 from,Vec3 to,unsigned ignore,bool* clear)const{
 if(clear)*clear=false;
 Stack stack(api_,32);if(!stack.at||!finite(from)||!finite(to))return std::nullopt;
 const std::array<float,4> start{from.x,from.y,from.z,0},end{to.x,to.y,to.z,0};
 std::memcpy(api_->memory+stack.at,start.data(),16);std::memcpy(api_->memory+stack.at+16,end.data(),16);
 if(!ray_(stack.at,stack.at+16,0x24,ignore,0)){
  if(clear)*clear=true;
  return std::nullopt;
 }
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
  if(destructible_crate_class(int(target.klass))?!w.crates_.can_carry(target):!w.enemy_can_be_latched(target))return;
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
  bool clear=false;
  const auto obstruction=w.ray(start,centre,w.body(),&clear);
  const auto hit_owner=obstruction?w.read<unsigned>(w.level(0x173e58)):0;
  const bool hit_target=obstruction&&hit_owner==target.address;
  clear=clear||hit_target||(obstruction&&hit_owner==w.weapon());
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
 },crates_.ready());
 return search.best;
}
bool NativeWorld::capture(Target t){
 if(lease_.target||!valid(t)||flying(t)||!can_launch())return false;
 if(!(site_.target==t)||!finite(site_.point))return false;
 const bool crate=capturable_crate_class(t.klass);
 if(crate?!crates_.detach(t):!enemy_can_be_latched(t))return false;
 lease_={};lease_.target=t;lease_.collision=read<unsigned>(t.address+moby_collision_offset);
 // The native dispatcher accepts a null per-instance callback, while
 // animation still runs. This lease follows the actor token, not its pool slot.
 if(!crate){
  lease_.native_update=read<unsigned>(t.address+moby_update_offset);
  if(lease_.native_update)write<unsigned>(t.address+moby_update_offset,0);
 }
 lease_.attachment=trooper_gun(t);
 lease_.disabled=read<unsigned>(t.address+moby_collision_disable_offset);lease_.origin=read<Vec3>(t.address+0x10);
 lease_.angles=read<Vec3>(t.address+0x40);lease_.spin_angle=read<float>(hero()+0x98);
 lease_.grip=capture_grip(lease_.origin,lease_.angles,site_.point,site_.approach,lease_.spin_angle);
 const auto q=lease_.grip.original;
 lease_.local_sphere=rotate({-q[0],-q[1],-q[2],q[3]},read<Vec3>(t.address)*(1.f/1024.f)-lease_.origin);
 write<unsigned>(t.address+moby_collision_offset,0);write<unsigned>(t.address+moby_collision_disable_offset,1);
 if(trace_){char s[240];std::snprintf(s,sizeof s,"captured class=%u moby=%08x local-grip=(%.3f,%.3f,%.3f)",t.klass,t.address,lease_.grip.local_point.x,lease_.grip.local_point.y,lease_.grip.local_point.z);log(s);}
 if(audio_)audio_->emit(WeaponAudioCue::Capture);
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
 // A health or aim change can invalidate capture eligibility before the
 // native death hook runs. Ownership follows the actor until it is released.
 if(!moby)return false;
 if(lease_.target.address==moby&&identity_valid(lease_.target))return true;
 for(const auto& entry:flights_)if(entry.lease.target.address==moby&&identity_valid(entry.lease.target))return true;
 return false;
}
void NativeWorld::update_nanotech(unsigned orb,NativeCrates::Update next)const{
 const auto owner=crates_.nanotech_owner(orb);
 if(owner&&frozen(owner))crates_.update_carried_nanotech(orb,owner,next);
 else next(orb);
}
void NativeWorld::move(Target t,Vec3 p,float yaw){
 auto* lease=owned_lease(t);
 if(!lease||!valid(t)||!finite(p)||!std::isfinite(yaw))return;
 if(!lease->flying)lease->spin_angle=yaw;
 const auto pose=attach_to_tip(lease->grip,p,lease->spin_angle,std::clamp(lease->hold_time/.25f,0.f,1.f));
 move_origin(*lease,pose.origin,pose.angles);
}
Target NativeWorld::trooper_gun(Target trooper)const{
 if(level_!=hoven_level||trooper.klass!=hoven_blarg_trooper_class||
    !Memory::valid(trooper.pvars,trooper_attachment_offset+sizeof(unsigned)))return {};
 const auto gun=token(read<unsigned>(trooper.pvars+trooper_attachment_offset));
 // The native spawner copies the parent's spawn frame to this child.
 if(gun.klass!=blarg_gun_class||!identity_valid(gun)||
    read<std::uint64_t>(gun.address+moby_spawn_frame_offset)!=read<std::uint64_t>(trooper.address+moby_spawn_frame_offset))return {};
 return gun;
}
void NativeWorld::move_origin(const Lease& lease,Vec3 p,Vec3 angles){
 const auto t=lease.target;
 const Vec3 centre=p+rotate(rotation_from_euler(angles),lease.local_sphere);
 write(t.address+0x10,p);write(t.address,centre*1024.f);write(t.address+0x40,angles);
 basis_(t.address+0xc0,t.address+0x40);
 if(lease.attachment&&lease.attachment==trooper_gun(t)){
  using UpdateAttachment=void(*)(unsigned,unsigned,int);
  const auto update=reinterpret_cast<UpdateAttachment>(find_function("FUN_L12_0027b9c0"));
  if(update)update(t.address,lease.attachment.address,blarg_gun_joint);
 }
}
void NativeWorld::restore_lease(Lease& lease,bool cancelled){
 const auto t=lease.target;if(!t)return;
 if(identity_valid(t)){
  // A launched actor is already in the world. Only a cancelled carry needs
  // a placement before native collision is restored.
  const auto position=cancelled&&!lease.flying?drop_fallback(lease,true):read<Vec3>(t.address+0x10);
  move_origin(lease,position,lease.angles);
  if(!read<unsigned>(t.address+moby_collision_offset))write(t.address+moby_collision_offset,lease.collision);
  if(read<unsigned>(t.address+moby_collision_disable_offset)==1)write(t.address+moby_collision_disable_offset,lease.disabled);
  if(lease.native_update&&!read<unsigned>(t.address+moby_update_offset))
   write(t.address+moby_update_offset,lease.native_update);
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
bool NativeWorld::crate_position_available(const Lease& lease,Vec3 origin)const{
 const auto centre=origin+rotate(rotation_from_euler(lease.angles),lease.local_sphere);
 struct Occupancy {const NativeWorld& world;Target target;Vec3 centre;float radius;bool blocked;};
 Occupancy query{*this,lease.target,centre,collision_radius(lease.target),false};
 each_enemy(&query,+[](const void* context,Target other){
  auto& q=*const_cast<Occupancy*>(static_cast<const Occupancy*>(context));
  if(q.blocked||other==q.target||!destructible_crate_class(int(other.klass))||
     !q.world.read<unsigned>(other.address+0x94)||q.world.read<unsigned>(other.address+0x98))return;
  const float separation=q.radius+q.world.collision_radius(other)-.05f;
  q.blocked=length_squared(q.world.damage_centre(other)-q.centre)<separation*separation;
 },true);
 return !query.blocked;
}
Vec3 NativeWorld::drop_fallback(const Lease& lease,bool try_ground)const{
 if(!capturable_crate_class(lease.target.klass)||crate_position_available(lease,lease.origin))return lease.origin;
 // A stack may have closed the gap. Prefer supported ground in live play,
 // otherwise restore the crate in place so it can settle under native gravity.
 if(try_ground&&last_frame_.playing&&last_frame_.alive&&read<int>(mode())==0){
  if(const auto ground=drop_position(lease))return *ground;
 }
 return read<Vec3>(lease.target.address+0x10);
}
std::optional<Vec3> NativeWorld::drop_position(const Lease& lease)const{
 if(!valid(lease.target)||!finite(last_frame_.position)||!std::isfinite(last_frame_.yaw))return {};
 const float radius=collision_radius(lease.target);
 const bool crate=capturable_crate_class(lease.target.klass);
 const float distance=std::clamp(radius+.65f,.9f,1.7f);
 const Vec3 upright_sphere=rotate(rotation_from_euler(lease.angles),lease.local_sphere);
 const float lift=std::max(0.f,radius-upright_sphere.z)+.025f;
 // Stay in the forward half-plane. Shorter and slightly lateral candidates
 // handle a wall or ledge without sending the enemy through solid scenery.
 for(float factor:{1.f,.75f,.5f})for(float offset:{0.f,.35f,-.35f,.7f,-.7f}){
  const float yaw=last_frame_.yaw+offset;
  const Vec3 candidate=last_frame_.position+Vec3{std::cos(yaw),std::sin(yaw),0}*(distance*factor);
  const Vec3 top=candidate+Vec3{0,0,1.25f};
  const Vec3 start=last_frame_.position+Vec3{0,0,1.25f};
  if(ray(start,top,body()))continue;
  const auto floor=ray(top,candidate+Vec3{0,0,-3.f},body());
  if(!floor)continue;
  const auto normal=read<Vec3>(level(0x173e80));
  if(!finite(normal)||normal.z<.6f||floor->z>last_frame_.position.z+1.f)continue;
  const Vec3 result{floor->x,floor->y,floor->z+lift};
  if(finite(result)&&(!crate||crate_position_available(lease,result)))return result;
 }
 return {};
}
bool NativeWorld::counterstrike(Target enemy){
 constexpr std::size_t required=offsetof(openrac_api,moby_slot_of)+sizeof(api_->moby_slot_of);
 if(!mod_||api_->size<required||!api_->moby_slot_of||!api_->moby_hit||!valid(enemy)||
    !last_frame_.playing||last_frame_.paused||!last_frame_.alive||read<unsigned>(enemy.address+0x98)!=0)return false;
 const auto player=token(body());
 if(!identity_valid(player)||player.klass!=0||read<int>(hero()+0x22a8)<=0)return false;
 const Vec3 position=read<Vec3>(hero()+0x80),origin=read<Vec3>(enemy.address+0x10),delta=position-origin;
 const float squared=delta.x*delta.x+delta.y*delta.y;
 // A failed placement must not turn into a ranged or through-wall punishment.
 if(!finite(position)||!finite(origin)||!std::isfinite(squared)||squared>4.f||std::abs(delta.z)>1.f)return false;
 if(!clear_path(origin+Vec3{0,0,.75f},position+Vec3{0,0,.75f},player.address,enemy.address))return false;
 const int victim=api_->moby_slot_of(player.address),attacker=api_->moby_slot_of(enemy.address);
 if(victim<0||attacker<0)return false;
 const float scale=squared>.0001f?.2f/std::sqrt(squared):0.f;
 openrac_hit hit{};hit.size=sizeof hit;hit.victim=victim;hit.attacker=attacker;hit.mask=1;hit.damage=1.f;
 hit.position[0]=position.x;hit.position[1]=position.y;hit.position[2]=position.z+.75f;
 hit.velocity[0]=delta.x*scale;hit.velocity[1]=delta.y*scale;
 // This is the native light-enemy melee contact, not a weapon-impact packet.
 // The hero's own update owns Hurt, knockback, damage and invulnerability.
 return api_->moby_hit(mod_,&hit)==OPENRAC_HOOK_OK;
}
void NativeWorld::drop(Target t){
 // Only the current captive can be dropped. A stale token must never alter a
 // recycled actor or an independent enemy that is already flying.
 if(!lease_.target||!(lease_.target==t)||lease_.flying)return;
 const auto position=drop_position(lease_);
 if(!(lease_.target==t))return;
 if(!lease_valid()){restore_lease(lease_,false);return;}
 Lease carried=lease_;lease_={};
 const Vec3 ground=position?*position:drop_fallback(carried);
 const bool crate=capturable_crate_class(t.klass);
 if(!crate)carried.angles.z=std::atan2(last_frame_.position.y-ground.y,last_frame_.position.x-ground.x);
 move_origin(carried,ground,carried.angles);
 restore_lease(carried,false);
 if(audio_)audio_->emit(WeaponAudioCue::Drop);
 ground_contact_=position&&!crate?GroundContact{t,0}:GroundContact{};
 const bool retaliation=!crate&&drop_counterattack_&&counterstrike(t);
 if(trace_){char message[240];std::snprintf(message,sizeof message,
  "undercharged drop target=%08x class=%u grounded=%d point=(%.5f,%.5f,%.5f) counterstrike=%d; native AI restored",
  t.address,t.klass,int(position.has_value()),ground.x,ground.y,ground.z,int(retaliation));log(message);}
}
void NativeWorld::enemy_updated(unsigned moby){
 if(!ground_contact_.target||ground_contact_.target.address!=moby)return;
 const auto target=ground_contact_.target;
 if(!valid(target)||frozen(moby)||read<int>(mode())!=0){ground_contact_={};return;}
 if(++ground_contact_.updates>12){ground_contact_={};return;}
 // Called only after this enemy's real native update. Its live collision
 // sphere, rather than the desired drop point, decides whether it has landed.
 const Vec3 centre=read<Vec3>(moby)*(1.f/1024.f);
 if(!finite(centre))return;
 const float radius=collision_radius(target);
 const auto floor=ray(centre,centre+Vec3{0,0,-radius-.12f},moby);
 if(!floor||read<Vec3>(level(0x173e80)).z<.6f)return;
 const float clearance=centre.z-radius-floor->z;
 if(clearance<-.12f||clearance>.08f)return;
 ground_contact_={};if(audio_)audio_->emit(WeaponAudioCue::GroundContact);
}
void NativeWorld::discarded(Target t){
 if(lease_.target&&lease_.target==t){restore_lease(lease_,true);attack_cancelled();return;}
 if(auto* entry=flight(t)){restore_lease(entry->lease,true);*entry={};}
}
std::optional<Contact> NativeWorld::sweep(Target t,Vec3 from,Vec3 to){
 if(const auto* entry=flight(t);entry&&entry->lease.target&&capturable_crate_class(t.klass)){
  const float root=read<Vec3>(t.address+0x10).z;
  const float end=root+to.z-from.z;
  if(std::isfinite(root)&&std::isfinite(end)&&end<NativeCrates::void_height){
   const float fraction=root>NativeCrates::void_height?(root-NativeCrates::void_height)/(root-end):0.f;
   const Vec3 limit=from+(to-from)*fraction;
   if(fraction>0.f)if(const auto hit=sweep_impact(t,from,limit))return hit;
   return Contact{limit,{},{},ImpactReason::Void};
  }
 }
 return sweep_impact(t,from,to);
}
float NativeWorld::flight_duration(Target t,float normal_duration)const{
 // A no-contact crate keeps falling to the native void height. The longer
 // watchdog only covers malformed terrain or out-of-world spawn coordinates.
 return capturable_crate_class(t.klass)?16.f:normal_duration;
}
std::optional<Contact> NativeWorld::sweep_impact(Target t,Vec3 from,Vec3 to){
 const auto* active_flight=flight(t);
 const bool golden=active_flight&&active_flight->impact.golden_flight(t);
 const Vec3 direction=to-from;
 const float square=length_squared(direction);
 if(!finite(from)||!finite(to)||!std::isfinite(square))return std::nullopt;
 struct Search {
  NativeWorld& world;Target target;Vec3 from,direction;float square;bool golden;
  std::optional<Contact> hit;float first=1.001f;
 } search{*this,t,from,direction,square,golden,{}};
 bool clear=false;
 if(const auto point=ray(from,to,t.address,&clear)){
  search.hit=Contact{*point,{},read<Vec3>(level(0x173e80))};
  search.first=square>.000001f?std::clamp(dot(*point-from,direction)/square,0.f,1.f):0;
 }else if(!clear){
  return std::nullopt;
 }
 const auto visit=+[](const void* ctx,Target candidate){
  auto& s=*const_cast<Search*>(static_cast<const Search*>(ctx));auto& w=s.world;
  if(candidate==s.target||w.flying(candidate)||w.frozen(candidate.address))return;
  const Vec3 point=s.golden?w.damage_centre(candidate):w.read<Vec3>(candidate.address+0x10)+Vec3{0,0,.5f};
  const float radius=s.golden?w.collision_radius(candidate)+.4f:1.1f;
  const Vec3 offset=s.from-point;
  const float c=length_squared(offset)-radius*radius;
  if(!finite(point)||!std::isfinite(c))return;
  float fraction=0;
  if(c>0){
   if(s.square<=.000001f)return;
   const float b=dot(offset,s.direction),discriminant=b*b-s.square*c;
   if(!std::isfinite(discriminant)||discriminant<0)return;
   fraction=(-b-std::sqrt(discriminant))/s.square;
  }
  if(fraction<0||fraction>1||fraction>=s.first||!w.visible_from(s.from,candidate,s.target.address))return;
  s.first=fraction;s.hit=Contact{s.from+s.direction*fraction,candidate,{}};
 };
 if(golden)each_damageable(&search,visit);
 else each_enemy(&search,visit,true);
 return search.hit;
}
bool NativeWorld::hit(Target t,float damage,Vec3 direction,unsigned attacker,bool explosive){
 const bool allowed=explosive?golden_damage_target(t):capturable_enemy(t)||destructible_crate_class(int(t.klass));
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
 if(!throw_strike_active()){
  recoil_body_=shot.body;recoil_.start(last_frame_);enforce_release_recoil();
 }
}
void NativeWorld::drill_overheat(){
 if(active_&&last_frame_.playing&&!last_frame_.paused&&last_frame_.equipped&&last_frame_.alive)
  pending_burn_={token(body()),.75f};
}
void NativeWorld::post_drill_burn(){
 if(!pending_burn_.player)return;
 const auto player=pending_burn_.player;
 pending_burn_.remaining-=last_frame_.seconds;
 if(pending_burn_.remaining<=0.f||player.klass!=0||player.address!=body()||!identity_valid(player)||
    read<int>(hero()+0x22a8)<=0){
  pending_burn_={};return;
 }
 // The native strike ignores incoming hit packets. Post as soon as the hero
 // can receive a normal hit, after the captured enemy has detached.
 if(read<unsigned>(hero()+hero_state_offset)>3||read<unsigned>(hero()+hero_control_mode_offset)>1)return;
 pending_burn_={};
 constexpr std::size_t required=offsetof(openrac_api,moby_slot_of)+sizeof(api_->moby_slot_of);
 if(!mod_||api_->size<required||!api_->moby_slot_of||!api_->moby_hit)return;
 const auto position=read<Vec3>(hero()+0x80);
 if(!finite(position))return;
 const int victim=api_->moby_slot_of(player.address);
 if(victim<0)return;
 // Use the native environmental hit and let the hero manage invulnerability.
 openrac_hit hit{};hit.size=sizeof(hit);hit.victim=victim;hit.attacker=-1;hit.mask=1;hit.damage=1.f;
 hit.position[0]=position.x;hit.position[1]=position.y;hit.position[2]=position.z+.75f;
 if(api_->moby_hit(mod_,&hit)==OPENRAC_HOOK_OK&&trace_)log("drill overheat: native hit queued for Ratchet");
}
void NativeWorld::impact_result(Target target,const ImpactContext& result){
 auto* entry=flight(target);if(!entry)return;
 const Shot shot=entry->shot;
 const auto blast=entry->impact.consume(target,result.reason);
 restore_lease(entry->lease,false);*entry={}; // Clear before native FX/damage reentry.
 if(result.reason==ImpactReason::Void&&crates_.break_in_void(target)){
  if(trace_)log("launched crate broke at the native fall limit");
  return;
 }
 if(audio_)audio_->emit(blast?WeaponAudioCue::GoldenImpact:WeaponAudioCue::Impact,blast?blast->charge:result.charge);
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
 if(result.other&&identity_valid(*result.other)&&golden_damage_target(*result.other)&&
    !flying(*result.other)&&!frozen(result.other->address))
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
 if(emit_golden_burst(target,centre,blast))++golden_explosions_;
 if(generation!=cancellation_generation_)return;
 const unsigned attacker=shot_attacker(shot);
 for(std::size_t i=0;i<plan.size();++i){
  const auto& hit=plan.hits()[i];
  if(!identity_valid(hit.target)||!attacker)continue;
  submit_hit(hit.target,hit.damage,hit.direction,shot,true);
 }
 if(trace_){char text[240];std::snprintf(text,sizeof text,"Golden impact: charge=%.3f radius=%.2f damage=%.2f scale=%.2f targets=%zu source=%08x",blast.charge,blast.radius,blast.damage,blast.visual_scale,plan.size(),attacker);log(text);}
}
bool NativeWorld::emit_golden_burst(Target target,Vec3 centre,const GoldenBlast& blast){
 Stack position(api_,16);
 if(!position.at)return false;
 position.put(0,std::array<float,4>{centre.x,centre.y,centre.z,0});
 // The native colored burst owns particles and flash only. The existing
 // target plan remains the sole dispatcher of damage for this explosion.
 if(!green_burst_){
  burst_(target.address,position.at,-1,blast.visual_scale,blast.flash);
  return true;
 }
 green_burst_(target.address,position.at,blast.visual_scale,blast.flash);
 // The green variant omits the stock burst's camera shake. Apply that same
 // view check and scaled lifetime, without spawning the orange effect.
 if(sphere_visible_&&scale_ticks_){
  position.put(12,2.f);
  if(sphere_visible_(position.at,10.f)!=-1){
   const auto camera=level(0x166c80);
   if(Memory::valid(camera,0x16c)){
    write(camera+0x160,blast.visual_scale*.1f);
    write(camera+0x168,scale_ticks_(20));
   }
  }
 }
 if(trace_)log("Golden impact uses the native green burst");
 return true;
}
void NativeWorld::observe_frame(const Frame& frame){
 contact_tip_.reset();
 const bool same_epoch=last_frame_.epoch==frame.epoch;
 last_frame_=frame;recoil_.tick(frame);frame_hits_=0;
 if(!same_epoch){gag_.reset();pending_burn_={};}
 else if(gag_.active())gag_.tick(frame,token(body()),hero());
 if(throw_animation_body_&&!throw_strike_active())throw_animation_body_=throw_animation_weapon_={};
 if(same_epoch&&frame.playing&&!frame.paused&&frame.alive&&finite(frame.position)&&
    std::isfinite(frame.yaw)&&std::isfinite(frame.seconds)&&frame.seconds>0&&frame.seconds<=.1f){
  post_drill_burn();drain_hits();
 }
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
 if(gag_.active()){
  if(read<int>(mode())!=0||!Memory::valid(hero(),0x22ac)||read<int>(hero()+0x22a8)<=0||
     !active_||!gag_.owns_hero(token(body()))||read<int>(hero()+0x10b8)!=18||
     read<int>(hero()+0x10b4)!=2||read<unsigned char>(hero()+0x20a4)!=0)return;
 }else{
  if(!recoil_.active()||read<int>(mode())==3)return;
  if(!release_recoil_active()){recoil_.cancel();recoil_body_={};return;}
 }
 // Native HeroMotion / Hero from rnc/gameplay/hero.h. Preserve velocity.z and
 // vertical integration, platform carry, position, camera input and all AI.
 for(unsigned offset:{0x190u,0x194u,0x160u,0xe0u,0xe4u,0x1d20u,0x1d24u})write<float>(hero()+offset,0);
}
void NativeWorld::enforce_carry_speed(){
 // Only the current capture owns this cost. Launched enemies have separate
 // leases and must not keep Ratchet slowed while their flights finish.
 if(!active_||!lease_valid()||capturable_crate_class(lease_.target.klass)||throw_strike_active()||!Memory::valid(hero(),0x22ac)||read<int>(mode())!=0||
    read<int>(hero()+0x22a8)<=0||read<int>(hero()+0x10b8)!=18||
    read<int>(hero()+0x10b4)!=2||read<unsigned char>(hero()+0x20a4)!=0)return;
 const auto player=token(body());
 const int state=read<int>(hero()+0x2084);
 if(!identity_valid(player)||player.klass!=0||state==0x3d||state==0x82||state==0x77)return;
 const float ground_speed=read<float>(hero()+0x194);
 const auto velocity=read<Vec3>(hero()+0xe0);
 if(!std::isfinite(ground_speed)||ground_speed<0||!finite(velocity))return;
 const float speed=std::hypot(velocity.x,velocity.y);
 const float limit=ground_speed*carry_speed_multiplier;
 if(!std::isfinite(speed)||speed<=limit||speed==0)return;
 const float scale=limit/speed;
 write<float>(hero()+0xe0,velocity.x*scale);
 write<float>(hero()+0xe4,velocity.y*scale);
}
bool NativeWorld::throw_strike_active()const{
 const auto held=token(weapon());
 return active_&&throw_animation_body_&&identity_valid(throw_animation_body_)&&throw_animation_body_.address==body()&&
  held&&held.address==throw_animation_weapon_.address&&held.stamp==throw_animation_weapon_.stamp&&
  held.pvars==throw_animation_weapon_.pvars&&read<int>(hero()+hero_state_offset)==hero_held_attack_state&&
  read<unsigned char>(body()+moby_destination_sequence_offset)==walloper_hero_sequence;
}
bool NativeWorld::throw_contact_reached()const{
 // The stock forward drive ends at this tick. Keep the enemy on the tip
 // through the lunge, then detach while the stock contact window is open.
 const int release_tick=scale_ticks_?scale_ticks_(walloper_release_tick):walloper_release_tick;
 return read<int>(hero()+hero_state_timer_offset)>=release_tick;
}
bool NativeWorld::attack_input_ready()const{
 const auto control=read<unsigned>(hero()+hero_control_mode_offset);
 const auto state=read<int>(hero()+hero_state_offset);
 return active_&&!gag_.active()&&control<2&&state!=1&&state!=hero_held_attack_state&&
  read<unsigned char>(hero()+hero_item_locked_offset)==0&&Memory::valid(weapon(),256);
}
void NativeWorld::finish_throw_animation(){
 // Hits, jumps, scene changes and unequip own their new state. Never replace it
 // merely because our old animation token was still cached.
 if(throw_strike_active()&&last_frame_.playing&&last_frame_.alive&&last_frame_.equipped&&
    read<int>(mode())==0)state_(hero_idle_state,1);
 throw_animation_body_=throw_animation_weapon_={};
}
void NativeWorld::attack_cancelled(){finish_throw_animation();recoil_.cancel();recoil_body_={};arm_.reset();sparks_.reset();contact_tip_.reset();}
void NativeWorld::cancelled(){
 ++cancellation_generation_;
 pending_burn_={};
 if(gag_.active())gag_camera_.reset();
 gag_.reset();
 restore_lease(lease_,true);
 for(auto& entry:flights_){restore_lease(entry.lease,true);entry={};}
 deferred_={};deferred_head_=deferred_size_=frame_hits_=0;ground_contact_={};attack_cancelled();
}
void NativeWorld::set_damage_multiplier(float value){damage_multiplier_=std::isfinite(value)?std::clamp(value,1.f,16.f):1.f;}
bool NativeWorld::blocks_native_hit(Target victim,unsigned hit)const{
 // Built-in Walloper swing remains the attack animation; its legacy impact must
 // not kill a capturable enemy before capture. Heavy enemies, tanks and bosses
 // keep the original native hit, including armor and damage rules. Keep this
 // decision with capture eligibility so callers cannot suppress their damage.
 // Our posted impacts name the weapon moby.
 if(!Memory::valid(hit,0x40)||read<unsigned>(hit+0x20)!=body()||read<int>(hero()+0x10b8)!=18)return false;
 const auto flags=read<unsigned>(hit+0x24);
 const auto origin=read<unsigned short>(hit+0x2a);
 if(throw_strike_active()&&flags==0x30000&&read<unsigned char>(hit+0x29)==3&&
    (origin==180||origin==2017||origin==2018)&&read<float>(hit+0x2c)==3.f)return identity_valid(victim);
 return identity_valid(victim)&&capturable_enemy(victim)&&enemy_can_be_latched(victim)&&
        origin!=2017&&(flags&0x30000)!=0;
}
Frame NativeWorld::snapshot(bool primary,unsigned epoch)const{
 Frame f;f.epoch=epoch;f.seconds=read<float>(global(0x15ed7c));
 f.equipped=active_&&read<int>(hero()+0x10b8)==18&&read<int>(hero()+0x10b4)==2&&read<unsigned char>(hero()+0x20a4)==0;
 f.playing=read<int>(mode())==0;f.paused=read<int>(mode())==3;
 const int state=read<int>(hero()+0x2084);
 f.alive=read<int>(hero()+0x22a8)>0&&state!=0x3d&&state!=0x82&&state!=0x77;
 f.primary=primary;f.grounded=state==0||state==1||state==2||state==3||state==0x20||throw_strike_active();
 f.position=read<Vec3>(hero()+0x80);f.yaw=read<float>(hero()+0x98);return f;
}
void NativeWorld::animate(Motion motion){
 if(motion==Motion::DryFire){rotor_.dry_fire();return;}
 if(api_->loaded_level()<0||api_->loaded_level()>18)return;
 if(motion==Motion::Strike){throw_animation_body_=throw_animation_weapon_={};if(audio_)audio_->emit(WeaponAudioCue::Attack);arm_.reset();state_(hero_held_attack_state,1);if(trace_)log("native Walloper strike");}
 if(motion==Motion::ThrowStrike){
  const auto actor=token(body()),held=token(weapon());
  if(!lease_valid()||!identity_valid(actor)||actor.klass!=0||!last_frame_.alive||
     !last_frame_.equipped||!last_frame_.playing||!last_frame_.grounded||
     read<int>(mode())!=0||!attack_input_ready())return;
  // Use the same native movement state as capture. It owns the lunge, clip and
  // transform-history trail. Its stock damage packet is filtered separately.
  state_(hero_held_attack_state,1);
  throw_animation_body_=actor;throw_animation_weapon_=held;
  if(!throw_strike_active()){throw_animation_body_=throw_animation_weapon_={};return;}
  arm_.begin_native_strike();
  if(trace_)log("throw strike: native Walloper state=0x20 clip=0x67");
 }
 if(motion==Motion::Release){
  if(audio_)audio_->emit(WeaponAudioCue::Throw);
  arm_.release();
  enforce_release_recoil();
  if(trace_)log("throw detached on native Walloper contact; independent flight");
 }
 if(motion==Motion::Recover&&!recoil_.active()&&!arm_.releasing())arm_.reset();
}
void NativeWorld::pose(Phase,float){
 const auto seconds=read<float>(global(0x15ed7c));
 if(gag_.active())arm_.hold(body(),weapon(),last_frame_.yaw,seconds);
 else arm_.settle(seconds);
}
void NativeWorld::align_weapon(float angle){
 if(auto tip=arm_.align_weapon(weapon()))if(lease_valid()){move(lease_.target,*tip,angle);contact_tip_=tip;}
}
void NativeWorld::contact_sparks(Phase phase,float angle){
 const bool spinning=holding_enemy(phase)&&lease_valid()&&contact_tip_.has_value()&&!level_transitioning();
 const auto count=sparks_.update(last_frame_,spinning?lease_.target:Target{},contact_tip_.value_or(Vec3{}),
     read<Vec3>(weapon()+0xd0)*-1.f,angle,rotor_.controller().speed(),golden_);
 if(count&&trace_){
  const auto point=*contact_tip_;char text[280];
  std::snprintf(text,sizeof text,"contact sparks target=%08x class=%u point=(%.5f,%.5f,%.5f) speed=%.3f count=%u total=%llu",
   lease_.target.address,lease_.target.klass,point.x,point.y,point.z,rotor_.controller().speed(),count,
   static_cast<unsigned long long>(sparks_.emitted()));log(text);
 }
}
std::optional<Vec3> NativeWorld::carry_anchor(float angle){
 if(lease_valid())lease_.hold_time+=std::clamp(read<float>(global(0x15ed7c)),0.f,.1f);
 (void)angle;
 contact_tip_=arm_.hold(body(),weapon(),read<float>(hero()+0x98),read<float>(global(0x15ed7c)));
 return contact_tip_;
}
std::optional<Vec3> NativeWorld::throw_anchor(float angle,float progress){
 if(!lease_valid())return {};
 lease_.hold_time+=std::clamp(read<float>(global(0x15ed7c)),0.f,.1f);
 (void)angle;
 contact_tip_=arm_.throw_pose(weapon(),progress);
 return contact_tip_;
}
void NativeWorld::reset(){
 gag_camera_.reset();
 if(lease_.target)release(lease_.target,true);
 cancelled();arm_.reset(true);level_=api_->loaded_level();last_frame_={};
}
}

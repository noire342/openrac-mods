// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace salvage {
Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(Vec3 a){return std::hypot(a.x,a.y,a.z);}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
namespace {
bool text_valid(const std::string& s,std::size_t maximum,bool identifier=false){
 if(s.empty()||s.size()>maximum||s.find('\0')!=std::string::npos)return false;
 if(identifier)for(const unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return false;
 return true;
}
bool sum(Quantity a,Quantity b,Quantity& out){if(b>std::numeric_limits<Quantity>::max()-a)return false;out=a+b;return true;}
Vec3 unit(Vec3 v,Vec3 fallback={0,0,1}){const auto n=length(v);return std::isfinite(n)&&n>1.e-7f?v*(1.f/n):fallback;}
Vec3 axis(Vec3 r){
 const float sx=std::sin(r.x),cx=std::cos(r.x),sy=std::sin(r.y),cy=std::cos(r.y),sz=std::sin(r.z),cz=std::cos(r.z);
 return {cz*sy*cx+sz*sx,sz*sy*cx-cz*sx,cy*cx};
}
struct Segment {Vec3 a,b;};
Segment capsule(const Fragment& f,const PartDescriptor& d){const auto h=axis(f.rotation)*d.capsule_half_height;return {f.position-h,f.position+h};}
// Closest points of two finite segments, including either/both point segments.
std::pair<Vec3,Vec3> closest(Segment p,Segment q){
 const auto d1=p.b-p.a,d2=q.b-q.a,r=p.a-q.a;
 const float a=dot(d1,d1),e=dot(d2,d2),f=dot(d2,r);
 float s=0,t=0;
 if(a<=1.e-10f&&e<=1.e-10f)return {p.a,q.a};
 if(a<=1.e-10f)t=std::clamp(f/e,0.f,1.f);
 else{
  const float c=dot(d1,r);
  if(e<=1.e-10f)s=std::clamp(-c/a,0.f,1.f);
  else{
   const float b=dot(d1,d2),den=a*e-b*b;
   if(den>1.e-10f)s=std::clamp((b*f-c*e)/den,0.f,1.f);
   t=(b*s+f)/e;
   if(t<0){t=0;s=std::clamp(-c/a,0.f,1.f);}
   else if(t>1){t=1;s=std::clamp((b-c)/a,0.f,1.f);}
  }
 }
 return {p.a+d1*s,q.a+d2*t};
}
}
PartId stable_part_id(const std::string& text)noexcept{
 std::uint64_t hash=14695981039346656037ull;
 for(const unsigned char c:text){hash^=c;hash*=1099511628211ull;}
 return hash?hash:1;
}
Status Catalog::register_part(PartDescriptor p){
 if(!text_valid(p.stable_id,95,true)||!text_valid(p.name_it,127)||!text_valid(p.name_en,127)||
    !text_valid(p.source_it,127)||!text_valid(p.source_en,127)||!std::isfinite(p.radius)||p.radius<=0||p.radius>10||
    !std::isfinite(p.capsule_half_height)||p.capsule_half_height<0||p.capsule_half_height>10||
    !std::isfinite(p.mass)||p.mass<=0||p.mass>1.e6f||p.model_key.size()>95||p.model_key.find('\0')!=std::string::npos)return Status::Invalid;
 if(!p.id)p.id=stable_part_id(p.stable_id);
 if(entries_.find(p.id)!=entries_.end())return Status::Duplicate;
 for(const auto& entry:entries_)if(entry.second.stable_id==p.stable_id)return Status::Duplicate;
 entries_.emplace(p.id,std::move(p));return Status::Ok;
}
const PartDescriptor* Catalog::find(PartId id)const noexcept{const auto it=entries_.find(id);return it==entries_.end()?nullptr:&it->second;}
Quantity Inventory::count(PartId id)const noexcept{const auto it=items_.find(id);return it==items_.end()?0:it->second;}
Status Inventory::apply(const Catalog& catalog,const std::vector<Change>& changes){
 struct Delta {Quantity add{},remove{};};std::map<PartId,Delta> deltas;
 for(const auto& c:changes){
  if(!c.part||!c.quantity||(c.operation!=Operation::Add&&c.operation!=Operation::Remove))return Status::Invalid;
  if(!catalog.find(c.part))return Status::UnknownPart;
  auto& delta=deltas[c.part];auto& n=c.operation==Operation::Add?delta.add:delta.remove;
  Quantity next{};if(!sum(n,c.quantity,next))return Status::Overflow;n=next;
 }
 auto next=items_;
 for(const auto& entry:deltas){
  Quantity available{};if(!sum(count(entry.first),entry.second.add,available))return Status::Overflow;
  if(entry.second.remove>available)return Status::Insufficient;
  const auto n=available-entry.second.remove;
  if(n)next[entry.first]=n;else next.erase(entry.first);
 }
 if(next==items_)return Status::Ok;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return Status::Overflow;
 items_.swap(next);++revision_;return Status::Ok;
}
Status Inventory::restore(const InventorySnapshot& snapshot){
 for(const auto& entry:snapshot)if(!entry.first||!entry.second)return Status::Invalid;
 if(snapshot==items_)return Status::Ok;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return Status::Overflow;
 auto copy=snapshot;items_.swap(copy);++revision_;return Status::Ok;
}
bool World::begin_visit(int planet,bool explicit_load){
 if(planet<0||(!explicit_load&&planet==planet_)||visit_==std::numeric_limits<std::uint64_t>::max())return false;
 claims_.clear();fragments_.clear();planet_=planet;++visit_;return true;
}
void World::end_visit(){claims_.clear();fragments_.clear();planet_=-1;if(visit_!=std::numeric_limits<std::uint64_t>::max())++visit_;}
InstanceId World::spawn(PartId part,const SpawnState& state){
 if(planet_<0||!part||!next_id_||!finite(state.position)||!finite(state.velocity)||!finite(state.rotation)||!finite(state.angular_velocity))return 0;
 const auto id=next_id_;
 fragments_.emplace(id,Fragment{id,part,visit_,state.position,state.velocity,state.rotation,state.angular_velocity});
 ++next_id_;return id;
}
Fragment* World::find(InstanceId id)noexcept{const auto it=fragments_.find(id);return it==fragments_.end()?nullptr:&it->second;}
const Fragment* World::find(InstanceId id)const noexcept{const auto it=fragments_.find(id);return it==fragments_.end()?nullptr:&it->second;}
bool World::erase(InstanceId id)noexcept{return !claimed(id)&&fragments_.erase(id)!=0;}
Status World::claim(InstanceId id,std::uint64_t visit,WorldLease& out){
 out={};if(visit!=visit_||!find(id))return Status::Stale;
 if(claimed(id))return Status::Duplicate;if(!next_lease_)return Status::Overflow;
 const auto generation=next_lease_;claims_.emplace(id,generation);++next_lease_;out={id,visit_,generation};return Status::Ok;
}
bool World::owns(const WorldLease& lease)const noexcept{const auto found=claims_.find(lease.instance);return lease.generation&&lease.visit==visit_&&found!=claims_.end()&&found->second==lease.generation&&find(lease.instance);}
bool World::claimed(InstanceId id)const noexcept{return claims_.count(id)!=0;}
bool World::release(const WorldLease& lease)noexcept{if(!owns(lease))return false;claims_.erase(lease.instance);return true;}
std::optional<Fragment> World::take(const WorldLease& lease)noexcept{if(!owns(lease))return {};const auto fragment=*find(lease.instance);claims_.erase(lease.instance);fragments_.erase(lease.instance);return fragment;}
bool Events::publish(Event event)noexcept{
 if(sequence_==std::numeric_limits<std::uint64_t>::max())return false;
 event.sequence=sequence_+1;
 if(count_==capacity_){entries_[head_]=event;head_=(head_+1)%capacity_;}
 else{entries_[(head_+count_)%capacity_]=event;++count_;}
 ++sequence_;return true;
}
Status Events::read(std::uint64_t& cursor,std::size_t maximum,std::vector<Event>& out)const{
 out.clear();
 if(cursor>sequence_||(count_&&cursor<entries_[head_].sequence-1))return Status::Stale;
 for(std::size_t i=0;i<count_;++i){const auto& entry=entries_[(head_+i)%capacity_];
  if(entry.sequence>cursor){if(out.size()==maximum)break;out.push_back(entry);}}
 if(!out.empty())cursor=out.back().sequence;return Status::Ok;
}
std::optional<InstanceId> Collector::target(const CollectionInput& input,const Visibility& visible)const{
 if(!input.playing||!input.alive||input.native_interaction||!finite(input.player)||!visible)return {};
 std::optional<InstanceId> selected;float best=reach;
 for(const auto& entry:world_.all()){
  const auto& f=entry.second;const auto* part=catalog_.find(f.part);if(f.visit!=world_.visit()||world_.claimed(f.id)||!part||!finite(f.position))continue;
  const auto nearest=closest(capsule(f,*part),Segment{input.player,input.player}).first;
  const float distance=std::max(0.f,length(nearest-input.player)-part->radius);
  if(distance>best||(!selected&&distance>reach)||(selected&&distance==best)||!visible(f))continue;
  best=distance;selected=f.id;
 }
 return selected;
}
CollectionResult Collector::update(const CollectionInput& input,const Visibility& visible){
 const bool pressed=input.triangle&&!triangle_;triangle_=input.triangle;
 if(!pressed)return {};
 const auto selected=target(input,visible);if(!selected)return {};
 const auto* f=world_.find(*selected);if(!f)return {Status::Stale};
 const PartId part=f->part;const auto visit=f->visit;
 WorldLease lease;const auto claim=world_.claim(*selected,visit,lease);if(claim!=Status::Ok)return {claim,*selected,part,inventory_.count(part)};
 struct Release{World& world;WorldLease lease;~Release(){world.release(lease);}} release{world_,lease};
 const auto status=inventory_.apply(catalog_,{{part,1,Operation::Add}});
 if(status!=Status::Ok)return {status,*selected,part,inventory_.count(part)};
 world_.take(lease);
 events_.publish({0,EventKind::Collected,part,*selected,1,inventory_.count(part),inventory_.revision(),visit});
 return {Status::Ok,*selected,part,inventory_.count(part)};
}

PhysicsResult Physics::step(World& world,const Catalog& catalog,CollisionPort& collision,float seconds,const PlayerBody& player){
 PhysicsResult result;
 if(!std::isfinite(seconds)||seconds<=0||seconds>.25f||!finite(settings_.gravity)||
    !std::isfinite(settings_.fixed_step)||settings_.fixed_step<=0||!settings_.maximum_steps||
    !std::isfinite(settings_.activation_distance)||settings_.activation_distance<=0||
    !std::isfinite(settings_.restitution)||settings_.restitution<0||settings_.restitution>1||
    !std::isfinite(settings_.friction)||settings_.friction<0||settings_.friction>1||
    !std::isfinite(settings_.sleep_speed)||settings_.sleep_speed<0||
    !std::isfinite(settings_.sleep_seconds)||settings_.sleep_seconds<0||!finite(player.position))return result;
 const float duration=std::min(seconds,settings_.fixed_step*float(settings_.maximum_steps));
 const unsigned steps=std::min(settings_.maximum_steps,std::max(1u,unsigned(std::ceil(duration/settings_.fixed_step))));
 const float dt=duration/float(steps);
 const bool push_player=player.enabled&&finite(player.velocity)&&std::isfinite(player.radius)&&player.radius>0&&
  std::isfinite(player.half_height)&&player.half_height>=0;
 std::vector<InstanceId> active;
 float maximum_radius=.1f;
 for(auto& entry:world.all()){
  auto& f=*world.find(entry.first);const auto* d=catalog.find(f.part);
  if(world.claimed(f.id))continue;
  if(!d||!finite(f.position)||!finite(f.velocity)||!finite(f.rotation)||!finite(f.angular_velocity))continue;
  if(!collision.simulation_enabled(f.id)){f.suspended=true;++result.suspended;continue;}
  f.suspended=length(f.position-player.position)>settings_.activation_distance;
  if(f.suspended){++result.suspended;continue;}
  active.push_back(f.id);maximum_radius=std::max(maximum_radius,d->radius+d->capsule_half_height);
 }
 for(unsigned sub=0;sub<steps;++sub){
  std::map<InstanceId,Vec3> support_normals;
  for(const auto id:active){
   auto& f=*world.find(id);const auto& d=*catalog.find(f.part);
   if(push_player){
    const Segment body=capsule(f,d),actor{{player.position.x,player.position.y,player.position.z+player.radius},
     {player.position.x,player.position.y,player.position.z+player.radius+2*player.half_height}};
    const auto contact=closest(body,actor);const auto delta=contact.second-contact.first;
    const float distance=length(delta),radius=d.radius+player.radius;
    if(distance<radius+.01f){
     const auto normal=unit(delta,unit(player.velocity*-1,{1,0,0}));
     const float penetration=std::max(0.f,radius-distance),ratio=80.f/(80.f+d.mass);
     const float closing=-dot(player.velocity-f.velocity,normal);
     if(penetration>.002f||closing>settings_.sleep_speed){
      f.sleeping=false;f.quiet_seconds=0;
      f.position=f.position-normal*(penetration*ratio);
      result.player_correction=result.player_correction+normal*(penetration*(1-ratio));
      if(closing>0)f.velocity=f.velocity-normal*(closing*ratio);
     }
    }
   }
   if(f.sleeping){support_normals[id]={0,0,1};continue;}
   f.velocity=f.velocity+settings_.gravity*dt;
   const Vec3 next=f.position+f.velocity*dt;
   const Vec3 offset=axis(f.rotation)*d.capsule_half_height;
   std::optional<Contact> best;Vec3 best_offset{};
   for(const Vec3 sample:{offset*-1,Vec3{},offset}){
    const auto hit=collision.sweep_sphere(f.position+sample,next+sample,d.radius,f.id);
    if(hit&&finite(hit->point)&&finite(hit->normal)&&finite(hit->surface_velocity)&&
       std::isfinite(hit->fraction)&&hit->fraction>=0&&hit->fraction<=1&&length(hit->normal)>.5f&&
       (!best||hit->fraction<best->fraction)){best=hit;best_offset=sample;}
    if(d.capsule_half_height==0)break;
   }
   // Keep a resting body in contact across the tiny separation skin. Without
   // this support probe, alternate no-contact steps continually reset sleep.
   if(!best&&f.velocity.z<=0&&length(f.velocity)<.25f){
    for(const Vec3 sample:{offset*-1,Vec3{},offset}){
     const auto hit=collision.sweep_sphere(next+sample,next+sample-Vec3{0,0,.004f},d.radius,f.id);
     if(hit&&finite(hit->point)&&finite(hit->normal)&&finite(hit->surface_velocity)&&
        std::isfinite(hit->fraction)&&hit->fraction>=0&&hit->fraction<=1&&hit->normal.z>.5f&&
        (!best||hit->point.z-sample.z>best->point.z-best_offset.z)){best=hit;best_offset=sample;}
     if(d.capsule_half_height==0)break;
    }
   }
   bool supported=false;
   if(best){
    const auto normal=unit(best->normal);
    f.position=best->point-best_offset+normal*.001f;
    auto relative=f.velocity-best->surface_velocity;const float approaching=dot(relative,normal);
    if(approaching<0)relative=relative-normal*((1+settings_.restitution)*approaching);
    const auto normal_velocity=normal*dot(relative,normal);
    relative=normal_velocity+(relative-normal_velocity)*(1-settings_.friction);
    f.velocity=relative+best->surface_velocity;f.angular_velocity=f.angular_velocity*.8f;
    supported=normal.z>.25f;
    support_normals[id]=normal;
   }else f.position=next;
   f.rotation=f.rotation+f.angular_velocity*dt;
   for(float* angle:{&f.rotation.x,&f.rotation.y,&f.rotation.z})*angle=std::remainder(*angle,6.283185307f);
   if(supported&&length(f.velocity)<settings_.sleep_speed&&length(f.angular_velocity)<.2f)f.quiet_seconds+=dt;
   else f.quiet_seconds=0;
   if(f.quiet_seconds>=settings_.sleep_seconds&&supported){f.sleeping=true;f.velocity={};f.angular_velocity={};}
  }
  // Deterministic spatial bins restrict pair work to neighboring cells. A bin
  // is at least the largest diameter, so overlapping capsules cannot skip it.
  using Cell=std::tuple<int,int,int>;std::map<Cell,std::vector<InstanceId>> bins;
  const float width=std::max(.1f,maximum_radius*2);
  auto cell=[&](Vec3 p){auto coordinate=[&](float n){return int(std::clamp(std::floor(double(n)/width),-1.e8,1.e8));};return Cell{coordinate(p.x),coordinate(p.y),coordinate(p.z)};};
  for(const auto id:active)bins[cell(world.find(id)->position)].push_back(id);
  for(const auto id:active){
   auto& a=*world.find(id);const auto& da=*catalog.find(a.part);const auto c=cell(a.position);
   for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){
    const auto bin=bins.find({std::get<0>(c)+x,std::get<1>(c)+y,std::get<2>(c)+z});if(bin==bins.end())continue;
    for(const auto other:bin->second){
     if(other<=id)continue;
     auto& b=*world.find(other);if(a.sleeping&&b.sleeping)continue;
     const auto& db=*catalog.find(b.part);const auto points=closest(capsule(a,da),capsule(b,db));
     const auto delta=points.second-points.first;const float distance=length(delta),radius=da.radius+db.radius;
     if(distance>=radius)continue;
     const auto normal=unit(delta,{1,0,0});float inverse_a=1/da.mass,inverse_b=1/db.mass;
     const auto support_a=support_normals.find(id),support_b=support_normals.find(other);
     // Resolve a pile upward rather than correcting its bottom body through
     // the terrain. Horizontal contacts still move supported pieces normally.
     if(support_a!=support_normals.end()&&dot(normal*-1,support_a->second)<-.5f)inverse_a=0;
     if(support_b!=support_normals.end()&&dot(normal,support_b->second)<-.5f)inverse_b=0;
     const float total=inverse_a+inverse_b;if(total<=0)continue;
     const float penetration=radius-distance;
     a.position=a.position-normal*(penetration*inverse_a/total*.8f);
     b.position=b.position+normal*(penetration*inverse_b/total*.8f);
     const float closing=dot(b.velocity-a.velocity,normal);
     if(closing<0){const float impulse=-(1+settings_.restitution)*closing/total;
      a.velocity=a.velocity-normal*(impulse*inverse_a);b.velocity=b.velocity+normal*(impulse*inverse_b);}
     if(penetration>.03f||closing<-settings_.sleep_speed){a.sleeping=b.sleeping=false;a.quiet_seconds=b.quiet_seconds=0;}
    }
   }
  }
 }
 for(const auto id:active)if(world.find(id)->sleeping)++result.sleeping;else ++result.active;
 return result;
}
}

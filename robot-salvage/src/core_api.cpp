// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "core_api.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace salvage {
namespace {
ApiService* active{};
template<class F>int32_t invoke(F&& fn)noexcept{
 if(!active)return ROBOT_SALVAGE_NOT_READY;
 if(!active->thread_ok())return ROBOT_SALVAGE_WRONG_THREAD;
 try{return fn(*active);}catch(...){return ROBOT_SALVAGE_INTERNAL;}
}
template<class T>bool valid(const T* value){return value&&value->size>=sizeof(T);}
template<std::size_t N>bool string(const char(&source)[N],std::string& out){
 const auto end=static_cast<const char*>(std::memchr(source,0,N));if(!end)return false;
 out.assign(source,end);return true;
}
template<std::size_t N>void string(char(&out)[N],const std::string& source){
 const auto size=std::min(N-1,source.size());std::memcpy(out,source.data(),size);out[size]=0;
}
robot_salvage_vec3_v1 vector(Vec3 value){return {value.x,value.y,value.z};}
Vec3 vector(robot_salvage_vec3_v1 value){return {value.x,value.y,value.z};}
robot_salvage_part_v1 copy(const PartDescriptor& p){
 robot_salvage_part_v1 out{};out.size=sizeof(out);out.id=p.id;
 string(out.stable_id,p.stable_id);string(out.name_it,p.name_it);string(out.name_en,p.name_en);
 string(out.source_it,p.source_it);string(out.source_en,p.source_en);string(out.model_key,p.model_key);
 out.source_class=p.source_class;out.fragment_class=p.fragment_class;
 out.radius=p.radius;out.capsule_half_height=p.capsule_half_height;out.mass=p.mass;return out;
}
int32_t catalog_count(uint64_t* out){return invoke([&](ApiService& s)->int32_t{
 if(!out)return ROBOT_SALVAGE_INVALID;*out=s.catalog.entries().size();return ROBOT_SALVAGE_OK;});}
int32_t catalog_entry(uint64_t index,robot_salvage_part_v1* out){return invoke([&](ApiService& s)->int32_t{
 if(!valid(out))return ROBOT_SALVAGE_INVALID;if(index>=s.catalog.entries().size())return ROBOT_SALVAGE_NOT_FOUND;
 auto entry=s.catalog.entries().begin();std::advance(entry,std::ptrdiff_t(index));*out=copy(entry->second);return ROBOT_SALVAGE_OK;});}
int32_t get_part(uint64_t id,robot_salvage_part_v1* out){return invoke([&](ApiService& s)->int32_t{
 if(!valid(out))return ROBOT_SALVAGE_INVALID;const auto* p=s.catalog.find(id);
 if(!p)return ROBOT_SALVAGE_NOT_FOUND;*out=copy(*p);return ROBOT_SALVAGE_OK;});}
int32_t inventory_count(uint64_t* out,uint64_t* revision){return invoke([&](ApiService& s)->int32_t{
 if(!out||!revision)return ROBOT_SALVAGE_INVALID;*out=s.inventory.snapshot().size();*revision=s.inventory.revision();return ROBOT_SALVAGE_OK;});}
int32_t inventory_entry(uint64_t index,robot_salvage_inventory_entry_v1* out){return invoke([&](ApiService& s)->int32_t{
 if(!valid(out))return ROBOT_SALVAGE_INVALID;if(index>=s.inventory.snapshot().size())return ROBOT_SALVAGE_NOT_FOUND;
 auto entry=s.inventory.snapshot().begin();std::advance(entry,std::ptrdiff_t(index));
 robot_salvage_inventory_entry_v1 value{};value.size=sizeof(value);value.known=s.catalog.find(entry->first)?1u:0u;
 value.part=entry->first;value.quantity=entry->second;value.revision=s.inventory.revision();*out=value;return ROBOT_SALVAGE_OK;});}
int32_t apply_changes(const robot_salvage_change_v1* changes,uint32_t count){return invoke([&](ApiService& s)->int32_t{
 if((count&&!changes)||count>65536)return ROBOT_SALVAGE_INVALID;
 std::vector<Change> batch;batch.reserve(count);
 for(uint32_t i=0;i<count;++i){const auto& c=changes[i];if(!valid(&c)||c.operation>ROBOT_SALVAGE_REMOVE)return ROBOT_SALVAGE_INVALID;
  batch.push_back({c.part,c.quantity,c.operation==ROBOT_SALVAGE_ADD?Operation::Add:Operation::Remove});}
 const auto before=s.inventory.snapshot();std::map<PartId,bool> changed;for(const auto& c:batch)changed[c.part]=true;
 const auto status=s.inventory.apply(s.catalog,batch);if(status!=Status::Ok)return int32_t(status);
 if(before==s.inventory.snapshot())return ROBOT_SALVAGE_OK;
 for(const auto& c:changed){const auto old=before.find(c.first);const Quantity n=old==before.end()?0:old->second,total=s.inventory.count(c.first);
  if(n!=total)s.events.publish({0,EventKind::InventoryChanged,c.first,0,n>total?n-total:total-n,total,s.inventory.revision(),s.world.visit()});}
 return ROBOT_SALVAGE_OK;});}
int32_t register_part(const robot_salvage_part_v1* source,uint64_t* registered){return invoke([&](ApiService& s)->int32_t{
 if(!valid(source)||!registered)return ROBOT_SALVAGE_INVALID;
 PartDescriptor p;p.id=source->id;
 if(!string(source->stable_id,p.stable_id)||!string(source->name_it,p.name_it)||!string(source->name_en,p.name_en)||
    !string(source->source_it,p.source_it)||!string(source->source_en,p.source_en)||!string(source->model_key,p.model_key))return ROBOT_SALVAGE_INVALID;
 p.source_class=source->source_class;p.fragment_class=source->fragment_class;
 p.radius=source->radius;p.capsule_half_height=source->capsule_half_height;p.mass=source->mass;
 const PartId id=p.id?p.id:stable_part_id(p.stable_id);const auto status=s.catalog.register_part(std::move(p));
 if(status!=Status::Ok)return int32_t(status);*registered=id;
 s.events.publish({0,EventKind::CatalogChanged,id,0,0,0,s.inventory.revision(),s.world.visit()});return ROBOT_SALVAGE_OK;});}
int32_t spawn_part(const robot_salvage_spawn_v1* source,uint64_t* instance){return invoke([&](ApiService& s)->int32_t{
 if(!valid(source)||!instance)return ROBOT_SALVAGE_INVALID;
 if(!s.catalog.find(source->part))return ROBOT_SALVAGE_UNKNOWN_PART;
 if(s.world.planet()<0)return ROBOT_SALVAGE_NOT_READY;
 const SpawnState state{vector(source->position),vector(source->velocity),vector(source->rotation),vector(source->angular_velocity)};
 if(!finite(state.position)||!finite(state.velocity)||!finite(state.rotation)||!finite(state.angular_velocity))return ROBOT_SALVAGE_INVALID;
 const auto id=s.world.spawn(source->part,state);if(!id)return ROBOT_SALVAGE_OVERFLOW;*instance=id;
 s.events.publish({0,EventKind::Spawned,source->part,id,0,s.inventory.count(source->part),s.inventory.revision(),s.world.visit()});return ROBOT_SALVAGE_OK;});}
int32_t world_count(uint64_t* out,uint64_t* visit){return invoke([&](ApiService& s)->int32_t{
 if(!out||!visit)return ROBOT_SALVAGE_INVALID;*out=s.world.all().size();*visit=s.world.visit();return ROBOT_SALVAGE_OK;});}
int32_t world_entry(uint64_t index,robot_salvage_fragment_v1* out){return invoke([&](ApiService& s)->int32_t{
 if(!valid(out))return ROBOT_SALVAGE_INVALID;if(index>=s.world.all().size())return ROBOT_SALVAGE_NOT_FOUND;
 auto entry=s.world.all().begin();std::advance(entry,std::ptrdiff_t(index));const auto& f=entry->second;
 robot_salvage_fragment_v1 value{};value.size=sizeof(value);value.sleeping=f.sleeping?1u:0u;value.suspended=f.suspended?1u:0u;
 value.instance=f.id;value.part=f.part;value.visit=f.visit;value.position=vector(f.position);value.velocity=vector(f.velocity);
 value.rotation=vector(f.rotation);value.angular_velocity=vector(f.angular_velocity);*out=value;return ROBOT_SALVAGE_OK;});}
int32_t read_events(uint64_t* cursor,robot_salvage_event_v1* out,uint32_t capacity,uint32_t* count,uint64_t* newest){return invoke([&](ApiService& s)->int32_t{
 if(!cursor||!count||!newest||(capacity&&!out)||capacity>65536)return ROBOT_SALVAGE_INVALID;
 for(uint32_t i=0;i<capacity;++i)if(!valid(out+i))return ROBOT_SALVAGE_INVALID;
 *count=0;*newest=s.events.newest_sequence();auto next=*cursor;std::vector<Event> entries;
 const auto status=s.events.read(next,capacity,entries);if(status!=Status::Ok)return int32_t(status);
 for(std::size_t i=0;i<entries.size();++i){const auto& e=entries[i];robot_salvage_event_v1 value{};value.size=sizeof(value);value.kind=uint32_t(e.kind);
  value.sequence=e.sequence;value.part=e.part;value.instance=e.instance;value.quantity=e.quantity;value.total=e.total;value.revision=e.revision;value.visit=e.visit;out[i]=value;}
 *cursor=next;*count=uint32_t(entries.size());return ROBOT_SALVAGE_OK;});}
const robot_salvage_api_v1 table{sizeof(table),ROBOT_SALVAGE_API_VERSION,catalog_count,catalog_entry,get_part,inventory_count,inventory_entry,
 apply_changes,register_part,spawn_part,world_count,world_entry,read_events};
}
ApiService::ApiService(Catalog& c,Inventory& i,World& w,Events& e):catalog(c),inventory(i),world(w),events(e),thread_(std::this_thread::get_id()){
 if(active)throw std::logic_error("robot-salvage already has an active API context");active=this;
}
ApiService::~ApiService(){if(active==this)active=nullptr;}
const robot_salvage_api_v1* ApiService::query(std::uint32_t version)noexcept{
 return active&&active->thread_ok()&&version==ROBOT_SALVAGE_API_VERSION?&table:nullptr;
}
}

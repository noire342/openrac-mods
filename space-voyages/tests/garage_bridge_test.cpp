// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed-width ABI observer tests; no provider DLL, native world or routing.
#include "garage_bridge.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
using namespace spacevoyages;
namespace {
unsigned checks{},calls{};std::size_t allocations{};
veldin_garage_snapshot_v1 published{};
bool query_failure{};
void require(bool condition,const char* message){++checks;if(!condition){std::fprintf(stderr,"garage bridge: %s\n",message);std::exit(1);}}
veldin_garage_snapshot_v1 empty(){
    veldin_garage_snapshot_v1 s{};s.size=sizeof s;s.version=VELDIN_GARAGE_API_V1;
    s.selected_ship=VELDIN_GARAGE_SHIP_NONE;
    std::memcpy(s.world_id,garage_world_id,std::strlen(garage_world_id)+1);std::memcpy(s.region,"pal",4);return s;
}
veldin_garage_snapshot_v1 ready(){
    auto s=empty();s.flags=VELDIN_GARAGE_READY|VELDIN_GARAGE_ACTIVE;s.owned_mask=15;
    s.selected_ship=VELDIN_GARAGE_SHIP_ORIGINAL;s.revision=1;s.generation=7;s.actor_count=4;
    s.actors[0]={0,20,2047,7,1};s.actors[1]={1,21,531,7,1};
    s.actors[2]={2,22,532,7,1};s.actors[3]={3,23,533,7,1};return s;
}
int query(veldin_garage_snapshot_v1* out,std::uint32_t bytes){
    ++calls;require(out&&bytes==176,"observer supplies exactly its own v1 buffer");
    if(query_failure)return 0;*out=published;return 1;
}
int throws(veldin_garage_snapshot_v1*,std::uint32_t){throw 7;}
void rejected(veldin_garage_snapshot_v1 invalid,GarageReadStatus expected=GarageReadStatus::InvalidSnapshot){
    GarageObservation output;output.ready=true;output.selected_ship=GarageShipIdentity::Purple;
    require(decode_garage_snapshot(invalid,GarageRegion::Pal,7,output)==expected,"malformed or mismatched snapshot is rejected");
    require(!output.ready&&!output.active&&!output.travelling&&output.owned_mask==0&&
        output.selected_ship==GarageShipIdentity::None&&output.actor_count==0,"failed decode never exposes a cached ship, ownership or actor");
}
}
void* operator new(std::size_t count){++allocations;if(auto* p=std::malloc(count?count:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
int main(){
    require(sizeof(veldin_garage_snapshot_v1)==176&&sizeof(veldin_garage_actor_v1)==20,"copied v1 ABI has exact reviewed sizes");
    GarageObservation observed;const auto source=ready();
    require(decode_garage_snapshot(source,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready,"valid PAL home fleet accepted");
    require(observed.ready&&observed.active&&!observed.travelling&&observed.selected_ship==GarageShipIdentity::Original&&observed.actor_count==4,"positive world identity and stable Original selection preserved");
    for(unsigned i=0;i<4;++i)require(unsigned(observed.actors[i].ship)==i&&observed.actors[i].generation==7&&observed.actors[i].visible,"each stable ship actor retains its own identity and generation");
    for(unsigned id=0;id<4;++id){auto s=source;s.selected_ship=id;require(decode_garage_snapshot(s,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready&&unsigned(observed.selected_ship)==id,"all4 authoritative selections stay distinct");}
    auto hidden=source;hidden.actors[0].native_class=531;hidden.actors[0].flags=0;
    require(decode_garage_snapshot(hidden,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready&&observed.selected_ship==GarageShipIdentity::Original&&!observed.actors[0].visible&&observed.actors[1].ship==GarageShipIdentity::Green,"Original native531 controller is never inferred to be Green");
    auto travelling=source;travelling.flags=VELDIN_GARAGE_READY|VELDIN_GARAGE_TRAVELLING;travelling.actor_count=0;
    require(decode_garage_snapshot(travelling,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready&&observed.ready&&observed.travelling&&!observed.active&&observed.actor_count==0,"travelling selection is readable without claiming a currently active home world");
    travelling.flags=VELDIN_GARAGE_READY;
    require(decode_garage_snapshot(travelling,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready&&!observed.active,"constant world_id alone never implies ACTIVE");
    travelling=source;travelling.flags|=VELDIN_GARAGE_TRAVELLING;
    require(decode_garage_snapshot(travelling,GarageRegion::Pal,7,observed)==GarageReadStatus::Ready&&observed.active&&observed.travelling,"known flags may overlap during a committed native boarding transition");
    auto none=empty();
    require(decode_garage_snapshot(none,GarageRegion::Pal,99,observed)==GarageReadStatus::NotReady&&observed.selected_ship==GarageShipIdentity::None&&!observed.ready,"cleared service is not permission to fall back to a physical ship");
    for(unsigned size:{0u,175u,177u}){auto s=source;s.size=size;rejected(s);}
    for(unsigned version:{0u,2u,std::numeric_limits<unsigned>::max()}){auto s=source;s.version=version;rejected(s);}
    auto s=source;s.reserved=1;rejected(s);s=source;s.flags|=8;rejected(s);
    s=source;s.actor_count=5;rejected(s);s=source;s.owned_mask|=16;rejected(s);
    s=source;s.owned_mask=0;rejected(s);s=source;s.selected_ship=4;rejected(s);
    s=source;s.selected_ship=VELDIN_GARAGE_SHIP_NONE;rejected(s);
    s=source;s.owned_mask=14;rejected(s);s=source;s.flags&=~VELDIN_GARAGE_ACTIVE;rejected(s);
    s=source;s.revision=0;rejected(s);
    s=source;s.generation=6;rejected(s,GarageReadStatus::StaleGeneration);
    s=source;s.actors[0].generation=6;rejected(s);
    for(int slot:{-1,4096}){s=source;s.actors[0].native_slot=slot;rejected(s);}
    for(unsigned cls:{0u,65536u}){s=source;s.actors[0].native_class=cls;rejected(s);}
    s=source;s.actors[0].ship_id=4;rejected(s);s=source;s.actors[0].flags=2;rejected(s);
    s=source;s.actors[1].ship_id=0;rejected(s);s=source;s.actors[1].native_slot=20;rejected(s);
    s=source;s.owned_mask=3;s.actor_count=3;rejected(s);
    s=source;std::memcpy(s.world_id,"veldin",7);rejected(s,GarageReadStatus::WrongWorld);
    s=source;std::memset(s.world_id,'x',sizeof s.world_id);rejected(s,GarageReadStatus::WrongWorld);
    s=source;std::memcpy(s.region,"ntsc",5);rejected(s,GarageReadStatus::WrongRegion);
    require(decode_garage_snapshot(s,GarageRegion::Ntsc,7,observed)==GarageReadStatus::Ready,"explicit matching region is validated without guessing address offsets");
    s=source;std::memset(s.region,'x',sizeof s.region);rejected(s,GarageReadStatus::WrongRegion);
    s=none;s.flags=VELDIN_GARAGE_ACTIVE;rejected(s);s=none;s.owned_mask=1;rejected(s);
    s=none;s.actor_count=1;rejected(s);s=none;s.generation=7;rejected(s);s=none;s.selected_ship=0;rejected(s);
    require(decode_garage_snapshot(source,static_cast<GarageRegion>(99),7,observed)==GarageReadStatus::WrongRegion,"invalid consumer region cannot silently use PAL");
    GarageBridge bridge;
    require(bridge.poll(7)==GarageReadStatus::Unavailable&&!calls,"unbound observer cannot resolve or call anything");
    published=source;bridge.bind(query);
    require(bridge.poll(7)==GarageReadStatus::Ready&&calls==1&&bridge.observation().active,"one poll makes exactly one read-only ABI query");
    auto before=allocations;const auto provider_before=published;
    for(unsigned i=0;i<10000;++i)require(bridge.poll(7)==GarageReadStatus::Ready,"repeat publication remains valid");
    require(allocations==before,"repeated queries and validation allocate no heap storage");
    require(std::memcmp(&provider_before,&published,sizeof published)==0,"observer never changes provider ownership, selection or actor observations");
    published.revision=0;require(bridge.poll(7)==GarageReadStatus::InvalidSnapshot&&!bridge.observation().ready,"malformed query clears previous selection immediately");
    published=source;published.revision=2;require(bridge.poll(7)==GarageReadStatus::Ready,"newer revision accepted");
    published.revision=1;require(bridge.poll(7)==GarageReadStatus::StaleRevision&&bridge.observation().selected_ship==GarageShipIdentity::None,"older publication never restores stale actors or selection");
    published.revision=2;published.selected_ship=1;require(bridge.poll(7)==GarageReadStatus::InconsistentRevision,"same revision cannot publish a different committed selection");
    published.revision=3;require(bridge.poll(7)==GarageReadStatus::Ready&&bridge.observation().selected_ship==GarageShipIdentity::Green,"fresh revision publishes the committed new identity");
    require(bridge.poll(8)==GarageReadStatus::StaleGeneration&&!bridge.observation().ready,"world reload makes every old actor observation unavailable");
    published.generation=8;for(auto& actor:published.actors)actor.generation=8;++published.revision;
    require(bridge.poll(8)==GarageReadStatus::Ready,"matching new world generation restores fresh observations");
    published=none;published.revision=5;
    require(bridge.poll(8)==GarageReadStatus::NotReady&&!bridge.observation().ready,"provider clear invalidates committed state without fallback");
    published=source;published.revision=4;require(bridge.poll(7)==GarageReadStatus::StaleRevision,"a pre-clear snapshot cannot reappear as a usable selection");
    query_failure=true;require(bridge.poll(7)==GarageReadStatus::QueryFailed&&bridge.observation().actor_count==0,"ABI failure clears output");query_failure=false;
    bridge.bind(throws);require(bridge.poll(7)==GarageReadStatus::QueryFailed,"misbehaving external C++ provider cannot leak cached output");
    bridge.unbind();before=calls;require(bridge.poll(7)==GarageReadStatus::Unavailable&&calls==before&&bridge.observation().selected_ship==GarageShipIdentity::None,"unbind before module unload drops function pointer and every observation");
    published=source;bridge.bind(query);require(bridge.poll(7)==GarageReadStatus::Ready,"explicit new provider lifetime resets revision history");
    std::printf("garage_bridge: %u ABI, identity, region, mask, generation, lifecycle and no-allocation checks passed (read-only fake provider; no native compatibility claim)\n",checks);
}

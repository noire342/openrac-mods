// SPDX-License-Identifier: GPL-3.0-or-later
#include "garage_bridge.hpp"
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace spacevoyages {
static_assert(sizeof(veldin_garage_actor_v1)==20,"garage actor v1 ABI");
static_assert(sizeof(veldin_garage_snapshot_v1)==176,"garage snapshot v1 ABI");
static_assert(offsetof(veldin_garage_snapshot_v1,revision)==24&&
    offsetof(veldin_garage_snapshot_v1,world_id)==40&&
    offsetof(veldin_garage_snapshot_v1,region)==88&&
    offsetof(veldin_garage_snapshot_v1,actors)==96,"garage v1 fixed field offsets");
static_assert(std::is_standard_layout<veldin_garage_snapshot_v1>::value,"garage C layout");
namespace {
template<std::size_t N>bool exact_string(const char(&text)[N],const char* expected) noexcept {
    if(!expected)return false;
    const auto size=std::strlen(expected);
    return size<N&&std::memchr(text,0,N)&&std::memcmp(text,expected,size+1)==0;
}
const char* region_name(GarageRegion region) noexcept {
    switch(region){case GarageRegion::Pal:return "pal";case GarageRegion::Ntsc:return "ntsc";}
    return nullptr;
}
bool same_observation(const GarageObservation& a,const GarageObservation& b) noexcept {
    if(a.ready!=b.ready||a.active!=b.active||a.travelling!=b.travelling||
        a.owned_mask!=b.owned_mask||a.generation!=b.generation||a.actor_count!=b.actor_count||
        a.selected_ship!=b.selected_ship||a.revision!=b.revision)return false;
    for(std::size_t i=0;i<a.actor_count;++i){
        const auto& x=a.actors[i];const auto& y=b.actors[i];
        if(x.ship!=y.ship||x.native_slot!=y.native_slot||x.native_class!=y.native_class||
            x.generation!=y.generation||x.visible!=y.visible)return false;
    }
    return true;
}
}
GarageReadStatus decode_garage_snapshot(const veldin_garage_snapshot_v1& input,
    GarageRegion region,std::uint32_t expected_generation,GarageObservation& output) noexcept {
    output={};
    constexpr std::uint32_t known_flags=VELDIN_GARAGE_READY|VELDIN_GARAGE_ACTIVE|VELDIN_GARAGE_TRAVELLING;
    if(input.size!=sizeof input||input.version!=VELDIN_GARAGE_API_V1||input.reserved||
        (input.flags&~known_flags)||input.actor_count>4||(input.owned_mask&~15u))return GarageReadStatus::InvalidSnapshot;
    if(!exact_string(input.world_id,garage_world_id))return GarageReadStatus::WrongWorld;
    if(!exact_string(input.region,region_name(region)))return GarageReadStatus::WrongRegion;
    if(!(input.flags&VELDIN_GARAGE_READY)){
        if(input.flags||input.owned_mask||input.actor_count||input.generation||
            input.selected_ship!=VELDIN_GARAGE_SHIP_NONE)return GarageReadStatus::InvalidSnapshot;
        output.revision=input.revision;return GarageReadStatus::NotReady;
    }
    if(!input.revision||!input.owned_mask||input.selected_ship>=4||
        !(input.owned_mask&(1u<<input.selected_ship))||
        (!(input.flags&VELDIN_GARAGE_ACTIVE)&&input.actor_count))return GarageReadStatus::InvalidSnapshot;
    if(input.generation!=expected_generation)return GarageReadStatus::StaleGeneration;
    GarageObservation next{};next.ready=true;next.active=(input.flags&VELDIN_GARAGE_ACTIVE)!=0;
    next.travelling=(input.flags&VELDIN_GARAGE_TRAVELLING)!=0;next.owned_mask=input.owned_mask;
    next.generation=input.generation;next.selected_ship=static_cast<GarageShipIdentity>(input.selected_ship);
    next.revision=input.revision;next.actor_count=input.actor_count;
    std::uint32_t seen=0;
    for(std::size_t i=0;i<input.actor_count;++i){
        const auto& actor=input.actors[i];
        if(actor.ship_id>=4||!(input.owned_mask&(1u<<actor.ship_id))||(seen&(1u<<actor.ship_id))||
            actor.native_slot<0||actor.native_slot>=4096||!actor.native_class||actor.native_class>65535||
            actor.generation!=input.generation||(actor.flags&~VELDIN_GARAGE_ACTOR_VISIBLE))return GarageReadStatus::InvalidSnapshot;
        for(std::size_t j=0;j<i;++j)if(input.actors[j].native_slot==actor.native_slot)return GarageReadStatus::InvalidSnapshot;
        seen|=1u<<actor.ship_id;
        next.actors[i]={static_cast<GarageShipIdentity>(actor.ship_id),actor.native_slot,actor.native_class,
            actor.generation,(actor.flags&VELDIN_GARAGE_ACTOR_VISIBLE)!=0};
    }
    output=next;return GarageReadStatus::Ready;
}
void GarageBridge::bind(openrac_veldin_garage_query_v1_fn query) noexcept {
    unbind();query_=query;
}
void GarageBridge::unbind() noexcept {
    query_=nullptr;observation_={};last_seen_={};have_revision_=false;status_=GarageReadStatus::Unavailable;
}
GarageReadStatus GarageBridge::poll(std::uint32_t expected_generation) noexcept {
    observation_={};
    if(!query_)return status_=GarageReadStatus::Unavailable;
    veldin_garage_snapshot_v1 raw{};
    try{if(query_(&raw,sizeof raw)!=1)return status_=GarageReadStatus::QueryFailed;}
    catch(...){return status_=GarageReadStatus::QueryFailed;}
    GarageObservation next{};
    const auto decoded=decode_garage_snapshot(raw,region_,expected_generation,next);
    if(decoded!=GarageReadStatus::Ready&&decoded!=GarageReadStatus::NotReady)return status_=decoded;
    if(have_revision_){
        if(next.revision<last_seen_.revision)return status_=GarageReadStatus::StaleRevision;
        if(next.revision==last_seen_.revision&&!same_observation(next,last_seen_))return status_=GarageReadStatus::InconsistentRevision;
    }
    last_seen_=next;have_revision_=true;observation_=next;return status_=decoded;
}
}

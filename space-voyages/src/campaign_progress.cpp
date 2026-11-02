// SPDX-License-Identifier: GPL-3.0-or-later
#include "campaign_progress.hpp"
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace spacevoyages {
static_assert(sizeof(veldin_campaign_snapshot_v1)==80,"canonical campaign v1 ABI");
static_assert(offsetof(veldin_campaign_snapshot_v1,capture_revision)==16&&
    offsetof(veldin_campaign_snapshot_v1,source_generation)==24&&
    offsetof(veldin_campaign_snapshot_v1,source_native_level)==28&&
    offsetof(veldin_campaign_snapshot_v1,coordinates)==32&&
    offsetof(veldin_campaign_snapshot_v1,visits)==51&&
    offsetof(veldin_campaign_snapshot_v1,reserved)==70&&
    offsetof(veldin_campaign_snapshot_v1,region)==72,"campaign v1 fixed offsets");
static_assert(std::is_standard_layout<veldin_campaign_snapshot_v1>::value&&
    std::is_trivially_copyable<veldin_campaign_snapshot_v1>::value,"campaign C POD");
namespace {
bool same_capture(const CampaignObservation& a,const CampaignObservation& b) noexcept {
    return a.source_generation==b.source_generation&&a.source_native_level==b.source_native_level&&
        a.coordinates==b.coordinates&&a.visits==b.visits;
}
template<class Array>bool all_zero(const Array& bytes) noexcept {
    for(const auto byte:bytes)if(byte)return false;
    return true;
}
}
CampaignReadStatus decode_campaign_snapshot(const veldin_campaign_snapshot_v1& input,
    CampaignRegion region,std::uint32_t expected_generation,CampaignObservation& output) noexcept {
    output={};
    constexpr std::uint32_t flags=VELDIN_CAMPAIGN_READY|VELDIN_CAMPAIGN_ACTIVE|VELDIN_CAMPAIGN_TRAVELLING;
    if(input.size!=sizeof input||input.version!=VELDIN_CAMPAIGN_API_V1||
        (input.flags&~flags)||!all_zero(input.reserved))return CampaignReadStatus::InvalidSnapshot;
    if(!(input.flags&VELDIN_CAMPAIGN_READY)){
        if(input.flags||input.generation||input.capture_revision||input.source_generation||
            input.source_native_level!=-1||!all_zero(input.coordinates)||!all_zero(input.visits)||
            !all_zero(input.region))return CampaignReadStatus::InvalidSnapshot;
        return CampaignReadStatus::NotReady;
    }
    // PAL is the only reviewed provider provenance. Require a bounded exact
    // string; no pointer to the provider's character storage is retained.
    if(region!=CampaignRegion::Pal||std::memcmp(input.region,"pal",4)!=0)
        return CampaignReadStatus::WrongRegion;
    if(!input.generation||!input.capture_revision||!input.source_generation||
        input.source_generation>input.generation||input.source_native_level<0||
        input.source_native_level>=int(VELDIN_CAMPAIGN_PLANETS_V1))return CampaignReadStatus::InvalidSnapshot;
    if(!expected_generation||input.generation!=expected_generation)return CampaignReadStatus::StaleGeneration;
    for(const auto coordinate:input.coordinates)if(coordinate>1)return CampaignReadStatus::InvalidSnapshot;
    CampaignObservation next{};next.ready=true;
    next.active=(input.flags&VELDIN_CAMPAIGN_ACTIVE)!=0;
    next.travelling=(input.flags&VELDIN_CAMPAIGN_TRAVELLING)!=0;
    next.generation=input.generation;next.source_generation=input.source_generation;
    next.capture_revision=input.capture_revision;next.source_native_level=input.source_native_level;
    for(std::size_t i=0;i<next.coordinates.size();++i){
        next.coordinates[i]=input.coordinates[i]!=0;next.visits[i]=input.visits[i];
    }
    output=next;return CampaignReadStatus::Ready;
}
void CampaignProgress::bind(veldin_garage_query_campaign_v1_fn query) noexcept {
    unbind();query_=query;
}
void CampaignProgress::unbind() noexcept {
    query_=nullptr;observation_={};last_seen_={};have_capture_=false;
    status_=CampaignReadStatus::Unavailable;
}
CampaignReadStatus CampaignProgress::poll(std::uint32_t expected_generation) noexcept {
    observation_={};
    if(!query_)return status_=CampaignReadStatus::Unavailable;
    veldin_campaign_snapshot_v1 raw{};
    try{if(query_(&raw,sizeof raw)!=1)return status_=CampaignReadStatus::QueryFailed;}
    catch(...){return status_=CampaignReadStatus::QueryFailed;}
    CampaignObservation next{};
    const auto decoded=decode_campaign_snapshot(raw,region_,expected_generation,next);
    if(decoded==CampaignReadStatus::NotReady){
        last_seen_={};have_capture_=false;return status_=decoded;
    }
    if(decoded!=CampaignReadStatus::Ready)return status_=decoded;
    if(have_capture_){
        if(next.generation<last_seen_.generation)return status_=CampaignReadStatus::StaleGeneration;
        if(next.capture_revision<last_seen_.capture_revision)return status_=CampaignReadStatus::StaleRevision;
        if(next.source_generation<last_seen_.source_generation)return status_=CampaignReadStatus::StaleSourceGeneration;
        if(next.capture_revision==last_seen_.capture_revision&&!same_capture(next,last_seen_))
            return status_=CampaignReadStatus::InconsistentRevision;
        // A native generation identifies one source world even when another
        // canonical checkpoint is taken within that same load.
        if(next.source_generation==last_seen_.source_generation&&
            next.source_native_level!=last_seen_.source_native_level)return status_=CampaignReadStatus::InconsistentRevision;
    }
    last_seen_=next;have_capture_=true;
    if(!next.active)return status_=CampaignReadStatus::Inactive;
    observation_=next;return status_=CampaignReadStatus::Ready;
}
bool CampaignProgress::copy_coordinates(std::array<bool,VELDIN_CAMPAIGN_PLANETS_V1>& output)const noexcept {
    output={};if(status_!=CampaignReadStatus::Ready)return false;
    output=observation_.coordinates;return true;
}
bool CampaignProgress::copy_visits(std::array<std::uint8_t,VELDIN_CAMPAIGN_PLANETS_V1>& output)const noexcept {
    output={};if(status_!=CampaignReadStatus::Ready)return false;
    output=observation_.visits;return true;
}
}

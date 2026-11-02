// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Exact provider-owned C ABI, copied from Veldin Home include/.
// SHA256 3a52d3860907e886310879cac90a4d0400081af105909e5d2b78762bf0627ae5.
#include "../include/openrac_veldin_campaign_v1.h"
#include <array>
#include <cstdint>

namespace spacevoyages {
inline constexpr const char* campaign_query_export="veldin_garage_query_campaign_v1";
enum class CampaignRegion {Pal,Ntsc,Unknown};
struct CampaignObservation {
    bool ready{},active{},travelling{};
    std::uint32_t generation{},source_generation{};
    std::uint64_t capture_revision{};
    std::int32_t source_native_level=-1;
    std::array<bool,VELDIN_CAMPAIGN_PLANETS_V1> coordinates{};
    // These are native state bytes, not booleans. Values 2..255 are valid.
    std::array<std::uint8_t,VELDIN_CAMPAIGN_PLANETS_V1> visits{};
};
enum class CampaignReadStatus {
    Unavailable,QueryFailed,InvalidSnapshot,WrongRegion,StaleGeneration,
    StaleRevision,StaleSourceGeneration,InconsistentRevision,NotReady,Inactive,Ready
};

// This boundary accepts only canonical PAL data; an NTSC caller cannot use the
// PAL provider as a substitute for its own campaign. No native RAM is read.
CampaignReadStatus decode_campaign_snapshot(const veldin_campaign_snapshot_v1&,
    CampaignRegion,std::uint32_t expected_generation,CampaignObservation&) noexcept;

// The host owns the retained provider lifetime. Every public observation is an
// owned copy, and usable only while READY+ACTIVE in the current native load.
// An unready publication or a new binding starts a fresh capture lifetime.
class CampaignProgress final {
public:
    explicit CampaignProgress(CampaignRegion region=CampaignRegion::Pal) noexcept:region_(region){}
    void bind(veldin_garage_query_campaign_v1_fn) noexcept;
    void unbind() noexcept;
    CampaignReadStatus poll(std::uint32_t expected_generation) noexcept;
    CampaignReadStatus status()const noexcept{return status_;}
    bool bound()const noexcept{return query_!=nullptr;}
    const CampaignObservation& observation()const noexcept{return observation_;}
    bool copy_coordinates(std::array<bool,VELDIN_CAMPAIGN_PLANETS_V1>&)const noexcept;
    bool copy_visits(std::array<std::uint8_t,VELDIN_CAMPAIGN_PLANETS_V1>&)const noexcept;
private:
    CampaignRegion region_;
    veldin_garage_query_campaign_v1_fn query_{};
    CampaignObservation observation_{},last_seen_{};
    CampaignReadStatus status_=CampaignReadStatus::Unavailable;
    bool have_capture_{};
};
}

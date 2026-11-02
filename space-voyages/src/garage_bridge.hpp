// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// The C ABI is copied unchanged from the independent Veldin Home component:
// work/veldin-home/veldin-home/include/openrac_veldin_garage_v1.h
// SHA256 c73f625ccceb32e0ab12b2882d69e27f4fa36c9603a876300153b20b4b1c31d2.
// See that component's docs/GARAGE_API.md. No travel-entry export exists in v1.
#include "../include/openrac_veldin_garage_v1.h"
#include <array>
#include <cstdint>

namespace spacevoyages {
inline constexpr const char* garage_world_id="noire342.veldin-home";
inline constexpr const char* garage_manifest_id="veldin-casa";
inline constexpr const char* garage_query_export="openrac_veldin_garage_query_v1";
enum class GarageRegion {Pal,Ntsc};
enum class GarageShipIdentity:std::uint32_t {
    Original=VELDIN_GARAGE_SHIP_ORIGINAL,Green=VELDIN_GARAGE_SHIP_GREEN,
    Red=VELDIN_GARAGE_SHIP_RED,Purple=VELDIN_GARAGE_SHIP_PURPLE,
    None=VELDIN_GARAGE_SHIP_NONE
};
struct GarageActorObservation {
    GarageShipIdentity ship=GarageShipIdentity::None;
    std::int32_t native_slot=-1;
    std::uint32_t native_class{},generation{};
    bool visible{};
};
struct GarageObservation {
    bool ready{},active{},travelling{};
    std::uint32_t owned_mask{},generation{},actor_count{};
    GarageShipIdentity selected_ship=GarageShipIdentity::None;
    std::uint64_t revision{};
    std::array<GarageActorObservation,4> actors{};
};
enum class GarageReadStatus {
    Unavailable,QueryFailed,InvalidSnapshot,WrongWorld,WrongRegion,
    StaleGeneration,StaleRevision,InconsistentRevision,NotReady,Ready
};
// Pure validation. Even on failure, output is cleared rather than retaining a
// formerly selected ship or actor. The expected generation comes from the host,
// not from a native slot, level18 or the snapshot being checked.
GarageReadStatus decode_garage_snapshot(const veldin_garage_snapshot_v1&,
    GarageRegion,std::uint32_t expected_load_generation,GarageObservation&) noexcept;

// Read-only observer: no DLL loading, native memory, save access, selection or
// routing. The host resolves the export ONLY from an already loaded provider,
// keeps that module alive during poll(), and calls unbind() before unload.
// A successful READY observation is a fleet/selection fact, not proof that the
// loaded world is home: ACTIVE and the validated logical identity are separate.
class GarageBridge final {
public:
    explicit GarageBridge(GarageRegion region=GarageRegion::Pal) noexcept:region_(region){}
    void bind(openrac_veldin_garage_query_v1_fn query) noexcept;
    void unbind() noexcept;
    GarageReadStatus poll(std::uint32_t expected_load_generation) noexcept;
    const GarageObservation& observation()const noexcept{return observation_;}
    GarageReadStatus status()const noexcept{return status_;}
private:
    GarageRegion region_;
    openrac_veldin_garage_query_v1_fn query_{};
    GarageObservation observation_{},last_seen_{};
    GarageReadStatus status_=GarageReadStatus::Unavailable;
    bool have_revision_{};
};
}

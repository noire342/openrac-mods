// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "boarding_timeline.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace spacevoyages {
// Stable garage identity is separate from both the native travel controller
// and the asset/profile slot. In particular Original and Green share531 only
// as a carrier; their visible models and identities are never interchangeable.
enum class ShipIdentity:std::uint8_t {Original=0,Green=1,Red=2,Purple=3};
inline constexpr std::size_t ship_profile_count=4;
struct ShipProfile {
    ShipIdentity identity;
    const char* asset_id;
    std::size_t slot;
    int native_carrier_class;
    bool weapons_allowed;
    int world_actor_class;
    boarding::EntryPresentation boarding_entry=boarding::EntryPresentation::OmitConsole;
};
inline constexpr std::array<ShipProfile,ship_profile_count> ship_profiles{{
    {ShipIdentity::Green,"ship_531",0,531,false,531},
    {ShipIdentity::Red,"ship_532",1,532,true,532},
    {ShipIdentity::Purple,"ship_533",2,533,true,533},
    // The transparent cockpit exposes the donor's actual climb during103..242.
    // Keep that entry arc; the terminal dashboard is still omitted at464.
    {ShipIdentity::Original,"ship_original",3,531,false,2047,boarding::EntryPresentation::PreserveEntry}
}};
constexpr const ShipProfile* ship_profile(ShipIdentity identity)noexcept {
    for(const auto& profile:ship_profiles)if(profile.identity==identity)return &profile;
    return nullptr;
}
constexpr std::optional<ShipIdentity> ship_identity_from_native_class(int native_class)noexcept {
    for(const auto& profile:ship_profiles)
        if(profile.identity!=ShipIdentity::Original&&profile.native_carrier_class==native_class)return profile.identity;
    return std::nullopt;
}
constexpr bool compatible_boarding_actor(ShipIdentity identity,int actual_class)noexcept {
    const auto* profile=ship_profile(identity);
    // The custom world actor remains2047 outside the provider's temporary
    // native531 call scope. Only a separately verified identity authorizes it.
    return profile&&(actual_class==profile->native_carrier_class||actual_class==profile->world_actor_class);
}
constexpr std::optional<ShipIdentity> ship_identity_from_stable_id(std::uint32_t stable_id)noexcept {
    for(const auto& profile:ship_profiles)if(std::uint32_t(profile.identity)==stable_id)return profile.identity;
    return std::nullopt;
}
}

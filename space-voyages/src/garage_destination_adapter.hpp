// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "registered_destination.hpp"
#include "destination_route.hpp"

namespace spacevoyages {
// Adapter for the current Veldin body/story policy. Adding another custom
// destination does not require changing the generic SDK gateway.
inline constexpr char veldin_destination_manifest[]="veldin-casa";
constexpr RegisteredDestination registered_route(DestinationRoute route)noexcept {
    if(!route.valid())return {};
    return route.variant==DestinationVariant::Campaign?RegisteredDestination::campaign(route.native_level):
        RegisteredDestination::added(veldin_destination_manifest,route.native_level);
}
constexpr DestinationRoute galaxy_route(RegisteredDestination route)noexcept {
    if(!route.valid())return {};
    if(route.kind==RegisteredDestinationKind::Campaign)return {route.native_level};
    return route==RegisteredDestination::added(veldin_destination_manifest,18)?
        DestinationRoute{18,DestinationVariant::VeldinGarage}:DestinationRoute{};
}
}

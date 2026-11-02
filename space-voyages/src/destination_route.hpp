// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>

namespace spacevoyages {
// Program18 is shared by two distinct mounted destinations. This value names
// the route selected by the host; it is not an overlay index or a native flag.
enum class DestinationVariant {Campaign,VeldinGarage};
struct DestinationRoute {
    int native_level=-1;
    DestinationVariant variant=DestinationVariant::Campaign;
    constexpr bool valid()const noexcept {
        return (variant==DestinationVariant::Campaign&&native_level>0&&native_level<19)||
            (variant==DestinationVariant::VeldinGarage&&native_level==18);
    }
};
constexpr bool operator==(DestinationRoute a,DestinationRoute b) noexcept {
    return a.native_level==b.native_level&&a.variant==b.variant;
}
constexpr bool operator!=(DestinationRoute a,DestinationRoute b) noexcept{return !(a==b);}
struct RouteAvailability {
    // True only after the host verifies SDK destination-entry support and a
    // compatible registered garage route for this disc/runtime. A read-only
    // garage query, resident level18, or an observed menu row is insufficient.
    bool veldin_garage=false;
};
// Pure eligibility/resolution, with no SDK calls, manifest strings, registry
// slots, save writes or inferred campaign flags. Default availability retains
// the original campaign-only behavior; native tutorial0 is never a route.
inline DestinationRoute resolve_destination_route(int marker,
    const std::array<bool,19>& coordinates,DestinationRoute origin,
    RouteAvailability available={}) noexcept {
    if(marker<0||marker>=19)return {};
    if(marker==0||marker==18){
        // A route home to campaign18 remains valid without inventing its flag.
        if(coordinates[18]||origin==DestinationRoute{18,DestinationVariant::Campaign})
            return {18,DestinationVariant::Campaign};
        return available.veldin_garage?DestinationRoute{18,DestinationVariant::VeldinGarage}:DestinationRoute{};
    }
    if(coordinates[unsigned(marker)]||origin==DestinationRoute{marker,DestinationVariant::Campaign})
        return {marker,DestinationVariant::Campaign};
    return {};
}
}

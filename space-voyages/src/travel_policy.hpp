// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace spacevoyages {
// Eligibility is separate from the flight simulation and native boarding
// detector. Preserve first story escapes, boss departures and Gemlik's fighter
// mission; normal ship-state0 alone does not identify an ordinary departure.
inline bool ordinary_departure_allowed(int origin,const std::array<bool,19>& coordinates,
    const std::array<std::uint8_t,19>& visits) noexcept {
    if(origin<=0||origin>=19)return false;
    switch(origin){
        case 7:case 14:case 17:return visits[unsigned(origin)]==2;
        case 13:return coordinates[14];
        default:return true;
    }
}
}

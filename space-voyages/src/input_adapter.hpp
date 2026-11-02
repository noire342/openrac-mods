// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "flight.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spacevoyages {
enum class VerticalInput {NativeAircraft,Direct};
// Gemlik's primary gun: native PadState0x84 is byte-swapped relative to the
// SDK report. Square/L1 are0x8000/0x0400 here. Ownership is checked by combat.
inline bool weapon_fire_from_pad(std::uint16_t held)noexcept{return (held&0x8400)!=0;}

// Ratchet & Clank's Pad_ProcessReport (FUN_00217328), before HeroUpdate's
// d-pad fallback. The centre is127, with48 raw steps of deadzone and76
// remaining steps of range. It is deliberately not a modern12% stick curve.
inline float native_pad_axis(std::uint8_t raw) noexcept {
    const int delta=int(raw)-127;
    const int magnitude=delta<0?-delta:delta;
    if(magnitude<48)return 0.f;
    const float value=std::min(float(magnitude-48)/76.f,1.f);
    return delta<0?-value:value;
}

// `held` uses the frontend's active-high DualShock report order, rather than
// the byte-swapped PadState order: D-pad U/R/D/L=0x10/20/40/80, Cross=0x4000.
// `analog` is RX,RY,LX,LY as openrac_transient_source.tick supplies it.
inline FlightInput flight_input_from_pad(const std::uint8_t analog[4],std::uint16_t held,
    VerticalInput vertical=VerticalInput::NativeAircraft,bool mirror_horizontal=false) noexcept {
    if(!analog)return {};
    float x=native_pad_axis(analog[2]),y=native_pad_axis(analog[3]);
    if(mirror_horizontal)x=-x;
    // HeroUpdate consumes the native post-deadzone axes. Close to centre,
    // retail selects actual d-pad buttons; synthetic stick directions are
    // absent from PadState.raw, so they must not be used here.
    if(std::sqrt(x*x+y*y)<.25f){
        x=float((held&0x20)!=0)-float((held&0x80)!=0);
        y=float((held&0x40)!=0)-float((held&0x10)!=0);
        if(mirror_horizontal)x=-x;
    }
    if(vertical==VerticalInput::Direct)y=-y;
    return {x,y,(held&0x4000)!=0};
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "equip_assembly.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
namespace {
constexpr float screw_turns=12.56637061435917295385f;
float smooth(float value){
    const float x=std::clamp(value,0.f,1.f);
    return x*x*x*(x*(x*6.f-15.f)+10.f);
}
}
void EquipAssembly::begin(bool has_ammunition){
    elapsed_=turn_elapsed_=0;
    powered_=has_ammunition;active_=true;
}
void EquipAssembly::reset(){
    elapsed_=turn_elapsed_=0;powered_=active_=false;
}
void EquipAssembly::update(const EquipAssemblyFrame& frame){
    if(!active_||!frame.playing||frame.paused)return;
    // Input is never held for this presentation. Its first real attack uses
    // the complete weapon immediately, preserving the current shaft phase.
    if(frame.attack){active_=false;return;}
    if(!std::isfinite(frame.seconds)||frame.seconds<=0||frame.seconds>.1f)return;
    elapsed_=std::min(duration,elapsed_+frame.seconds);
    if(powered_&&frame.has_ammunition)
        turn_elapsed_=std::min(screw_duration,turn_elapsed_+frame.seconds);
    if(elapsed_>=duration-1.e-6f)active_=false;
}
EquipAssemblyPose EquipAssembly::pose()const{
    EquipAssemblyPose result;
    // Retain a stopped partial phase if ammunition disappeared mid-assembly.
    // A constant phase is also harmless after an attack interrupts the intro.
    // This prevents a stopped drill snapping back to its pre-equip angle.
    const float screw_time=screw_duration-screw_delay;
    result.head_turn=screw_turns*smooth((turn_elapsed_-screw_delay)/screw_time);
    if(!active_)return result;
    result.housing_scale=elapsed_<housing_peak_time?
        initial_housing_scale+(maximum_housing_scale-initial_housing_scale)*smooth(elapsed_/housing_peak_time):
        maximum_housing_scale+(1.f-maximum_housing_scale)*smooth((elapsed_-housing_peak_time)/(housing_settle_time-housing_peak_time));
    // The head's base stays at its physical socket. Extending the existing
    // connected geometry while rotating it reads as a drill screwing together,
    // without inventing floating pieces or changing the weapon's source mesh.
    result.head_axial_scale=initial_head_scale+(1.f-initial_head_scale)*smooth((elapsed_-screw_delay)/screw_time);
    result.active=true;
    return result;
}
}

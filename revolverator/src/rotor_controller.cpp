// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "rotor_controller.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
namespace {
constexpr float tau=6.2831853071795864769f;
float wrapped(float angle){return std::remainder(angle,tau);}
}
void RotorController::reset(float angle){
    angle_=std::isfinite(angle)?wrapped(angle):0;
    speed_=phase_offset_=dry_base_=dry_elapsed_=0;
    activation_=pulse_remaining_=0;
    dry_=synchronized_=false;
}
void RotorController::dry_fire(){
    if(dry_)return;
    dry_=true;dry_base_=angle_;dry_elapsed_=0;synchronized_=false;speed_=0;
    activation_=pulse_remaining_=0;
}
void RotorController::update(const RotorFrame& frame){
    if(!frame.playing||frame.paused||!std::isfinite(frame.seconds)||frame.seconds<=0||frame.seconds>.1f)return;
    const float previous=angle_;
    const bool catching=frame.phase==Phase::Windup||frame.phase==Phase::Spinning;
    if(frame.phase==Phase::Spinning){activation_=pulse_remaining_=0;}
    else{
        if(frame.phase==Phase::Windup)pulse_remaining_=pulse_duration;
        else pulse_remaining_=std::max(0.f,pulse_remaining_-frame.seconds);
        const float target=pulse_remaining_>0?1.f:0.f;
        const float step=frame.seconds*(target>activation_?20.f:12.f);
        activation_+=std::clamp(target-activation_,-step,step);
    }
    if(dry_&&catching){angle_=dry_base_;dry_=false;dry_elapsed_=0;}
    if(dry_){
        dry_elapsed_+=frame.seconds;
        if(dry_elapsed_+1e-6f>=dry_duration){angle_=dry_base_;dry_=false;speed_=0;return;}
        float excursion;
        if(dry_elapsed_<=.06f){
            const float u=std::clamp(dry_elapsed_/.06f,0.f,1.f);
            excursion=1-(1-u)*(1-u)*(1-u);
        }else{
            const float u=std::clamp((dry_elapsed_-.06f)/.12f,0.f,1.f);
            excursion=1-u*u*(3-2*u);
        }
        angle_=wrapped(dry_base_+dry_excursion*excursion);
        speed_=wrapped(angle_-previous)/frame.seconds;
        return;
    }
    if(frame.phase==Phase::Spinning){
        if(!std::isfinite(frame.enemy_angle))return;
        if(!synchronized_){phase_offset_=wrapped(angle_-frame.enemy_angle);synchronized_=true;}
        angle_=wrapped(frame.enemy_angle+phase_offset_);
        speed_=wrapped(angle_-previous)/frame.seconds;
        return;
    }
    synchronized_=false;
    speed_=pulse_remaining_>0?windup_speed:frame.ammunition>0?idle_speed:0;
    angle_=wrapped(angle_+speed_*frame.seconds);
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_contact_sparks.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace revolverator {
namespace {
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 normalized(Vec3 v){return v*(1.f/std::sqrt(length_squared(v)));}
}
NativeContactSparks::NativeContactSparks(const openrac_api* api):Memory(api),
    spawn_(reinterpret_cast<Spawn>(find_function("FUN_L00_0026c6e8"))){}
unsigned NativeContactSparks::update(const Frame& frame,Target target,Vec3 point,Vec3 shaft,float angle,float speed){
    const bool active=spawn_&&frame.playing&&frame.alive&&frame.equipped&&target&&
        finite(point)&&finite(shaft)&&length_squared(shaft)>.25f&&length_squared(shaft)<4.f&&std::isfinite(angle);
    const auto count=rate_.update(frame.seconds,active,frame.paused,target,speed);
    if(!count||!api_->stack_push||!api_->stack_pop)return 0;
    unsigned saved{};const auto stack=api_->stack_push(32,&saved);
    if(!stack){rate_.reset();return 0;}
    struct Pop{const openrac_api* api;unsigned saved;~Pop(){api->stack_pop(saved);}} pop{api_,saved};
    // The resident wrench-contact spawner copies both aligned vectors into
    // native type19 streaks. It owns allocation and their ~20-tick lifetime.
    // No native particle, model pointer or guest stack address is retained.
    shaft=normalized(shaft);
    const auto radial=normalized(cross(shaft,std::abs(shaft.z)<.9f?Vec3{0,0,1}:Vec3{1,0,0}));
    const auto tangent=cross(shaft,radial);
    const float intensity=std::clamp((speed-24.f)/18.f,0.f,1.f);
    for(unsigned i=0;i<count;++i){
        // Local deterministic dispersion avoids drawing from gameplay RNG.
        const float phase=std::remainder(angle+float(sequence_%4096u)*2.39996323f,6.283185307f);++sequence_;
        const auto outward=radial*std::cos(phase)+tangent*std::sin(phase);
        const auto velocity=cross(shaft,outward)*(.065f+.025f*intensity)+outward*.025f+shaft*.015f;
        const auto origin=point+outward*.012f;
        const std::array<float,4> position{origin.x,origin.y,origin.z,1},direction{velocity.x,velocity.y,velocity.z,0};
        std::memcpy(api_->memory+stack,position.data(),16);std::memcpy(api_->memory+stack+16,direction.data(),16);
        spawn_(stack,stack+16);++emitted_;
    }
    return count;
}
}

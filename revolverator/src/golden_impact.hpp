// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include <array>
#include <cstddef>
#include <optional>

namespace revolverator {

// Damage eligibility is independent from capture eligibility. Native GetHit
// still decides armor, boss phases, destruction, drops and death bookkeeping.
bool golden_damage_class(unsigned klass);

struct GoldenBlast {
    float charge{},radius{},damage{},visual_scale{},flash{},knockback{};
    float damage_at(float surface_distance)const;
};

// A shot owns the variant and charge selected at release. Changing the equipped
// weapon or its upgrade later cannot promote, weaken or re-arm this shot.
class GoldenImpact final {
public:
    void launch(Target projectile,bool golden,float normalized_charge,float multiplier);
    bool pending(Target projectile)const{return projectile_&&projectile_==projectile;}
    bool golden_flight(Target projectile)const{return pending(projectile)&&golden_;}
    std::optional<GoldenBlast> consume(Target projectile,ImpactReason reason);
    void cancel(){projectile_={};golden_=false;blast_={};}
private:
    Target projectile_{};
    bool golden_{};
    GoldenBlast blast_{};
};

struct GoldenBlastHit {
    Target target{};
    Vec3 direction{};
    float damage{},distance{};
    bool primary{};
};

// Native PostHit has a 64-record ring. A single burst admits at most 48 distinct
// nearby actors, reserving room for unrelated native hits in this same tick.
// The thrown actor and actual contact are retained; the nearest remaining
// visible actors win. No game memory or native functions are used here.
class GoldenBlastPlan final {
public:
    static constexpr std::size_t maximum_hits=48;
    GoldenBlastPlan(GoldenBlast blast,Vec3 centre):blast_(blast),centre_(centre){}
    bool consider(Target target,Vec3 position,float collision_radius,bool visible,bool primary=false);
    const std::array<GoldenBlastHit,maximum_hits>& hits()const{return hits_;}
    std::size_t size()const{return size_;}
private:
    GoldenBlast blast_{};
    Vec3 centre_{};
    std::array<GoldenBlastHit,maximum_hits> hits_{};
    std::size_t size_{};
};
}

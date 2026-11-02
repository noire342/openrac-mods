// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

namespace revolverator {
// A cosmetic envelope, independent of native actors, input ownership or combat.
// The native visual owner starts it only on a real equip/actor transition.
struct EquipAssemblyPose {
    float housing_scale=1.f;
    float head_axial_scale=1.f;
    float head_turn=0.f;
    bool active=false;
};
struct EquipAssemblyFrame {
    float seconds=1.f/60.f;
    bool playing=true;
    bool paused=false;
    bool attack=false;
    bool has_ammunition=true;
};
class EquipAssembly final {
public:
    // Native Blaster/Pyrocitor seq0 starts with hidden parts, then pops the
    // body and progressively builds its barrel. Keep that readable staging:
    // the motor appears first, followed by a longer screw deployment.
    static constexpr float duration=.60f;
    static constexpr float housing_peak_time=.13f;
    static constexpr float housing_settle_time=.29f;
    static constexpr float screw_delay=.055f;
    static constexpr float screw_duration=.55f;
    static constexpr float initial_housing_scale=.001f;
    static constexpr float maximum_housing_scale=1.22f;
    static constexpr float initial_head_scale=.025f;
    void begin(bool has_ammunition);
    void update(const EquipAssemblyFrame&);
    void reset();
    bool active()const{return active_;}
    EquipAssemblyPose pose()const;
private:
    float elapsed_{},turn_elapsed_{};
    bool active_{},powered_{};
};
}

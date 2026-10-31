// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"

namespace revolverator {
struct RotorFrame {
    int ammunition{};
    Phase phase=Phase::Idle;
    float charge{},enemy_angle{},seconds{};
    bool playing{},paused{};
};

// Pure motor timing. The adapter supplies the held enemy's authoritative angle,
// so the head cannot drift out of phase by independently integrating its speed.
class RotorController {
public:
    void update(const RotorFrame& frame);
    void dry_fire();
    void reset(float angle=0);
    float angle()const{return angle_;}
    float speed()const{return speed_;}
    float radial_scale()const{return 1.f+(maximum_radial_scale-1.f)*activation_;}
    float axial_scale()const{return 1.f+(maximum_axial_scale-1.f)*activation_;}
    bool dry_firing()const{return dry_;}
    bool synchronized()const{return synchronized_;}
    static constexpr float idle_speed=1.f;
    static constexpr float windup_speed=55.f;
    static constexpr float maximum_radial_scale=2.f,maximum_axial_scale=1.20f;
    static constexpr float pulse_duration=.16f;
    static constexpr float dry_duration=.18f;
    static constexpr float dry_excursion=.20f;
private:
    float angle_{},speed_{},phase_offset_{},dry_base_{},dry_elapsed_{};
    float activation_{},pulse_remaining_{};
    bool dry_{},synchronized_{};
};
}

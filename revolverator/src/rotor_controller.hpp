// SPDX-License-Identifier: MIT
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
    bool dry_firing()const{return dry_;}
    bool synchronized()const{return synchronized_;}
    static constexpr float idle_speed=1.f;
    static constexpr float dry_duration=.18f;
    static constexpr float dry_excursion=.20f;
private:
    float angle_{},speed_{},phase_offset_{},dry_base_{},dry_elapsed_{};
    bool dry_{},synchronized_{};
};
}

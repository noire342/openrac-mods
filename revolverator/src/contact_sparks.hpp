// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
namespace revolverator {
// Cosmetic rate limiter. It owns no actors, ammunition, combat or game RNG.
class ContactSparks final {
public:
    unsigned update(float seconds,bool spinning,bool paused,Target target,float angular_speed);
    void reset(){target_={};credit_=0;}
    static constexpr unsigned maximum_per_frame=4;
private:
    Target target_{};float credit_{};
};
}

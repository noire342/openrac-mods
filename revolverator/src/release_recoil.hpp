// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"

namespace revolverator {
// Only a locomotion hold. It never delays the projectile, advances animation,
// changes the camera, or owns an actor. The native adapter clamps Ratchet's
// verified horizontal movement fields while active().
class ReleaseRecoil final {
public:
    static constexpr float duration=.15f;
    void start(const Frame& frame);
    void tick(const Frame& frame);
    void cancel(){remaining_=0;}
    bool active()const{return remaining_>0;}
    float remaining()const{return remaining_;}
private:
    float remaining_{};
    unsigned epoch_{};
};
}

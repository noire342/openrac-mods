// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "release_recoil.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
void ReleaseRecoil::start(const Frame& f){
    cancel();
    if(!f.equipped||!f.alive||!f.playing||f.paused||!finite(f.position)||!std::isfinite(f.yaw))return;
    epoch_=f.epoch;remaining_=duration;
}
void ReleaseRecoil::tick(const Frame& f){
    if(!active())return;
    if(f.epoch!=epoch_||!f.equipped||!f.alive||!finite(f.position)||!std::isfinite(f.yaw)){
        cancel();return;
    }
    if(f.paused)return;
    if(!f.playing||!std::isfinite(f.seconds)||f.seconds<=0||f.seconds>.1f){cancel();return;}
    remaining_=std::max(0.f,remaining_-f.seconds);
    if(remaining_<.000001f)remaining_=0;
}
}

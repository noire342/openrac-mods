// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "contact_sparks.hpp"
#include <algorithm>
#include <cmath>
namespace revolverator {
unsigned ContactSparks::update(float seconds,bool spinning,bool paused,Target target,float speed){
    if(!spinning||!target||!std::isfinite(speed)||speed<=1.f){reset();return 0;}
    if(paused)return 0;
    if(!std::isfinite(seconds)||seconds<=0||seconds>.1f){credit_=0;return 0;}
    if(!(target==target_)){target_=target;credit_=1.f;}
    const float intensity=std::clamp((std::abs(speed)-24.f)/18.f,0.f,1.f);
    credit_=std::min(float(maximum_per_frame),credit_+seconds*(24.f+18.f*intensity));
    const auto count=unsigned(credit_);credit_-=float(count);return count;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "arm_math.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 unit(Vec3 v) {
    const float n = std::sqrt(length_squared(v));
    return std::isfinite(n) && n > .00001f ? v*(1/n) : Vec3{};
}
Quaternion shortest_arc(Vec3 from, Vec3 to) {
    const auto a = unit(from), b = unit(to);
    const float d = std::clamp(dot(a,b), -1.f, 1.f);
    Vec3 cross{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
    if (d < -.9999f) {
        const Vec3 other = std::abs(a.x) < .8f ? Vec3{1,0,0} : Vec3{0,1,0};
        cross = unit({a.y*other.z-a.z*other.y, a.z*other.x-a.x*other.z, a.x*other.y-a.y*other.x});
        return {cross.x,cross.y,cross.z,0};
    }
    const float n = std::sqrt(length_squared(cross)+(1+d)*(1+d));
    return n > .00001f ? Quaternion{cross.x/n,cross.y/n,cross.z/n,(1+d)/n} : Quaternion{0,0,0,1};
}
Quaternion blend_rotation(Quaternion to, float weight) {
    weight = std::clamp(weight,0.f,1.f);
    if(to[3] < 0) for(auto& v:to) v=-v;
    Quaternion q{to[0]*weight,to[1]*weight,to[2]*weight,1+(to[3]-1)*weight};
    float n=0;for(float v:q)n+=v*v;n=std::sqrt(n);
    if(n>.00001f)for(auto& v:q)v/=n;
    return q;
}
std::optional<ArmGoal> arm_goal(Vec3 shoulder, float upper, float lower,
                                Vec3 outward, Vec3 forward, float raise) {
    if(!finite(shoulder)||!finite(outward)||!finite(forward)||
       !std::isfinite(upper)||!std::isfinite(lower)||upper<=.001f||lower<=.001f) return {};
    outward=unit(outward); forward=unit(forward);
    const float reach=upper+lower;
    // Keep the wrist outside the ear and the elbow farther out still. The arm
    // reaches diagonally upward, rather than folding the clavicle over the skull.
    Vec3 offset=outward*(reach*.66f)+forward*(reach*.10f)+Vec3{0,0,reach*.67f};
    const float distance=std::clamp(std::sqrt(length_squared(offset)),
        std::abs(upper-lower)+.0001f,reach*.96f);
    const Vec3 direction=unit(offset);
    const Vec3 pole=unit(outward-direction*dot(outward,direction));
    const float along=(upper*upper-lower*lower+distance*distance)/(2*distance);
    const float side=std::sqrt(std::max(0.f,upper*upper-along*along));
    (void)raise;
    return ArmGoal{shoulder+direction*along+pole*side,shoulder+direction*distance};
}
}

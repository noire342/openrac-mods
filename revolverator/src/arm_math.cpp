// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
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
namespace {
std::optional<ArmGoal> solve_arm(Vec3 shoulder,float upper,float lower,Vec3 outward,Vec3 offset) {
    if(!finite(shoulder)||!finite(outward)||!finite(offset)||
       !std::isfinite(upper)||!std::isfinite(lower)||upper<=.001f||lower<=.001f) return {};
    outward=unit(outward);
    const float reach=upper+lower;
    const float distance=std::clamp(std::sqrt(length_squared(offset)),
        std::abs(upper-lower)+.0001f,reach*.96f);
    const Vec3 direction=unit(offset);
    const Vec3 pole=unit(outward-direction*dot(outward,direction));
    const float along=(upper*upper-lower*lower+distance*distance)/(2*distance);
    const float side=std::sqrt(std::max(0.f,upper*upper-along*along));
    return ArmGoal{shoulder+direction*along+pole*side,shoulder+direction*distance};
}
}
std::optional<ArmGoal> arm_goal(Vec3 shoulder,float upper,float lower,
                                Vec3 outward,Vec3 forward,float raise) {
    if(!finite(forward))return {};
    const float reach=upper+lower;
    (void)raise;
    return solve_arm(shoulder,upper,lower,outward,
        unit(outward)*(reach*.66f)+unit(forward)*(reach*.10f)+Vec3{0,0,reach*.67f});
}
std::optional<ArmGoal> throw_goal(Vec3 shoulder,float upper,float lower,
                                  Vec3 outward,Vec3 forward,float progress) {
    if(!std::isfinite(progress)||!finite(forward))return {};
    const float p=std::clamp(progress,0.f,1.f),reach=upper+lower;
    const float u=std::clamp(p/.55f,0.f,1.f),s=u*u*(3-2*u);
    const float tail=std::clamp((p-.55f)/.45f,0.f,1.f);
    // Sweep the raised hand forward, then let it follow through below the
    // shoulder. The same outward pole keeps both solutions outside the head.
    const Vec3 offset=unit(outward)*(reach*(.66f-.36f*s))+
        unit(forward)*(reach*(.10f+.75f*s-.09f*tail))+
        Vec3{0,0,reach*(.67f-.57f*s-.26f*tail)};
    return solve_arm(shoulder,upper,lower,outward,offset);
}
float throw_lean(float progress) {
    if(!std::isfinite(progress)||progress<=0||progress>=1)return 0;
    const float t=progress<.4f?progress/.4f:(1-progress)/.6f;
    return throw_lean_radians*t*t*(3-2*t);
}
}

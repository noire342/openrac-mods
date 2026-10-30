// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "attachment.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
Quaternion multiply(Quaternion a,Quaternion b) {
    return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
            a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
            a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
            a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
}
Quaternion rotation_from_euler(Vec3 a) {
    const float x=std::sin(a.x*.5f),X=std::cos(a.x*.5f);
    const float y=std::sin(a.y*.5f),Y=std::cos(a.y*.5f);
    const float z=std::sin(a.z*.5f),Z=std::cos(a.z*.5f);
    return {x*Y*Z-X*y*z,X*y*Z+x*Y*z,X*Y*z-x*y*Z,X*Y*Z+x*y*z};
}
Vec3 rotation_to_euler(Quaternion q) {
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    const float sine=2*(w*y-z*x);
    if(std::abs(sine)>.99999f)
        return {0,std::copysign(1.570796327f,sine),std::atan2(2*(w*z-x*y),1-2*(x*x+z*z))};
    return {std::atan2(2*(w*x+y*z),1-2*(x*x+y*y)),
        std::asin(std::clamp(2*(w*y-z*x),-1.f,1.f)),
        std::atan2(2*(w*z+x*y),1-2*(y*y+z*z))};
}
Vec3 rotate(Quaternion q,Vec3 p) {
    const Quaternion inverse{-q[0],-q[1],-q[2],q[3]};
    const auto value=multiply(multiply(q,{p.x,p.y,p.z,0}),inverse);
    return {value[0],value[1],value[2]};
}
CaptureGrip capture_grip(Vec3 origin,Vec3 angles,Vec3 contact,Vec3 approach,float angle) {
    CaptureGrip grip;grip.original=rotation_from_euler(angles);grip.initial_angle=angle;
    const auto q=grip.original;
    grip.local_point=rotate({-q[0],-q[1],-q[2],q[3]},contact-origin);
    grip.lift=shortest_arc(approach,{0,0,1});
    return grip;
}
AttachedPose attach_to_tip(const CaptureGrip& grip,Vec3 tip,float angle,float lift) {
    const float delta=angle-grip.initial_angle;
    const Quaternion spin{0,0,std::sin(delta*.5f),std::cos(delta*.5f)};
    const auto q=multiply(spin,multiply(blend_rotation(grip.lift,lift),grip.original));
    return {tip-rotate(q,grip.local_point),rotation_to_euler(q),q};
}
}

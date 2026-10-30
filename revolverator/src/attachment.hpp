// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arm_math.hpp"

namespace revolverator {
Quaternion multiply(Quaternion a,Quaternion b);
Quaternion rotation_from_euler(Vec3 angles);
Vec3 rotation_to_euler(Quaternion q);
Vec3 rotate(Quaternion q,Vec3 point);
struct CaptureGrip {
    Vec3 local_point{};
    Quaternion original{0,0,0,1}, lift{0,0,0,1};
    float initial_angle{};
};
struct AttachedPose { Vec3 origin{},angles{};Quaternion rotation{0,0,0,1}; };
CaptureGrip capture_grip(Vec3 origin,Vec3 angles,Vec3 contact,Vec3 approach,float angle);
AttachedPose attach_to_tip(const CaptureGrip& grip,Vec3 tip,float angle,float lift);
}

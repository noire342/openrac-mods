// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include <array>

namespace revolverator {
using Quaternion = std::array<float, 4>;
float dot(Vec3 a, Vec3 b);
Vec3 unit(Vec3 v);
Quaternion shortest_arc(Vec3 from, Vec3 to);
Quaternion blend_rotation(Quaternion to, float weight);

struct ArmGoal { Vec3 elbow, hand; };
// The pole fixes which side the elbow bends toward. In particular, an overhead
// target must not choose the equally reachable bend through Ratchet's head.
std::optional<ArmGoal> arm_goal(Vec3 shoulder, float upper, float lower,
                                Vec3 outward, Vec3 forward, float raise);
}

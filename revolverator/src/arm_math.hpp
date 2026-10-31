// SPDX-License-Identifier: GPL-3.0-or-later
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
std::optional<ArmGoal> throw_goal(Vec3 shoulder, float upper, float lower,
                                  Vec3 outward, Vec3 forward, float progress);
// Small additive spine bend, in radians. Zero disables the experiment without
// changing the arm trajectory, projectile release or recovery timing.
inline constexpr float throw_lean_radians = .20943951f; // 12 degrees total.
float throw_lean(float progress);
}

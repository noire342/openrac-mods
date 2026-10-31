// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>

namespace revolverator::gold_shop_layout {
inline constexpr float display_scale=.1458333283662796f*2.1f;
inline constexpr float display_yaw=-1.57079632679f;
// Full rigid2019 mesh bounds, not the grip/root of the held weapon. Native
// Novalis Gold models have a median visual-centre height of1.7747m above cases.
inline constexpr std::array<float,3> mesh_center{{.09664209855f,-2.57684492094f,.92004388653f}};
inline constexpr float center_height=1.775f;
inline constexpr float interaction_radius=3.5f;
inline constexpr std::array<float,4> display_origin(const std::array<float,4>& base){
    // At yaw -pi/2, R(center) is (center.y,-center.x,center.z).
    return {{base[0]-mesh_center[1]*display_scale,
             base[1]+mesh_center[0]*display_scale,
             base[2]+center_height-mesh_center[2]*display_scale,1.f}};
}
}

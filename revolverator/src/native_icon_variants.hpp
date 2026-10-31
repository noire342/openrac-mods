// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "hd_icon.hpp"
#include <array>

namespace revolverator {
// Derives menu variants from the resident native HUD, without embedding game
// artwork or changing its tables. All variants own their pixels and CLUT;
// HdIcon::install copies all returned data into its own guest allocation.
class NativeIconVariants {
public:
 bool build(const Memory& memory,const HdIconData& normal);
 HdIconData normal()const;
 HdIconData ammo()const;
 HdIconData golden()const;
 void reset();
private:
 std::array<std::uint8_t,1024> ammo_pixels_{},ammo_palette_{},gold_palette_{},normal_palette_{};
 std::array<std::uint8_t,16384> gold_pixels_{},normal_pixels_{};
 std::size_t normal_bytes_{};
 bool ready_{};
};
}

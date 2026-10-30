// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>

namespace weaponxp {
struct Disc {
    const char* version;
    std::uint32_t hero, item_in_hand, tick_seconds;
    std::uint32_t mode, item_table, hit_ring;
    std::uint32_t video_mode, hud_parameters;
    std::uint32_t hud_bank;
    const char* rectangle;
    const char* weapon_draw;
    const char* relay;
    std::uint32_t small_font_metrics;
    const char* quad;
    const char* texture;
    const char* gauge_draw;
    const char* screen_position;
    const char* transform;
    const char* drone_spawn;
};

// Resident globals use relocate_data; overlay globals use relocate_level_data(0, ...).
// Same addresses as difficulty-mod/src/discs.h; fonts/rectangles from OpenRAC's US map.
inline constexpr Disc kDiscs[] = {
    {"pal",
     0x0013F450,
     0x00140508,
     0x0015EE7C,
     0x0015F6A8,
     0x001C43B0,
     0x00178200,
     0x0015EE80,
     0x0015F9F4,
     0x00199C60,
     "func_00201640",
     "func_L00_0023B140",
     "func_L00_002A7078",
     0x001DF770,
     "func_001F5800",
     "func_001F4868",
     "func_L00_002381D0",
     "func_L00_00236400",
     "func_L00_00236468",
     "func_L00_002CF6A0"},
    {"ntsc",         0x0013F350,         0x00140408,         0x0015ED7C,         0x0015F5C4,
     0x001C40B0,     0x00178100,         0x0015ED80,         0x0015F91C,         0x00199B60,
     "FUN_00200e08", "FUN_L00_0023a7a8", "FUN_L00_002a5dd8", 0x001DF3F0,         "FUN_001f5450",
     "FUN_001f44b8", "FUN_L00_00237840", "FUN_L00_00235a70", "FUN_L00_00235ad8", "FUN_L00_002ce1f0"}
};
}  // namespace weaponxp

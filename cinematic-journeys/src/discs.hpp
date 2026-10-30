// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace cinematic {
// Evidence: OpenRAC games/rac1/src/game/space.c; port/game/rac1/host/boot.c;
// PAL/NTSC hand/functional flight update and host/rac1_version.h. See docs/EVIDENCE.md.
struct Disc {
    const char* version;
    std::uint32_t pad, ship, loader_busy, scene_done, level_mode;
    const char* flight_update;
};

inline constexpr Disc discs[] =
    {{"pal", 0x13ca40, 0x13e130, 0x15ef4a, 0x15f6fc, 0x15f6a8, "func_0022FDC0"},
     {"ntsc", 0x13c940, 0x13e030, 0x15ee4a, 0x15f618, 0x15f5c4, "FUN_0022eaa8"}};
}  // namespace cinematic

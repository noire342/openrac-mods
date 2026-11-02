// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <openrac/openrac_mod.h>

namespace clank {
// Slot 3 is the entire backpack assembly, including the separate Clank body
// (part 1). Class-based filtering misses that body and upgraded packs.
constexpr int backpack_slot = 3;

inline bool carries_clank(const openrac_held &held) noexcept {
    for (int i = 0; i < std::clamp(held.count, 0, OPENRAC_HELD_MAX); ++i) {
        const auto &piece = held.pieces[i];
        if (piece.slot == backpack_slot && piece.part == 1 && !(piece.flags & OPENRAC_HELD_HIDDEN))
            return true;
    }
    return false;
}

inline openrac_held parked_loadout(const openrac_held &attached) noexcept {
    openrac_held parked{};
    parked.size = sizeof(parked);
    for (int i = 0; i < std::clamp(attached.count, 0, OPENRAC_HELD_MAX); ++i) {
        const auto &piece = attached.pieces[i];
        if (piece.slot != backpack_slot)
            parked.pieces[parked.count++] = piece;
    }
    return parked;
}
} // namespace clank

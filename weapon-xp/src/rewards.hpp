// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <algorithm>

namespace weaponxp {
// Retail's nominal payout before its random +/-25% scatter. Source/group bookkeeping stays native.
inline int nominal_bolts(int base, int budget, int already_collected, int group, int save_id) {
    if (group == 0xFE || save_id < 0) {
        return 0;
    }
    if (group >= 0 && group < 128) {
        if (group >= 64) {
            return 0;
        }
        const int remaining = budget - already_collected;
        if (remaining > (budget + 1) / 2) {
            base = remaining;
        }
        base = std::max(1, base);
    }
    return std::max(0, base);
}

inline constexpr int kMaxKillXp = 500;
}  // namespace weaponxp

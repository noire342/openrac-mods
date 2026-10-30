// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <cstring>

#include "discs.hpp"
#include "enemies.hpp"
#include "openrac/openrac_mod.h"
#include "rewards.hpp"

namespace weaponxp {
class NativeRewards {
public:
    NativeRewards(const openrac_api* api, const Disc& disc) : m_api(api), m_disc(disc) {}

    int bolts(unsigned moby) const {
        if (moby < 0x10000 || moby > 0x2000000 - 256 || !enemy_reward(read<short>(moby + 0xA6))) {
            return -1;
        }
        const bool pal = std::strcmp(m_disc.version, "pal") == 0;
        const auto level = read<int>(m_api->relocate_data(pal ? 0x15EE84 : 0x15ED84));
        const int group = read<unsigned char>(moby + 0xB1);
        int collected = 0;
        if (group < 64) {
            if (level < 0 || level >= 20) {
                return 0;
            }
            const auto stats = m_api->relocate_data(pal ? 0x14D690 : 0x14D590);
            collected = read<short>(stats + (unsigned(level) * 64 + unsigned(group)) * 4 + 2);
        }
        return nominal_bolts(
            read<short>(moby + 0xB4),
            read<short>(moby + 0xB6),
            collected,
            group,
            read<short>(moby + 0xB2)
        );
    }

private:
    const openrac_api* m_api;
    const Disc& m_disc;

    template <class T>
    T read(unsigned at) const {
        T v{};
        if (at >= 0x10000 && at <= 0x2000000 - sizeof v) {
            std::memcpy(&v, m_api->memory + at, sizeof v);
        }
        return v;
    }
};
}  // namespace weaponxp

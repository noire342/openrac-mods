// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cmath>
#include <cstring>

#include "discs.hpp"
#include "openrac/openrac_mod.h"

namespace weaponxp {
// Optional held-model orientation only. The native update/beam uses the original
// basis; its post-update render pose rolls around the local firing axis (row 1).
// Forward stays forward: the model is upside down, never aimed back at Ratchet.
class NativeMorphModel {
public:
    NativeMorphModel(const openrac_api* api, const Disc& disc) : m_api(api), m_disc(disc) {}

    ~NativeMorphModel() { reset(); }

    void reset() noexcept {
        if (m_moby && m_level == m_api->loaded_level() && valid(m_moby, 256)
            && read<short>(m_moby + 0xA6) == 185
            && std::memcmp(m_api->memory + m_moby + 0xC0, m_render.data(), sizeof(m_render)) == 0) {
            std::memcpy(m_api->memory + m_moby + 0xC0, m_original.data(), sizeof(m_original));
        }
        m_moby = 0;
    }

    void after(unsigned moby, bool enabled) noexcept {
        if (!enabled || !valid(moby, 256) || read<short>(moby + 0xA6) != 185) {
            return;
        }
        const auto hero = m_api->relocate_data(m_disc.hero);
        const auto held = m_api->relocate_data(m_disc.item_in_hand);
        if (!valid(hero, 0x1094) || !valid(held, 4) || read<int>(held) != 21
            || read<unsigned>(hero + 0x1090) != moby) {
            return;
        }
        std::memcpy(m_original.data(), m_api->memory + moby + 0xC0, sizeof(m_original));
        for (float f : m_original) {
            if (!std::isfinite(f) || std::abs(f) > 16) {
                return;
            }
        }
        for (unsigned row = 0; row < 3; ++row) {
            float length = 0;
            for (unsigned axis = 0; axis < 3; ++axis) {
                length += m_original[row * 4 + axis] * m_original[row * 4 + axis];
            }
            if (length < .001f || length > 100) {
                return;
            }
        }
        m_render = m_original;
        for (unsigned row : {0u, 2u}) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                m_render[row * 4 + axis] = -m_render[row * 4 + axis];
            }
        }
        std::memcpy(m_api->memory + moby + 0xC0, m_render.data(), sizeof(m_render));
        m_moby = moby;
        m_level = m_api->loaded_level();
    }

private:
    const openrac_api* m_api;
    const Disc& m_disc;
    unsigned m_moby = 0;
    int m_level = -1;
    std::array<float, 12> m_original{}, m_render{};

    static bool valid(unsigned at, unsigned size) {
        return at >= 0x10000 && at <= 0x2000000 - size;
    }

    template <class T>
    T read(unsigned at) const {
        T value{};
        if (valid(at, sizeof(T))) {
            std::memcpy(&value, m_api->memory + at, sizeof(T));
        }
        return value;
    }
};
}  // namespace weaponxp

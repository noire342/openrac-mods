// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cmath>
#include <cstring>

#include "discs.hpp"
#include "openrac/openrac_mod.h"

namespace weaponxp {
// ComputeHitResponse replaces Blaster hit damage with a resident two-entry table.
// Scope its optional multiplier to the pending reaction; restore before the next hit
// query, frame, level load or unload. No projectile/aim/particle changes are involved.
class NativeDamage {
public:
    NativeDamage(const openrac_api* api, const Disc& disc) : m_api(api), m_disc(disc) {}

    ~NativeDamage() { restore(); }

    void prepare(float multiplier) {
        restore();
        if (multiplier == 1.0f) {
            return;
        }
        m_address =
            m_api->relocate_data(std::strcmp(m_disc.version, "pal") == 0 ? 0x1600C8 : 0x15FFC8);
        if (m_address < 0x10000 || m_address > 0x2000000 - 8) {
            m_address = 0;
            return;
        }
        std::memcpy(m_base.data(), m_api->memory + m_address, 8);
        for (float value : m_base) {
            if (!std::isfinite(value) || value <= 0 || value > 1000000) {
                m_address = 0;
                return;
            }
        }
        for (int i = 0; i < 2; ++i) {
            m_scaled[i] = m_base[i] * multiplier;
        }
        std::memcpy(m_api->memory + m_address, m_scaled.data(), 8);
    }

    void restore() {
        if (!m_address) {
            return;
        }
        for (int i = 0; i < 2; ++i) {
            float current;
            std::memcpy(&current, m_api->memory + m_address + i * 4, 4);
            if (current == m_scaled[i]) {
                std::memcpy(m_api->memory + m_address + i * 4, &m_base[i], 4);
            }
        }
        m_address = 0;
    }

private:
    const openrac_api* m_api;
    const Disc& m_disc;
    unsigned m_address = 0;
    std::array<float, 2> m_base{}, m_scaled{};
};
}  // namespace weaponxp

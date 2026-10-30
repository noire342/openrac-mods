// SPDX-License-Identifier: GPL-3.0-or-later
#include "pause_access.hpp"

#include <cstring>
#include <stdexcept>

#include "native_map.hpp"

namespace skillpoints {
std::uint32_t PauseAccess::word(std::uint32_t a) const {
    std::uint32_t v = 0;
    if (a >= 0x10000 && a <= 0x02000000 - 4) {
        std::memcpy(&v, m_api->memory + a, 4);
    }
    return v;
}

void PauseAccess::word(std::uint32_t a, std::uint32_t v) {
    if (a >= 0x10000 && a <= 0x02000000 - 4) {
        std::memcpy(m_api->memory + a, &v, 4);
    }
}

void PauseAccess::change(std::uint32_t a, std::uint32_t v, std::uint32_t hidden) {
    m_changes.push_back({a, word(a), v, hidden});
    word(a, v);
}

void PauseAccess::attach(std::uint32_t goodies, int level) {
    restore();
    if (level < 0 || level >= 19) {
        throw std::runtime_error("Unsupported pause overlay");
    }
    const auto page = word(goodies + 0x38);
    if (!page || word(page + 0x3C) != 2) {
        throw std::runtime_error("Original pause page not found");
    }
    std::uint32_t screens[7]{};
    for (unsigned i = 0; i < 7; ++i) {
        const auto s = word(page + 0x44 + 4 * i), items = word(s + 0x34);
        const auto label = word(items) & 0xffffu;
        if (!s || word(s) != kPointInput[level]
            || m_api->function_at(word(s + 4)) != m_api->function("FUN_0021b1c8")
            || label != (i < 6 ? 0x4EE2u + i : 0x4FC2u) || (word(items) >> 16) != 3
            || (word(items + 12) & 0xffffu) != 0 || (i == 6 && word(items + 4) != goodies)) {
            throw std::runtime_error("Unsupported native pause menu layout");
        }
        screens[i] = s;
    }
    m_page = page;
    m_level = level;
    m_system = m_api->relocate_data(0x001D5BF0);
    // The stock pause initializer rewrites these links on every pause opening.
    // refresh() also reapplies them after that initializer, never touching save data.
    change(screens[0] + 0x38, screens[6], screens[5]);
    change(screens[5] + 0x3C, screens[6], screens[0]);
}

void PauseAccess::refresh() {
    if (!m_page || m_level != m_api->loaded_level()) {
        return;
    }
    const auto current = word(m_system + 4), next = word(m_system + 8);
    const bool show = current == m_page || next == m_page;
    if (show) {
        if (!m_forced) {
            m_original_visibility = word(m_system + 0xD8);
            m_forced = true;
        }
        word(m_system + 0xD8, 1);
        for (auto& c : m_changes) {
            // A new stock initialization may reset links. Respect other replacements.
            if (word(c.address) == c.original || word(c.address) == c.replacement
                || word(c.address) == c.stock_hidden) {
                if (word(c.address) == c.stock_hidden) {
                    c.original = c.stock_hidden;
                }
                word(c.address, c.replacement);
            }
        }
    } else if (m_forced) {
        if (word(m_system + 0xD8) == 1) {
            word(m_system + 0xD8, m_original_visibility);
        }
        m_forced = false;
    }
}

void PauseAccess::restore() {
    if (m_level == m_api->loaded_level()) {
        if (m_forced && word(m_system + 0xD8) == 1) {
            word(m_system + 0xD8, m_original_visibility);
        }
        for (auto it = m_changes.rbegin(); it != m_changes.rend(); ++it) {
            if (word(it->address) == it->replacement) {
                word(it->address, it->original);
            }
        }
    }
    m_changes.clear();
    m_forced = false;
    m_page = 0;
    m_level = -99;
}
}  // namespace skillpoints

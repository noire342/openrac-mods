// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>

#include <openrac/openrac_mod.h>

namespace skillpoints {
// Makes the existing seventh pause panel reachable without changing campaign flags.
class PauseAccess {
public:
    explicit PauseAccess(const openrac_api* api) : m_api(api) {}

    void attach(std::uint32_t goodies, int level);
    void refresh();
    void restore();

private:
    struct Change {
        std::uint32_t address, original, replacement, stock_hidden;
    };

    std::uint32_t word(std::uint32_t) const;
    void word(std::uint32_t, std::uint32_t);
    void change(std::uint32_t, std::uint32_t, std::uint32_t);
    const openrac_api* m_api;
    std::vector<Change> m_changes;
    std::uint32_t m_page = 0, m_system = 0, m_original_visibility = 0;
    int m_level = -99;
    bool m_forced = false;
};
}  // namespace skillpoints

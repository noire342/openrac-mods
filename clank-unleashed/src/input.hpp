// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller.hpp"
#include <cstddef>
namespace clank {
class Input final {
  public:
    void poll(int port, std::uint8_t *report, int bytes, bool detached, bool nearby) noexcept {
        if (port != 0 || !report || bytes < 2)
            return;
        constexpr std::uint16_t l3 = 2, triangle = 0x1000;
        const auto held = static_cast<std::uint16_t>(~(report[0] | (report[1] << 8)));
        const bool combination = (held & (l3 | triangle)) == (l3 | triangle);
        if (!detached && combination && !m_combination)
            m_pending.detach = true;
        if (detached && nearby && (held & triangle) && !(m_held & triangle))
            m_pending.join = true;
        if ((!detached && combination) || (detached && nearby && (held & triangle))) {
            report[1] |= 0x10; // active-low Triangle
            if (bytes > 10)
                report[10] = 0; // Triangle pressure
        }
        m_combination = combination;
        m_held = held;
    }
    Commands take() noexcept {
        const auto out = m_pending;
        m_pending = {};
        return out;
    }

  private:
    bool m_combination{};
    std::uint16_t m_held{};
    Commands m_pending{};
};
} // namespace clank

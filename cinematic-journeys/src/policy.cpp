// SPDX-License-Identifier: GPL-3.0-or-later
#include "policy.hpp"

#include <algorithm>
#include <cmath>

namespace cinematic {
bool skip_chord(std::uint16_t buttons) noexcept {
    return (buttons & (start | shoulders)) == 0;
}

bool protected_scene(int level, int mode, int ship_state, bool flight_busy) noexcept {
    if (level >= 0 && level < 19 && (mode == 2 || mode == 6)) {
        return true;
    }
    return flight_busy && (ship_state == 3 || ship_state == 4 || ship_state == 7);
}

std::uint16_t filter_buttons(std::uint16_t buttons, bool protect, bool ship_scene) noexcept {
    if (!protect || skip_chord(buttons)) {
        return buttons;
    }
    return static_cast<std::uint16_t>(buttons | start | (ship_scene ? scene_faces : 0));
}

void write_buttons(std::uint8_t* report, std::size_t size, std::uint16_t buttons) noexcept {
    if (!report || size < 2) {
        return;
    }
    const auto previous = static_cast<std::uint16_t>(report[0] | (report[1] << 8));
    report[0] = static_cast<std::uint8_t>(buttons);
    report[1] = static_cast<std::uint8_t>(buttons >> 8);
    constexpr unsigned bits[] = {5, 7, 4, 6, 12, 13, 14, 15, 10, 11, 8, 9};
    // Only synchronize buttons this mod changed; preserve partial pressure on other buttons.
    for (std::size_t i = 0; i < 12 && 6 + i < size; ++i) {
        const auto bit = 1u << bits[i];
        if ((previous ^ buttons) & bit) {
            report[6 + i] = buttons & bit ? 0 : 255;
        }
    }
}

double valid_seconds(double seconds) noexcept {
    return std::isfinite(seconds) ? std::clamp(seconds, 0.0, 30.0) : 5.0;
}

FlightMinimum::FlightMinimum(double seconds) noexcept : m_seconds(valid_seconds(seconds)) {}

bool FlightMinimum::hold(bool active, double now) noexcept {
    if (!active || !std::isfinite(now)) {
        m_active = false;
        return false;
    }
    if (!m_active) {
        m_started = now;
        m_active = true;
    }
    return now - m_started < m_seconds;
}
}  // namespace cinematic

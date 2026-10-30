// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace cinematic {
constexpr std::uint16_t start = 0x0008, shoulders = 0x0f00, scene_faces = 0x7000;

// Pad bits are active low. Kept independent of the SDK and guest memory.
bool skip_chord(std::uint16_t buttons) noexcept;
bool protected_scene(int level, int mode, int ship_state, bool flight_busy) noexcept;
std::uint16_t filter_buttons(std::uint16_t buttons, bool protect, bool ship_scene) noexcept;
void write_buttons(std::uint8_t* report, std::size_t size, std::uint16_t buttons) noexcept;
double valid_seconds(double seconds) noexcept;

class FlightMinimum {
public:
    explicit FlightMinimum(double seconds) noexcept;
    // One observation per pad poll. A clock value is supplied by the adapter, never read here.
    bool hold(bool active, double now_seconds) noexcept;

private:
    double m_seconds, m_started = 0;
    bool m_active = false;
};
}  // namespace cinematic

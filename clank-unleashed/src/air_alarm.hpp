// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace clank {
struct AirAlarm {
    int frame{};
    bool warning{};
};
// The original oxygen draw's 1/3 threshold and scaled 75% flash duty cycle.
inline AirAlarm air_alarm(int value, int frame, int ticks, float scale_sq) noexcept {
    if (value < 0 || value > 3333 || ticks <= 0 || ticks > 1000 || !std::isfinite(scale_sq))
        return {};
    const int period = ticks * 60;
    const int duty = static_cast<int>(std::clamp(scale_sq, 0.0f, 100.0f) * 0.75f * 60);
    const bool previous = (frame - 1) % period < duty, current = frame % period < duty;
    return {current ? 1 : 0, !previous && current};
}
} // namespace clank

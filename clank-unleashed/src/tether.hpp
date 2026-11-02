// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace clank {
struct TetherConfig {
    bool enabled{};
    float distance = 25.0f;
    float seconds{}; // 0 selects Ratchet's native per-tick oxygen loss.
};
struct AirStatus {
    bool visible{}, exhausted{};
    float remaining = 1.0f;
};
// Pure game-time policy. Native HUD, character state and settings stay outside.
class Tether final {
  public:
    explicit Tether(TetherConfig config = {}) : m_config(config) {
        if (!std::isfinite(config.distance) || config.distance < 5 || config.distance > 100 ||
            !std::isfinite(config.seconds) || config.seconds < 0 || config.seconds > 120)
            throw std::invalid_argument("Invalid Clank distance/oxygen settings");
    }
    AirStatus update(bool detached, bool playing, float distance, float seconds,
                     float native_loss = 0) noexcept {
        if (!m_config.enabled || !detached) {
            reset();
            return {};
        }
        if (!std::isfinite(distance) || distance < 0)
            return {m_outside || m_remaining < 1, false, static_cast<float>(m_remaining)};
        if (playing && std::isfinite(seconds) && seconds > 0) {
            // One unit of hysteresis avoids flicker at the radius boundary.
            if (distance > m_config.distance)
                m_outside = true;
            else if (distance <= m_config.distance - 1)
                m_outside = false;
            const float dt = std::min(seconds, 0.1f);
            if (m_outside) {
                const double loss = m_config.seconds != 0 ? dt / m_config.seconds
                                    : std::isfinite(native_loss)
                                        ? std::clamp(native_loss, 0.0f, 1.0f)
                                        : 0;
                m_remaining = std::max(0.0, m_remaining - loss);
            } else
                m_remaining = std::min(1.0, m_remaining + dt / 3.0);
        }
        return {m_outside || m_remaining < 1, m_remaining <= 0, static_cast<float>(m_remaining)};
    }
    void reset() noexcept {
        m_remaining = 1;
        m_outside = false;
    }

  private:
    TetherConfig m_config;
    double m_remaining = 1;
    bool m_outside{};
};
} // namespace clank

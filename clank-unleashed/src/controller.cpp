// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller.hpp"
#include <cmath>
#include <stdexcept>
namespace clank {
Controller::Controller(IWorld &world, float radius) : m_world(world), m_radius(radius) {
    if (!std::isfinite(radius) || radius < 0.5f || radius > 5.0f)
        throw std::invalid_argument("Invalid Clank reattachment radius");
}
bool Controller::near_ratchet() const {
    if (m_phase != Phase::Detached)
        return false;
    const auto s = m_world.observe();
    const float dx = s.position.x - m_anchor.x, dy = s.position.y - m_anchor.y,
                dz = s.position.z - m_anchor.z;
    return s.epoch == m_epoch && s.playing && s.alive && std::isfinite(dx) && std::isfinite(dy) &&
           std::isfinite(dz) && dx * dx + dy * dy + dz * dz <= m_radius * m_radius;
}
void Controller::restore(ReturnReason reason) {
    if (m_phase == Phase::Detached && m_world.restore(reason))
        m_phase = Phase::Attached;
}
void Controller::tick(Commands commands) {
    const auto state = m_world.observe();
    if (m_phase == Phase::Detached) {
        if (state.epoch != m_epoch) {
            // The level already retired every actor. Never follow an old pointer.
            m_world.expire();
            m_phase = Phase::Attached;
            return;
        }
        if (!state.alive) {
            restore(ReturnReason::Death);
            return;
        }
        if (!state.playing && !state.menu) {
            restore(ReturnReason::Scene);
            return;
        }
        if (commands.join && near_ratchet()) {
            restore(ReturnReason::Nearby);
            return;
        }
        m_world.maintain();
        return;
    }
    if (commands.detach && state.playing && state.alive && state.ratchet && state.carrying_clank &&
        state.grounded && m_world.detach()) {
        m_anchor = state.position;
        m_epoch = state.epoch;
        m_phase = Phase::Detached;
    }
}
void Controller::before_level_change() { restore(ReturnReason::LevelChange); }
void Controller::stop() { restore(ReturnReason::Shutdown); }
void Controller::out_of_air() { restore(ReturnReason::Death); }
} // namespace clank

// SPDX-License-Identifier: GPL-3.0-or-later
#include "combat.hpp"

#include <cmath>

#include "rewards.hpp"

namespace weaponxp {

float Combat::hit(
    std::uint32_t target, int item, float damage, std::uint64_t tick, bool secondary
) {
    if (weapon_index(item) < 0 || !std::isfinite(damage) || damage <= 0 || damage > 1000000) {
        return damage;
    }
    if (m_targets.size() < 4096 || m_targets.count(target)) {
        auto found = m_targets.find(target);
        if (found == m_targets.end()) {
            m_targets.emplace(target, Target{item, tick, false, secondary});
        } else if (!found->second.credited) {
            found->second = {item, tick, false, secondary};
        }
    }
    return m_settings.scale_damage
               ? damage * m_state.rule(item).damage[m_state.weapon_level(item) - 1]
               : damage;
}

Award Combat::death(std::uint32_t target, int reward, std::uint64_t tick) {
    const auto found = m_targets.find(target);
    if (reward < 0 || found == m_targets.end() || found->second.credited
        || tick - found->second.tick > 120) {
        return {};
    }
    auto& source = found->second;
    source.credited = true;
    const auto before = m_state.experience(source.item);
    const auto index = weapon_index(source.item);
    const auto fraction = m_state.quarters[index];
    const int old_level = m_state.weapon_level(source.item);
    const auto amount = reward_quarters(reward, m_settings, m_state.rule(source.item));
    const int new_level = m_state.award_quarters(source.item, amount);
    return {
        before != m_state.experience(source.item) || fraction != m_state.quarters[index],
        source.item,
        old_level,
        new_level,
        source.secondary
    };
}

void Combat::expire(std::uint64_t tick) {
    for (auto it = m_targets.begin(); it != m_targets.end();) {
        if (tick - it->second.tick > 120) {
            it = m_targets.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace weaponxp

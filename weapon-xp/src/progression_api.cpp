// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <algorithm>
#include <cmath>
#include <iterator>

#include "native_rewards.hpp"
#include "runtime.hpp"

namespace weaponxp {
namespace {
weapon_xp_progress_v5 snapshot(const WeaponRules& rules, const Settings& settings,
                               std::uint32_t xp, std::uint8_t fraction, int previous_level) {
    const auto current = level(xp, rules);
    const auto start = rules.thresholds[current - 1];
    const auto next = rules.thresholds[std::min(current, kMaxLevel - 1)];
    const double fractional_xp = double(fraction) / kQuartersPerXp;
    const auto progress = current == kMaxLevel ? 1.0f
        : std::min(float((double(xp - start) + fractional_xp) / (next - start)),
                   std::nextafter(1.0f, 0.0f));
    return {
        sizeof(weapon_xp_progress_v5), std::uint32_t(previous_level), std::uint32_t(current),
        xp, fraction, start, next, progress,
        settings.scale_damage ? rules.damage[current - 1] : 1.0f,
        settings.scale_capacity ? rules.ammo[current - 1] : 1.0f
    };
}
}

int Runtime::get_progress(int item, weapon_xp_progress_v5* out) const {
    const int index = weapon_index(item);
    if (!out || out->size < sizeof(*out) || index < 0) return WEAPON_XP_INVALID_ARGUMENT;
    *out = snapshot(m_state.rules[index], m_settings, m_state.xp[index], m_state.quarters[index],
                    m_state.weapon_level(item));
    return WEAPON_XP_OK;
}

int Runtime::get_native_reward(std::uint32_t moby, std::uint32_t* nominal_bolts) {
    if (!nominal_bolts) return WEAPON_XP_INVALID_ARGUMENT;
    if (!gameplay_active()) return WEAPON_XP_NOT_READY;
    const auto reward = NativeRewards(m_api, *m_disc).bolts(moby);
    if (reward < 0) return WEAPON_XP_NOT_FOUND;
    *nominal_bolts = std::uint32_t(reward);
    return WEAPON_XP_OK;
}

int Runtime::evaluate_progress(const weapon_xp_rules_v3* custom, const weapon_xp_balance_v5* balance,
                               std::uint32_t nominal_bolts, weapon_xp_progress_v5* out) const {
    if (!balance || balance->size < sizeof(*balance) || balance->xp > kMaxExperience ||
        balance->xp_quarters >= kQuartersPerXp ||
        (balance->xp == kMaxExperience && balance->xp_quarters) ||
        !out || out->size < sizeof(*out)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }

    WeaponRules rules;
    if (custom) {
        if (custom->size < sizeof(*custom) || custom->level_count != rules.thresholds.size()) {
            return WEAPON_XP_INVALID_ARGUMENT;
        }
        std::copy(std::begin(custom->thresholds), std::end(custom->thresholds), rules.thresholds.begin());
        std::copy(std::begin(custom->damage), std::end(custom->damage), rules.damage.begin());
        std::copy(std::begin(custom->ammo), std::end(custom->ammo), rules.ammo.begin());
        rules.xp_multiplier = custom->xp_multiplier;
        std::string error;
        if (!validate(rules,error)) return WEAPON_XP_INVALID_ARGUMENT;
    }

    auto xp = balance->xp;
    auto fraction = static_cast<std::uint8_t>(balance->xp_quarters);
    const int previous_level = level(xp, rules);
    const auto capped_bolts = static_cast<int>(std::min(nominal_bolts, m_settings.kill_xp_cap));
    add_quarters(xp, fraction, reward_quarters(capped_bolts, m_settings, rules),
                 rules.thresholds.back());
    *out = snapshot(rules, m_settings, xp, fraction, previous_level);
    return WEAPON_XP_OK;
}
}  // namespace weaponxp

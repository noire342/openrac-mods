// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <algorithm>
#include <limits>
#include <set>

#include "rules_file.hpp"
#include "runtime.hpp"

namespace weaponxp {
static_assert(kMaxLevel == WEAPON_XP_LEVEL_COUNT);
static_assert(std::tuple_size<Rules>::value == kWeapons.size());

bool Runtime::configuration_busy() const {
    // A network session keeps a stable ruleset for prepared/confirmed receipts.
    return m_events.dispatching() || m_filters.dispatching() || m_external_processing || m_external_scope
           || m_upgrades->pending();
}

int Runtime::configuration_revision(std::uint64_t expected) const {
    if (configuration_busy()) {
        return WEAPON_XP_BUSY;
    }
    if (expected != m_revision) {
        return WEAPON_XP_STALE_REVISION;
    }
    return m_revision == std::numeric_limits<std::uint64_t>::max() ? WEAPON_XP_FULL : WEAPON_XP_OK;
}

int Runtime::get_configuration(weapon_xp_configuration_v3* out) {
    if (!out || out->size < sizeof(*out)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    out->revision = m_revision;
    out->xp_rate = m_settings.xp_rate;
    out->level_count = kMaxLevel;
    out->kill_xp_cap = m_settings.kill_xp_cap;
    out->flags = (m_settings.scale_damage ? WEAPON_XP_SCALE_DAMAGE : 0)
                 | (m_settings.scale_capacity ? WEAPON_XP_SCALE_CAPACITY : 0)
                 | (m_settings.hud ? WEAPON_XP_SHOW_HUD : 0)
                 | (m_settings.reduced_motion ? WEAPON_XP_REDUCED_MOTION : 0)
                 | (m_settings.animations ? WEAPON_XP_LEVEL_UP_ANIMATION : 0)
                 | (m_settings.versioned_names ? WEAPON_XP_VERSIONED_NAMES : 0)
                 | (m_settings.flip_morph_model ? WEAPON_XP_FLIP_MORPH_MODEL : 0);
    return WEAPON_XP_OK;
}

int Runtime::set_configuration(const weapon_xp_configuration_v3* in, std::uint64_t expected) {
    const int ready = configuration_revision(expected);
    if (ready) {
        return ready;
    }
    if (!in || in->size < sizeof(*in) || in->level_count != kMaxLevel || (in->flags & ~127u)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    Settings next = m_settings;
    next.xp_rate = in->xp_rate;
    next.kill_xp_cap = in->kill_xp_cap;
    next.scale_damage = (in->flags & WEAPON_XP_SCALE_DAMAGE) != 0;
    next.scale_capacity = (in->flags & WEAPON_XP_SCALE_CAPACITY) != 0;
    next.hud = (in->flags & WEAPON_XP_SHOW_HUD) != 0;
    next.reduced_motion = (in->flags & WEAPON_XP_REDUCED_MOTION) != 0;
    next.animations = (in->flags & WEAPON_XP_LEVEL_UP_ANIMATION) != 0;
    next.versioned_names = (in->flags & WEAPON_XP_VERSIONED_NAMES) != 0;
    next.flip_morph_model = (in->flags & WEAPON_XP_FLIP_MORPH_MODEL) != 0;
    if (!validate(next, m_rules_error)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    m_morph_model->reset();
    restore_hits();
    restore_capacity();
    m_table = 0;
    m_settings = next;
    configuration_changed();
    return WEAPON_XP_OK;
}

int Runtime::get_rules(int item, weapon_xp_rules_v3* out) {
    if (!out || out->size < sizeof(*out) || weapon_index(item) < 0) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    const auto& r = m_state.rule(item);
    out->item_id = item;
    out->level_count = kMaxLevel;
    out->xp_multiplier = r.xp_multiplier;
    std::copy(r.thresholds.begin(), r.thresholds.end(), out->thresholds);
    std::copy(r.damage.begin(), r.damage.end(), out->damage);
    std::copy(r.ammo.begin(), r.ammo.end(), out->ammo);
    return WEAPON_XP_OK;
}

int Runtime::set_rules(
    const weapon_xp_rules_v3* entries, std::uint32_t count, std::uint64_t expected
) {
    const int ready = configuration_revision(expected);
    if (ready) {
        return ready;
    }
    if (!entries || !count || count > kWeapons.size()) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    Rules next = m_state.rules;
    std::set<int> seen;
    for (unsigned i = 0; i < count; ++i) {
        const auto& e = entries[i];
        const int index = weapon_index(e.item_id);
        if (e.size < sizeof(e) || e.level_count != kMaxLevel || index < 0
            || !seen.insert(index).second) {
            return WEAPON_XP_INVALID_ARGUMENT;
        }
        auto& r = next[index];
        r.xp_multiplier = e.xp_multiplier;
        std::copy(std::begin(e.thresholds), std::end(e.thresholds), r.thresholds.begin());
        std::copy(std::begin(e.damage), std::end(e.damage), r.damage.begin());
        std::copy(std::begin(e.ammo), std::end(e.ammo), r.ammo.begin());
        if (!validate(r, m_rules_error)) {
            return WEAPON_XP_INVALID_ARGUMENT;
        }
    }
    m_morph_model->reset();
    restore_hits();
    restore_capacity();
    m_table = 0;
    m_state.rules = next;
    configuration_changed();
    return WEAPON_XP_OK;
}

int Runtime::reload_rules(std::uint64_t expected) {
    const int ready = configuration_revision(expected);
    if (ready) {
        return ready;
    }
    Rules next;
    if (!read_rules_file(m_rules_path, next, m_rules_error, false)) {
        return WEAPON_XP_IO_ERROR;
    }
    m_morph_model->reset();
    restore_hits();
    restore_capacity();
    m_table = 0;
    m_state.rules = next;
    configuration_changed();
    return WEAPON_XP_OK;
}

void Runtime::configuration_changed() {
    ++m_revision;
    m_rules_error.clear();
    if (!m_settings.hud) {
        m_hud_bridge->detach();
    }
    m_ui.enabled = m_settings.hud;
    m_ui.reduced_motion = m_settings.reduced_motion;
    weapon_xp_event_v1 event{};
    event.size = sizeof(event);
    event.kind = WEAPON_XP_CONFIGURATION_CHANGED;
    event.item_id = -1;
    m_events.emit(event);
}
}  // namespace weaponxp

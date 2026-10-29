// SPDX-License-Identifier: GPL-3.0-or-later
#include <cmath>
#include <cstring>

#include "enemies.hpp"
#include "native_rewards.hpp"
#include "runtime.hpp"

namespace weaponxp {

void Runtime::accept_award(const Award& award) {
    if (!award.changed) {
        return;
    }
    m_dirty = true;
    weapon_xp_event_v1 event{};
    event.size = sizeof event;
    event.kind = WEAPON_XP_CHANGED;
    event.item_id = award.item;
    event.old_level = award.old_level;
    event.new_level = award.new_level;
    event.xp = m_state.experience(award.item);
    m_events.emit(event);
    if (award.new_level > award.old_level) {
        if (m_settings.animations) {
            m_upgrades->enqueue(award.item, award.old_level, award.new_level);
        }
        m_ui.level_up(award.item, award.new_level);
        event.kind = WEAPON_XP_LEVEL_UP;
        m_events.emit(event);
    }
}

int Runtime::get_stats(int item, weapon_xp_stats_v1* out) {
    if (!out || out->size < sizeof(weapon_xp_stats_v1) || weapon_index(item) < 0) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    const int current = m_state.weapon_level(item);
    out->item_id = item;
    out->name = kWeapons[weapon_index(item)].name;
    out->level = current;
    out->xp = m_state.experience(item);
    out->level_start_xp = m_state.rule(item).thresholds[current - 1];
    out->next_level_xp =
        m_state.rule(item).thresholds[current == kMaxLevel ? kMaxLevel - 1 : current];
    out->damage_multiplier =
        m_settings.scale_damage ? m_state.rule(item).damage[current - 1] : 1.0f;
    out->ammo_multiplier = m_settings.scale_capacity ? m_state.rule(item).ammo[current - 1] : 1.0f;
    return WEAPON_XP_OK;
}

int Runtime::award_xp(int item, std::uint32_t amount) {
    if (m_events.dispatching()) {
        return WEAPON_XP_BUSY;
    }
    if (weapon_index(item) < 0) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    if (!m_state.rule(item).xp_multiplier) {
        return WEAPON_XP_DISABLED;
    }
    const auto old_xp = m_state.experience(item);
    const int before = m_state.weapon_level(item);
    const int after = m_state.award(item, amount);
    accept_award({m_state.experience(item) != old_xp, item, before, after});
    return WEAPON_XP_OK;
}

bool Runtime::same_key(const ExternalKey& a, const weapon_xp_external_hit_v2& b) {
    return a.session == b.session_id && a.event == b.event_id && a.target == b.target_id
           && a.epoch == b.epoch;
}

bool Runtime::external_context(const weapon_xp_external_hit_v2* c, bool require_ring) {
    if (!c || c->size < sizeof(*c) || !m_external_scope || c->session_id != m_external_scope
        || !c->event_id || !c->target_id || !c->epoch || c->target_class < 0
        || !enemy_reward(c->target_class)) {
        return false;
    }
    if (!require_ring) {
        return true;
    }
    const auto ring = overlay(m_disc->hit_ring);
    return valid(c->target_moby, 0x100) && read<short>(c->target_moby + 0xA6) == c->target_class
           && c->hit_address >= ring && (c->hit_address - ring) % 64 == 0
           && (c->hit_address - ring) / 64 < 64 && valid(c->hit_address, 64)
           && read<unsigned>(c->hit_address + 0x34) == c->target_moby
           && read<unsigned char>(c->target_moby + 0xA4) == (c->hit_address - ring) / 64;
}

int Runtime::begin_external_scope(std::uint64_t session) {
    if (m_events.dispatching() || m_external_processing) {
        return WEAPON_XP_BUSY;
    }
    if (session == m_external_scope) {
        return WEAPON_XP_OK;
    }
    m_external_scope = session;
    m_external_highest = 0;
    m_external_cursor = 0;
    for (auto& entry : m_external_prepared) {
        entry = ExternalPrepared{};
    }
    m_external_replicated = {};
    return WEAPON_XP_OK;
}

int Runtime::prepare_local_hit(const weapon_xp_external_hit_v2* c, weapon_xp_prepared_hit_v2* out) {
    if (m_events.dispatching() || m_external_processing) {
        return WEAPON_XP_BUSY;
    }
    if (!out || out->size < sizeof(*out) || !external_context(c, false)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    const auto output = [&](const ExternalPrepared& p) {
        out->item_id = p.item;
        out->damage = p.damage;
        out->flags = p.secondary ? WEAPON_XP_SECONDARY_HIT : 0;
    };
    for (const auto& p : m_external_prepared) {
        if (p.key.event == c->event_id && p.key.session == c->session_id) {
            if (!same_key(p.key, *c) || p.moby != c->target_moby || p.hit != c->hit_address
                || p.target_class != c->target_class) {
                return WEAPON_XP_INVALID_ARGUMENT;
            }
            if (valid(p.hit, 64) && std::memcmp(p.before.data(), m_api->memory + p.hit, 64) != 0
                && std::memcmp(p.after.data(), m_api->memory + p.hit, 64) != 0) {
                return WEAPON_XP_INVALID_ARGUMENT;
            }
            output(p);
            return WEAPON_XP_OK;
        }
    }
    if (c->event_id <= m_external_highest) {
        return WEAPON_XP_NOT_FOUND;  // Evicted events cannot be replayed as fresh hits.
    }
    if (!playing() || !external_context(c, true)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    const auto ring = overlay(m_disc->hit_ring);
    const auto& replicated = m_external_replicated[(c->hit_address - ring) / 64];
    if (replicated.key.session == m_external_scope && replicated.moby == c->target_moby
        && replicated.hit == c->hit_address
        && std::memcmp(replicated.payload.data(), m_api->memory + c->hit_address, 64) == 0) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    auto& p = m_external_prepared[m_external_cursor % m_external_prepared.size()];
    if (p.key.event && !p.confirmed && m_tick - p.at <= 600) {
        return WEAPON_XP_FULL;
    }
    const int item = source_item(c->hit_address);
    const float damage = read<float>(c->hit_address + 0x2C);
    if (weapon_index(item) < 0 || !std::isfinite(damage) || damage <= 0 || damage > 1000000) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }

    struct Processing {
        bool& value;

        explicit Processing(bool& v) : value(v) { value = true; }

        ~Processing() { value = false; }
    } processing(m_external_processing);

    p = ExternalPrepared{};
    p.key = {c->session_id, c->event_id, c->target_id, c->epoch};
    p.moby = c->target_moby;
    p.hit = c->hit_address;
    p.target_class = c->target_class;
    p.reward = NativeRewards(m_api, *m_disc).bolts(p.moby);
    p.item = item;
    p.secondary = false;
    p.at = m_tick;
    std::memcpy(p.before.data(), m_api->memory + p.hit, 64);
    if (!process_hit(p.moby, p.hit)) {
        p = ExternalPrepared{};
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    m_combat.forget(p.moby);  // Replayed native death must not award before confirmation.
    p.damage = read<float>(p.hit + 0x2C);
    std::memcpy(p.after.data(), m_api->memory + p.hit, 64);
    m_external_highest = c->event_id;
    ++m_external_cursor;
    output(p);
    return WEAPON_XP_OK;
}

int Runtime::mark_replicated_hit(const weapon_xp_external_hit_v2* c) {
    if (m_events.dispatching() || m_external_processing) {
        return WEAPON_XP_BUSY;
    }
    if (!external_context(c, true)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    auto& p = m_external_replicated[(c->hit_address - overlay(m_disc->hit_ring)) / 64];
    p.key = {c->session_id, c->event_id, c->target_id, c->epoch};
    p.moby = c->target_moby;
    p.hit = c->hit_address;
    p.at = m_tick;
    std::memcpy(p.payload.data(), m_api->memory + p.hit, 64);
    m_edits[(c->hit_address - overlay(m_disc->hit_ring)) / 64] = {};
    m_combat.forget(p.moby);
    return WEAPON_XP_OK;
}

int Runtime::confirm_external_death(const weapon_xp_external_hit_v2* c) {
    if (m_events.dispatching() || m_external_processing) {
        return WEAPON_XP_BUSY;
    }
    if (!external_context(c, false)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    for (auto& p : m_external_prepared) {
        if (!same_key(p.key, *c)) {
            continue;
        }
        if (p.target_class != c->target_class) {
            return WEAPON_XP_INVALID_ARGUMENT;
        }
        if (p.confirmed) {
            return WEAPON_XP_OK;
        }
        if (m_tick - p.at > 600) {
            return WEAPON_XP_NOT_FOUND;
        }
        const auto before = m_state.experience(p.item);
        const auto index = weapon_index(p.item);
        const auto fraction = m_state.quarters[index];
        const int old_level = m_state.weapon_level(p.item);
        const auto amount = reward_quarters(p.reward, m_settings, m_state.rule(p.item));
        const int new_level = m_state.award_quarters(p.item, amount);
        p.confirmed = true;  // Set before callbacks; confirmation is idempotent.
        accept_award(
            {before != m_state.experience(p.item) || fraction != m_state.quarters[index],
             p.item,
             old_level,
             new_level,
             p.secondary}
        );
        return WEAPON_XP_OK;
    }
    return WEAPON_XP_NOT_FOUND;
}

const weapon_xp_api_v1* Runtime::query(std::uint32_t abi) {
    if (abi != WEAPON_XP_ABI_V1 && abi != WEAPON_XP_ABI_V2 && abi != WEAPON_XP_ABI_V3) {
        return nullptr;
    }
    static const weapon_xp_api_v1 service{
        sizeof(weapon_xp_api_v1),
        WEAPON_XP_ABI_V1,
        +[]() -> std::uint32_t { return static_cast<std::uint32_t>(kWeapons.size()); },
        +[](std::uint32_t index) -> std::int32_t {
            return index < kWeapons.size() ? kWeapons[index].item : -1;
        },
        +[](std::int32_t item, weapon_xp_stats_v1* out) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->get_stats(item, out)
                                                     : WEAPON_XP_NOT_READY;
        },
        +[](std::int32_t item, std::uint32_t amount) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->award_xp(item, amount)
                                                     : WEAPON_XP_NOT_READY;
        },
        +[](weapon_xp_listener_v1 fn, void* user, std::uint32_t* token) -> std::int32_t {
            return s_instance && s_instance->m_ready
                       ? s_instance->m_events.subscribe(fn, user, token)
                       : WEAPON_XP_NOT_READY;
        },
        +[](std::uint32_t token) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->m_events.unsubscribe(token)
                                                     : WEAPON_XP_NOT_READY;
        }
    };
    if (abi == WEAPON_XP_ABI_V1) {
        return &service;
    }
    static const weapon_xp_api_v2 external{
        {sizeof(weapon_xp_api_v2),
         WEAPON_XP_ABI_V2,
         service.weapon_count,
         service.weapon_id_at,
         service.get_stats,
         service.award_xp,
         service.subscribe,
         service.unsubscribe},
        +[](std::uint64_t session) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->begin_external_scope(session)
                                                     : WEAPON_XP_NOT_READY;
        },
        +[](const weapon_xp_external_hit_v2* c, weapon_xp_prepared_hit_v2* out) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->prepare_local_hit(c, out)
                                                     : WEAPON_XP_NOT_READY;
        },
        +[](const weapon_xp_external_hit_v2* c) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->mark_replicated_hit(c)
                                                     : WEAPON_XP_NOT_READY;
        },
        +[](const weapon_xp_external_hit_v2* c) -> std::int32_t {
            return s_instance && s_instance->m_ready ? s_instance->confirm_external_death(c)
                                                     : WEAPON_XP_NOT_READY;
        }
    };
    if (abi == WEAPON_XP_ABI_V2) {
        return &external.base;
    }
    static const weapon_xp_api_v3 extension{
        external,
        +[](weapon_xp_configuration_v3* out) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            try {
                return s_instance->get_configuration(out);
            } catch (...) {
                return WEAPON_XP_IO_ERROR;
            }
        },
        +[](std::int32_t item, weapon_xp_rules_v3* out) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            try {
                return s_instance->get_rules(item, out);
            } catch (...) {
                return WEAPON_XP_IO_ERROR;
            }
        },
        +[](const weapon_xp_configuration_v3* in, std::uint64_t revision) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            try {
                return s_instance->set_configuration(in, revision);
            } catch (...) {
                return WEAPON_XP_IO_ERROR;
            }
        },
        +[](const weapon_xp_rules_v3* in,
            std::uint32_t count,
            std::uint64_t revision) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            try {
                return s_instance->set_rules(in, count, revision);
            } catch (...) {
                return WEAPON_XP_IO_ERROR;
            }
        },
        +[](std::uint64_t revision) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            try {
                return s_instance->reload_rules(revision);
            } catch (...) {
                return WEAPON_XP_IO_ERROR;
            }
        },
        +[]() -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            if (s_instance->m_events.dispatching() || s_instance->m_external_processing) {
                return WEAPON_XP_BUSY;
            }
            s_instance->save();
            return s_instance->m_dirty ? WEAPON_XP_IO_ERROR : WEAPON_XP_OK;
        },
        +[](char* buffer, std::uint32_t capacity) -> std::int32_t {
            if (!s_instance || !s_instance->m_ready) {
                return WEAPON_XP_NOT_READY;
            }
            if (!buffer || !capacity) {
                return WEAPON_XP_INVALID_ARGUMENT;
            }
            const auto& text = s_instance->m_rules_error;
            const auto length = std::min<std::size_t>(text.size(), capacity - 1);
            std::memcpy(buffer, text.data(), length);
            buffer[length] = 0;
            return static_cast<std::int32_t>(text.size() + 1);
        }
    };
    static const weapon_xp_api_v3 versioned = [] {
        auto table = extension;
        table.v2.base.size = sizeof(weapon_xp_api_v3);
        table.v2.base.abi_version = WEAPON_XP_ABI_V3;
        return table;
    }();
    return &versioned.v2.base;
}

}  // namespace weaponxp

extern "C" OPENRAC_MOD_EXPORT const weapon_xp_api_v1* openrac_weapon_xp_query(std::uint32_t abi) {
    return weaponxp::Runtime::query(abi);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "events.hpp"
#include "progression.hpp"

namespace weaponxp {
namespace {
constexpr auto kKnownEvents =
    WEAPON_XP_EVENT_MASK(WEAPON_XP_HIT) | WEAPON_XP_EVENT_MASK(WEAPON_XP_CHANGED) |
    WEAPON_XP_EVENT_MASK(WEAPON_XP_LEVEL_UP) | WEAPON_XP_EVENT_MASK(WEAPON_XP_LEVEL_LOADED) |
    WEAPON_XP_EVENT_MASK(WEAPON_XP_STOPPING) | WEAPON_XP_EVENT_MASK(WEAPON_XP_CONFIGURATION_CHANGED);
}

int Events::subscribe(weapon_xp_listener_v1 fn, void* user, std::uint32_t* token) {
    return subscribe(m_listeners, {fn, user}, token);
}

int Events::subscribe_filtered(std::uint32_t mask, int item, weapon_xp_listener_v1 fn,
                               void* user, std::uint32_t* token) {
    if (!mask || (mask & ~kKnownEvents) ||
        (item != WEAPON_XP_ANY_ITEM && item != WEAPON_XP_SESSION_ITEM && weapon_index(item) < 0)) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    return subscribe(m_filtered, {fn, user, 0, mask, item}, token);
}

int Events::subscribe(Listeners& bank, Listener registration, std::uint32_t* token) {
    if (!registration.fn || !token) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    if (m_dispatching) {
        return WEAPON_XP_BUSY;
    }
    for (auto& listener : bank) {
        if (!listener.fn) {
            if (m_next == 0) {
                return WEAPON_XP_FULL;
            }
            registration.token = m_next++;
            listener = registration;
            *token = listener.token;
            return WEAPON_XP_OK;
        }
    }
    return WEAPON_XP_FULL;
}

int Events::unsubscribe(std::uint32_t token) {
    if (!token) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    for (auto* bank : {&m_listeners, &m_filtered}) {
        for (auto& listener : *bank) {
            if (listener.token == token) {
                listener = {};
                return WEAPON_XP_OK;
            }
        }
    }
    return WEAPON_XP_NOT_FOUND;
}

void Events::emit(const weapon_xp_event_v1& event) noexcept {
    if (m_dispatching) {
        return;
    }
    m_dispatching = true;
    const auto mask = event.kind < 32 ? WEAPON_XP_EVENT_MASK(event.kind) : 0u;
    for (auto* bank : {&m_listeners, &m_filtered}) {
        for (auto& listener : *bank) {
            if (listener.fn && (!listener.mask || (listener.mask & mask)) &&
                (listener.item == WEAPON_XP_ANY_ITEM || listener.item == event.item_id)) {
                const auto fn = listener.fn;
                void* user = listener.user;
                try {
                    fn(user, &event);
                } catch (...) {
                    listener = {};
                }
            }
        }
    }
    m_dispatching = false;
}

}  // namespace weaponxp

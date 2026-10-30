// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "events.hpp"

namespace weaponxp {

int Events::subscribe(weapon_xp_listener_v1 fn, void* user, std::uint32_t* token) {
    if (!fn || !token) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    if (m_dispatching) {
        return WEAPON_XP_BUSY;
    }
    for (auto& listener : m_listeners) {
        if (!listener.fn) {
            if (m_next == 0) {
                return WEAPON_XP_FULL;
            }
            listener = {fn, user, m_next++};
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
    for (auto& listener : m_listeners) {
        if (listener.token == token) {
            listener = {};
            return WEAPON_XP_OK;
        }
    }
    return WEAPON_XP_NOT_FOUND;
}

void Events::emit(const weapon_xp_event_v1& event) noexcept {
    if (m_dispatching) {
        return;
    }
    m_dispatching = true;
    for (auto& listener : m_listeners) {
        if (listener.fn) {
            const auto fn = listener.fn;
            void* user = listener.user;
            try {
                fn(user, &event);
            } catch (...) {
                listener = {};
            }
        }
    }
    m_dispatching = false;
}

}  // namespace weaponxp

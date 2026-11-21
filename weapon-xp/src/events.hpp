// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstdint>

#include "weapon_xp_api.h"

namespace weaponxp {

// Synchronous bounded observer hub. No heap allocation or SDK dependency during dispatch.
class Events {
public:
    int subscribe(weapon_xp_listener_v1 fn, void* user, std::uint32_t* token);
    int subscribe_filtered(std::uint32_t mask, int item, weapon_xp_listener_v1 fn,
                           void* user, std::uint32_t* token);
    int unsubscribe(std::uint32_t token);
    void emit(const weapon_xp_event_v1& event) noexcept;

    bool dispatching() const { return m_dispatching; }

private:
    struct Listener {
        weapon_xp_listener_v1 fn = nullptr;
        void* user = nullptr;
        std::uint32_t token = 0;
        std::uint32_t mask = 0;
        int item = WEAPON_XP_ANY_ITEM;
    };

    static constexpr std::size_t kCapacity = 16;
    using Listeners = std::array<Listener, kCapacity>;
    int subscribe(Listeners& bank, Listener listener, std::uint32_t* token);
    Listeners m_listeners{};
    Listeners m_filtered{};
    std::uint32_t m_next = 1;
    bool m_dispatching = false;
};

}  // namespace weaponxp

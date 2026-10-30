// SPDX-License-Identifier: MIT
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
    int unsubscribe(std::uint32_t token);
    void emit(const weapon_xp_event_v1& event) noexcept;

    bool dispatching() const { return m_dispatching; }

private:
    struct Listener {
        weapon_xp_listener_v1 fn = nullptr;
        void* user = nullptr;
        std::uint32_t token = 0;
    };

    std::array<Listener, 16> m_listeners{};
    std::uint32_t m_next = 1;
    bool m_dispatching = false;
};

}  // namespace weaponxp

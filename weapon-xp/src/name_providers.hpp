// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstdint>

#include "weapon_xp_api.h"

namespace weaponxp {

class NameProviders {
public:
    int subscribe(weapon_xp_name_provider_v5 provider, void* user, std::uint32_t* token);
    int unsubscribe(std::uint32_t token);
    bool resolve(const weapon_xp_name_context_v5& context, char* out,
                 std::uint32_t capacity) noexcept;
    bool dispatching() const { return m_dispatching; }
    std::uint64_t revision() const { return m_revision; }

private:
    struct Entry {
        weapon_xp_name_provider_v5 provider = nullptr;
        void* user = nullptr;
        std::uint32_t token = 0;
    };

    std::array<Entry, 16> m_entries{};
    std::uint32_t m_next = 1;
    std::uint64_t m_revision = 0;
    bool m_dispatching = false;
};

}  // namespace weaponxp

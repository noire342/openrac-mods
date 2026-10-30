// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "weapon_xp_api.h"
#include <array>
#include <cstdint>

namespace weaponxp {
class HitFilters final {
public:
    int subscribe(weapon_xp_hit_filter_v4 filter,void* user,std::uint32_t* token);
    int unsubscribe(std::uint32_t token);
    bool external(const weapon_xp_hit_context_v4& context)noexcept;
    bool dispatching()const{return dispatching_;}
private:
    struct Entry {weapon_xp_hit_filter_v4 filter{};void* user{};std::uint32_t token{};};
    std::array<Entry,16> entries_{};
    std::uint32_t next_=1;
    bool dispatching_=false;
};
}

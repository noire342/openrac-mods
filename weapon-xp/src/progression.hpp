// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "rules.hpp"

namespace weaponxp {

struct Weapon {
    int item;
    const char* name;
};

inline constexpr std::array<Weapon, 12> kWeapons{
    {{0x09, "Suck Cannon"},
     {0x0A, "Bomb Glove"},
     {0x0B, "Devastator"},
     {0x0D, "Visibomb Gun"},
     {0x0F, "Blaster"},
     {0x10, "Pyrocitor"},
     {0x11, "Mine Glove"},
     {0x12, "Walloper"},
     {0x13, "Tesla Claw"},
     {0x14, "Glove of Doom"},
     {0x17, "R.Y.N.O."},
     {0x18, "Drone Device"}}
};
inline constexpr std::array<std::uint32_t, 5> kLegacyThresholds{{0, 150, 450, 900, 1500}};
inline constexpr int kItems = 37;

int weapon_index(int item);
int weapon_from_class(int oclass);
int level(std::uint32_t xp);
float progress(std::uint32_t xp);
int capacity(int base, int weapon_level);

struct Progression {
    std::array<std::uint32_t, kWeapons.size()> xp{};
    std::array<std::uint8_t, kWeapons.size()> quarters{};
    Rules rules{};
    const WeaponRules& rule(int item) const;
    float weapon_progress(int item) const;
    int weapon_capacity(int item, int base) const;
    bool migrated = false;
    int migration_source = 0;
    // Returns the level after adding XP. Values saturate at the maximum level.
    int award(int item, std::uint32_t amount);
    int award_quarters(int item, std::uint32_t amount);
    int weapon_level(int item) const;
    std::uint32_t experience(int item) const;
};

// Whole-file, versioned format. Invalid/future files are refused without changing the state.
std::string encode(const Progression& state);
bool decode(const std::string& data, Progression& state);

}  // namespace weaponxp

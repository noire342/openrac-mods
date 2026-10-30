// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace weaponxp {
inline constexpr int kMaxLevel = 10;
inline constexpr std::uint32_t kMaxExperience = 1000000000;
inline constexpr std::array<std::uint32_t, kMaxLevel> kThresholds{
    {0, 400, 1400, 3200, 6000, 10000, 15500, 23000, 33000, 46000}
};
inline constexpr std::array<float, kMaxLevel> kDamage{
    {1, 1.15f, 1.30f, 1.50f, 1.75f, 2, 2.25f, 2.50f, 2.75f, 3}
};
inline constexpr std::array<float, kMaxLevel> kCapacity{
    {1, 1.125f, 1.25f, 1.375f, 1.50f, 1.625f, 1.75f, 1.875f, 2, 2.125f}
};

struct WeaponRules {
    std::array<std::uint32_t, kMaxLevel> thresholds = kThresholds;
    std::array<float, kMaxLevel> damage = kDamage, ammo = kCapacity;
    unsigned xp_multiplier = 1;  // Integer 0..16; zero pauses awards without erasing XP.
};

using Rules = std::array<WeaponRules, 12>;

struct Settings {
    double xp_rate = 1;
    unsigned kill_xp_cap = 500;
    bool scale_damage = false, scale_capacity = false;
    bool hud = true, reduced_motion = false, animations = true, versioned_names = true;
    bool flip_morph_model = false;
};

bool validate(const WeaponRules& rule, std::string& error);
bool validate(const Settings& settings, std::string& error);
int level(std::uint32_t xp, const WeaponRules& rule);
float progress(std::uint32_t xp, const WeaponRules& rule);
int capacity(int base, int weapon_level, const WeaponRules& rule);
std::uint32_t reward_quarters(int bolts, const Settings& settings, const WeaponRules& rule);
// Strict bounded INI: parse into a temporary set; errors never partially modify a session.
bool parse_rules(const std::string& input, Rules& rules, std::string& error);
std::string rules_template();
}  // namespace weaponxp

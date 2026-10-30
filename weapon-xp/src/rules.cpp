// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "rules.hpp"

#include <algorithm>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>

#include "progression.hpp"

namespace weaponxp {
bool validate(const WeaponRules& r, std::string& error) {
    if (r.thresholds[0] != 0 || r.thresholds.back() > kMaxExperience || r.xp_multiplier > 16) {
        error = "thresholds must start at 0, end at most 1000000000; xp_multiplier must be 0..16";
        return false;
    }
    for (int i = 0; i < kMaxLevel; ++i) {
        if ((i && r.thresholds[i] <= r.thresholds[i - 1]) || !std::isfinite(r.damage[i])
            || !std::isfinite(r.ammo[i]) || r.damage[i] < 1 || r.damage[i] > 16 || r.ammo[i] < 1
            || r.ammo[i] > 16
            || (i && (r.damage[i] < r.damage[i - 1] || r.ammo[i] < r.ammo[i - 1]))) {
            error = "ten strictly increasing thresholds and ten finite nondecreasing multipliers "
                    "in 1..16 required";
            return false;
        }
    }
    if (r.damage[0] != 1 || r.ammo[0] != 1) {
        error = "level 1 multipliers must be 1";
        return false;
    }
    return true;
}

bool validate(const Settings& s, std::string& error) {
    if (!std::isfinite(s.xp_rate) || s.xp_rate < .25 || s.xp_rate > 4 || s.kill_xp_cap > 100000) {
        error = "xp_rate must be .25..4; kill_xp_cap must be 0..100000";
        return false;
    }
    return true;
}

int level(std::uint32_t xp, const WeaponRules& r) {
    return int(
        std::upper_bound(r.thresholds.begin(), r.thresholds.end(), xp) - r.thresholds.begin()
    );
}

float progress(std::uint32_t xp, const WeaponRules& r) {
    const int lv = level(xp, r);
    return lv == kMaxLevel
               ? 1
               : float(xp - r.thresholds[lv - 1]) / float(r.thresholds[lv] - r.thresholds[lv - 1]);
}

int capacity(int base, int lv, const WeaponRules& r) {
    return base <= 0
               ? 0
               : int(std::min(
                     65535.0, std::ceil(double(base) * r.ammo[std::clamp(lv, 1, kMaxLevel) - 1])
                 ));
}

std::uint32_t reward_quarters(int bolts, const Settings& s, const WeaponRules& r) {
    std::string error;
    if (bolts < 0 || !validate(s, error)) {
        return 0;
    }
    return std::min(unsigned(bolts), s.kill_xp_cap) * unsigned(std::lround(s.xp_rate * 4))
           * r.xp_multiplier;
}

namespace {
std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

template <class T>
bool number(const std::string& s, T& value) {
    if (s.empty() || s[0] == '-' || s[0] == '+') {
        return false;
    }
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    if (!(in >> value)) {
        return false;
    }
    in >> std::ws;
    return in.eof();
}

template <class T, std::size_t N>
bool list(const std::string& s, std::array<T, N>& out) {
    std::istringstream in(s);
    std::string word;
    for (std::size_t i = 0; i < N; ++i) {
        if (!std::getline(in, word, ',') || !number(trim(word), out[i])) {
            return false;
        }
    }
    return !std::getline(in, word, ',') && !s.empty() && s.back() != ',';
}
}  // namespace

bool parse_rules(const std::string& input, Rules& out, std::string& error) {
    if (input.size() > 32768) {
        error = "rules file exceeds 32 KiB";
        return false;
    }
    Rules next{};
    std::set<std::string> keys;
    std::set<int> sections;
    bool version = false;
    int item = -1;
    unsigned line = 0;
    std::istringstream in(input);
    std::string text;
    auto fail = [&](const std::string& why) {
        error = "line " + std::to_string(line) + ": " + why;
        return false;
    };
    while (std::getline(in, text)) {
        ++line;
        text = trim(text.substr(0, text.find_first_of(";#")));
        if (text.empty()) {
            continue;
        }
        if (text.front() == '[') {
            if (text.back() != ']' || text.rfind("[weapon.", 0) != 0
                || !number(text.substr(8, text.size() - 9), item) || weapon_index(item) < 0
                || !sections.insert(item).second) {
                return fail("invalid or duplicate weapon section");
            }
            continue;
        }
        const auto eq = text.find('=');
        if (eq == std::string::npos) {
            return fail("expected key=value");
        }
        const auto key = trim(text.substr(0, eq)), value = trim(text.substr(eq + 1));
        if (!keys.insert(std::to_string(item) + "/" + key).second) {
            return fail("duplicate key");
        }
        if (item < 0) {
            unsigned n = 0;
            if (key != "format" || !number(value, n) || n != 1) {
                return fail("expected format=1 before weapon sections");
            }
            version = true;
            continue;
        }
        if (!version) {
            return fail("missing format=1");
        }
        auto& r = next[weapon_index(item)];
        bool ok = false;
        if (key == "thresholds") {
            ok = list(value, r.thresholds);
        } else if (key == "damage") {
            ok = list(value, r.damage);
        } else if (key == "ammo") {
            ok = list(value, r.ammo);
        } else if (key == "xp_multiplier") {
            ok = number(value, r.xp_multiplier);
        }
        if (!ok) {
            return fail("unknown key or invalid value: " + key);
        }
    }
    if (!version) {
        return fail("missing format=1");
    }
    for (const auto& r : next) {
        if (!validate(r, error)) {
            return false;
        }
    }
    out = next;
    return true;
}

std::string rules_template() {
    return "; Weapon Experience rules v1. Omitted keys/weapons use shipped defaults.\n"
           "; Reload with API v3 or restart. Bad files are rejected atomically.\n"
           "; Keys: thresholds (10 integers), damage/ammo (10 multipliers), xp_multiplier "
           "(0..16).\n"
           "; Weapon IDs: 9,10,11,13,15,16,17,18,19,20,23,24.\nformat=1\n\n"
           "; Example: Blaster. Remove leading semicolons to override.\n"
           ";[weapon.15]\n;thresholds=0,400,1400,3200,6000,10000,15500,23000,33000,46000\n"
           ";damage=1,1.15,1.30,1.50,1.75,2,2.25,2.5,2.75,3\n"
           ";ammo=1,1.125,1.25,1.375,1.5,1.625,1.75,1.875,2,2.125\n;xp_multiplier=1\n";
}
}  // namespace weaponxp

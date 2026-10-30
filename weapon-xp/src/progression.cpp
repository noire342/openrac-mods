// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "progression.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace weaponxp {

int weapon_index(int item) {
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        if (kWeapons[i].item == item) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int weapon_from_class(int oclass) {
    // Source weapon classes, not generic missiles: enemies also fire missiles.
    // OpenRAC games/rac1/include/moby_class_id.h and the weapon hit descriptors.
    switch (oclass) {
        case 849:  // Suck Cannon's stamped source, also on launched captured mobys.
            return 0x09;
        case 153:
        case 157:
            return 0x0B;
        case 163:
        case 172:
            return 0x0D;
        case 168:
            return 0x0F;
        case 176:
            return 0x10;
        case 177:
            return 0x13;
        case 180:
            return 0x12;
        case 186:
            return 0x14;
        case 190:
            return 0x11;
        case 192:
            return 0x0A;
        case 454:
            return 0x17;
        case 479:
            return 0x18;
        default:
            return -1;
    }
}

int level(std::uint32_t xp) {
    return level(xp, WeaponRules{});
}

float progress(std::uint32_t xp) {
    return progress(xp, WeaponRules{});
}

int capacity(int base, int lv) {
    return capacity(base, lv, WeaponRules{});
}

const WeaponRules& Progression::rule(int item) const {
    static const WeaponRules fallback;
    const int i = weapon_index(item);
    return i < 0 ? fallback : rules[i];
}

float Progression::weapon_progress(int item) const {
    return progress(experience(item), rule(item));
}

int Progression::weapon_capacity(int item, int base) const {
    return capacity(base, weapon_level(item), rule(item));
}

int Progression::award(int item, std::uint32_t amount) {
    const int index = weapon_index(item);
    if (index < 0) {
        return 0;
    }
    const auto cap = rules[index].thresholds.back();
    if (xp[index] >= cap) {
        return weapon_level(item);  // Config changes never erase earned XP.
    }
    xp[index] += std::min(amount, cap - xp[index]);
    if (xp[index] == rules[index].thresholds.back()) {
        quarters[index] = 0;
    }
    return weapon_level(item);
}

int Progression::award_quarters(int item, std::uint32_t amount) {
    const int index = weapon_index(item);
    if (index < 0) {
        return 0;
    }
    if (xp[index] >= rules[index].thresholds.back()) {
        return weapon_level(item);
    }
    const auto total = std::uint64_t(quarters[index]) + amount;
    const int result = award(item, static_cast<std::uint32_t>(total / 4));
    quarters[index] =
        xp[index] == rules[index].thresholds.back() ? 0 : static_cast<std::uint8_t>(total % 4);
    return result;
}

int Progression::weapon_level(int item) const {
    return level(experience(item), rule(item));
}

std::uint32_t Progression::experience(int item) const {
    const int index = weapon_index(item);
    return index < 0 ? 0 : xp[index];
}

namespace {
std::uint32_t checksum(const std::string& text) {
    std::uint32_t result = 2166136261u;
    for (unsigned char c : text) {
        result = (result ^ c) * 16777619u;
    }
    return result;
}
}  // namespace

std::string encode(const Progression& state) {
    std::ostringstream body;
    body << "OPENRAC_WEAPON_XP 4\n";
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        body << kWeapons[i].item << ' ' << state.xp[i] << ' ' << unsigned(state.quarters[i])
             << '\n';
    }
    const std::string payload = body.str();
    body << "CHECKSUM " << std::hex << checksum(payload) << '\n';
    return body.str();
}

bool decode(const std::string& data, Progression& state) {
    const bool legacy = data.rfind("OPENRAC_WEAPON_XP 1\n", 0) == 0;
    const bool v2 = data.rfind("OPENRAC_WEAPON_XP 2\n", 0) == 0;
    const bool v3 = data.rfind("OPENRAC_WEAPON_XP 3\n", 0) == 0;
    if (data.size() > 4096
        || (!legacy && !v2 && !v3 && data.rfind("OPENRAC_WEAPON_XP 4\n", 0) != 0)) {
        return false;
    }
    const auto split = data.find("CHECKSUM ");
    if (split == std::string::npos) {
        return false;
    }
    std::istringstream signature(data.substr(split));
    std::string word;
    std::uint32_t expected = 0;
    if (!(signature >> word >> std::hex >> expected) || word != "CHECKSUM"
        || checksum(data.substr(0, split)) != expected) {
        return false;
    }
    signature >> std::ws;
    if (!signature.eof()) {
        return false;
    }
    Progression next;
    std::istringstream input(data.substr(0, split));
    int version = 0;
    input >> word >> version;
    const auto limit = legacy ? kLegacyThresholds.back()
                       : v2   ? kThresholds[4]
                       : v3   ? kThresholds.back()
                              : kMaxExperience;
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        int item = 0;
        std::uint64_t xp = 0;
        unsigned fraction = 0;
        if (!(input >> item >> xp) || item != kWeapons[i].item || xp > limit) {
            return false;
        }
        if (!legacy && (!(input >> fraction) || fraction >= 4 || (xp == limit && fraction))) {
            return false;
        }
        if (legacy) {
            const auto old = static_cast<std::uint32_t>(xp);
            const auto stage =
                std::upper_bound(kLegacyThresholds.begin(), kLegacyThresholds.end(), old)
                - kLegacyThresholds.begin() - 1;
            xp = kThresholds[stage];
            if (stage < 4) {
                xp += std::uint64_t(old - kLegacyThresholds[stage])
                      * (kThresholds[stage + 1] - kThresholds[stage])
                      / (kLegacyThresholds[stage + 1] - kLegacyThresholds[stage]);
            }
        }
        next.xp[i] = static_cast<std::uint32_t>(xp);
        next.quarters[i] = static_cast<std::uint8_t>(fraction);
    }
    input >> std::ws;
    if (!input.eof()) {
        return false;
    }
    next.rules = state.rules;
    next.migrated = version < 4;
    next.migration_source = next.migrated ? version : 0;
    state = next;
    return true;
}

}  // namespace weaponxp

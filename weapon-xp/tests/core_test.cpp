// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <cmath>
#include <limits>

#include "check.hpp"
#include "combat.hpp"

using namespace weaponxp;

int main() {
    Progression state;
    Settings settings;
    Combat combat(state, settings);
    // A bomb damages A, then a Blaster damages B: their delayed deaths keep the source weapon.
    CHECK(combat.hit(100, 0x0A, 2.0f, 0) == 2.0f);
    combat.hit(200, 0x0F, 1.0f, 1);
    const auto bomb = combat.death(100, 10, 2);
    CHECK(bomb.item == 0x0A && state.experience(0x0A) == 10 && state.experience(0x0F) == 0);
    CHECK(!combat.death(100, 10, 2).changed);
    CHECK(combat.death(200, 10, 3).item == 0x0F);
    // A later hit by another weapon gets the kill. Invalid hits do not steal attribution.
    combat.hit(300, 0x0A, 2, 4);
    combat.hit(300, 0x0F, 1, 5);
    combat.hit(300, 0x0B, 0, 6);
    CHECK(combat.death(300, 10, 7).item == 0x0F);
    CHECK(!combat.death(999, 10, 7).changed);
    combat.hit(400, 0x0A, 2, 8);
    CHECK(!combat.death(400, 10, 129).changed);
    CHECK(!combat.death(300, 0, 8).changed);
    // Every boundary, overflow saturation and untouched gadgets.
    for (int i = 0; i < kMaxLevel; ++i) {
        CHECK(level(kThresholds[i]) == i + 1);
        if (i > 0) {
            CHECK(level(kThresholds[i] - 1) == i);
        }
    }
    CHECK(state.award(0x0C, 100) == 0);
    CHECK(state.award(0x0A, std::numeric_limits<std::uint32_t>::max()) == kMaxLevel);
    CHECK(state.experience(0x0A) == kThresholds.back() && state.award(0x0A, 100) == kMaxLevel);
    CHECK(combat.hit(500, 0x0A, 4, 200) == 4);  // Default: XP only, original damage.
    settings.scale_damage = true;
    CHECK(combat.hit(501, 0x0A, 4, 200) == 12);
    settings.scale_damage = false;
    CHECK(combat.hit(502, 0x0A, 4, 200) == 4);
    CHECK(progress(kThresholds.back()) == 1 && level(UINT32_MAX) == 10);
    CHECK(capacity(40, 10) == 85 && capacity(40, 99) == 85);
    CHECK(capacity(40, 1) == 40 && capacity(40, 5) == 60);
    CHECK(capacity(0, 5) == 0 && capacity(65535, 5) == 65535);
    CHECK(weapon_from_class(49) == -1);  // Generic enemy/player rocket is ambiguous.
    CHECK(weapon_from_class(168) == 0x0F);
    const auto data = encode(state);
    Progression restored;
    CHECK(decode(data, restored) && restored.xp == state.xp);
    const auto before = restored.xp;
    auto corrupt = data;
    corrupt[30] ^= 1;
    CHECK(!decode(corrupt, restored) && restored.xp == before);
    CHECK(!decode(data + "junk", restored));
    CHECK(!decode("OPENRAC_WEAPON_XP 2\n", restored));
    std::puts("progression and combat: ok");
}

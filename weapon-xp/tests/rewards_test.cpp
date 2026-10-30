// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <cstring>

#include "check.hpp"
#include "combat.hpp"
#include "guest_memory.hpp"
#include "native_rewards.hpp"
using namespace weaponxp;
GuestMemory ram;

template <class T>
void put(unsigned at, T value) {
    std::memcpy(ram.data() + at, &value, sizeof value);
}

unsigned relocate(unsigned at) {
    return at;
}

int main() {
    CHECK(nominal_bolts(16, 16, 0, 0xFF, 5) == 16);
    CHECK(nominal_bolts(39, 39, 0, 0xFF, 5) == 39);
    CHECK(nominal_bolts(4, 100, 0, 3, 5) == 100);
    CHECK(nominal_bolts(4, 100, 60, 3, 5) == 4);
    CHECK(nominal_bolts(1, 100, 100, 3, 5) == 1);
    CHECK(nominal_bolts(0, 100, 100, 3, 5) == 1);  // Retail's floor for tracked bolt sources.
    CHECK(nominal_bolts(0, 0, 0, 0xFF, 5) == 0);
    CHECK(nominal_bolts(40, 40, 0, 0xFE, 5) == 0);
    CHECK(nominal_bolts(40, 40, 0, 0xFF, -1) == 0);
    for (const auto& d : kDiscs) {
        std::memset(ram.data(), 0, 0x2000000);
        openrac_api api{};
        api.memory = ram.data();
        api.relocate_data = relocate;
        const bool pal = std::strcmp(d.version, "pal") == 0;
        put<int>(pal ? 0x15EE84 : 0x15ED84, 1);
        const unsigned m = 0x700000, stats = pal ? 0x14D690 : 0x14D590;
        put<short>(m + 0xA6, 459);
        put<unsigned char>(m + 0xB1, 3);
        put<short>(m + 0xB2, 42);
        put<short>(m + 0xB4, 4);
        put<short>(m + 0xB6, 100);
        NativeRewards rewards(&api, d);
        CHECK(rewards.bolts(m) == 100);
        put<short>(stats + (1 * 64 + 3) * 4 + 2, 60);
        CHECK(rewards.bolts(m) == 4);
        put<short>(m + 0xB4, 1);
        CHECK(rewards.bolts(m) == 1);
        put<short>(m + 0xA6, 6);
        CHECK(rewards.bolts(m) == -1);  // Crates never grant weapon XP.
    }
    Progression state;
    Settings settings;
    Combat combat(state, settings);
    settings.xp_rate = .25;
    for (unsigned i = 1; i <= 3; ++i) {
        combat.hit(i, 15, 1, i);
        CHECK(combat.death(i, 1, i).changed);
    }
    CHECK(state.experience(15) == 0 && state.quarters[weapon_index(15)] == 3);
    Progression loaded;
    CHECK(decode(encode(state), loaded));
    CHECK(loaded.quarters == state.quarters);
    Settings resumed_settings;
    Combat resumed(loaded, resumed_settings);
    resumed_settings.xp_rate = .25;
    resumed.hit(4, 15, 1, 4);
    resumed.death(4, 1, 4);
    CHECK(loaded.experience(15) == 1 && loaded.quarters[weapon_index(15)] == 0);
    settings.xp_rate = 1;
    combat.hit(50, 15, 1, 50);
    combat.death(50, 5000, 50);
    CHECK(state.experience(15) == kMaxKillXp);
    combat.hit(51, 20, 1, 51);
    const auto zero = combat.death(51, 0, 51);
    CHECK(zero.item == 20 && !zero.changed);  // Doom can celebrate a kill with no XP payout.
    const char* legacy = "OPENRAC_WEAPON_XP 1\n9 0\n10 0\n11 0\n13 0\n15 1500\n16 0\n17 0\n18 "
                         "0\n19 0\n20 0\n23 0\n24 0\nCHECKSUM 72421153\n";
    CHECK(decode(legacy, loaded) && loaded.migrated && loaded.experience(15) == kThresholds[4]);
    CHECK(loaded.weapon_level(15) == 5);
    CHECK(decode(encode(loaded), state) && !state.migrated && state.xp == loaded.xp);
    std::puts(
        "retail nominal bolt budgets, source depletion, PAL/NTSC, fractional persistence, kill cap "
        "and legacy level preservation: ok"
    );
}

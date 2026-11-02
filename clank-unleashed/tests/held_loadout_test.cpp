// SPDX-License-Identifier: GPL-3.0-or-later
#include "held_loadout.hpp"
#include <cstring>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

int main() {
    openrac_held original{};
    original.size = sizeof(original);
    original.count = 5;
    original.pieces[0] = {0, 0, 8, 42, 5, OPENRAC_HELD_LOOSE};
    original.pieces[1] = {3, 0, 2, 607, 2}; // Helipack.
    original.pieces[2] = {3, 1, 2, 605, 2}; // Separate Clank body.
    original.pieces[3] = {1, 0, 6, 612, 3, OPENRAC_HELD_HIDDEN};
    original.pieces[4] = {2, 0, 9, 607, 4}; // Same class in another slot.
    const auto saved = original;
    CHECK(clank::carries_clank(original));
    auto parked = clank::parked_loadout(original);
    CHECK(parked.size == sizeof(parked) && parked.count == 3);
    CHECK(std::memcmp(&parked.pieces[0], &original.pieces[0], sizeof(openrac_held_piece)) == 0);
    CHECK(std::memcmp(&parked.pieces[1], &original.pieces[3], sizeof(openrac_held_piece)) == 0);
    CHECK(std::memcmp(&parked.pieces[2], &original.pieces[4], sizeof(openrac_held_piece)) == 0);
    CHECK(std::memcmp(&original, &saved, sizeof(original)) == 0);

    // Upgrading the pack changes its class, not which slot must disappear.
    original.pieces[1].oclass = 608;
    original.pieces[2].oclass = 999;
    parked = clank::parked_loadout(original);
    CHECK(parked.count == 3 && clank::carries_clank(original));
    original.pieces[2].flags = OPENRAC_HELD_HIDDEN;
    CHECK(!clank::carries_clank(original));
    original.count = 2;
    CHECK(!clank::carries_clank(original)); // A pack alone is not Clank.
    original.count = -1;
    CHECK(clank::parked_loadout(original).count == 0);
    original = {};
    original.count = OPENRAC_HELD_MAX + 100;
    CHECK(clank::parked_loadout(original).count == OPENRAC_HELD_MAX);
}

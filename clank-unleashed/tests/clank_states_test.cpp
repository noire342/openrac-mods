// SPDX-License-Identifier: GPL-3.0-or-later
#include "clank_states.hpp"
#include <stdexcept>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)
int main() {
    for (int state = 0x43; state <= 0x52; ++state)
        CHECK(clank::clank_state(state) == state);
    CHECK(clank::clank_state(0x77) == 0x77); // Native fatal fall explicitly supports Clank.
    CHECK(clank::clank_state(0x64) == 0x64); // Native scene entry.
    for (int hazard :
         {0x33, 0x34, 0x35, 0x36, 0x37, 0x68, 0x69, 0x6A, 0x73, 0x75, 0x7B, 0x7C, 0x7F})
        CHECK(clank::clank_state(hazard) == 0x7D);
    CHECK(clank::clank_state(0) == 0x43 && clank::clank_state(2) == 0x44);
    CHECK(clank::clank_state(0xE) == 0x4F && clank::clank_state(0x13) == 0x51);
}

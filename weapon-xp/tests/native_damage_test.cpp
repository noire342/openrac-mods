// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <vector>

#include "check.hpp"
#include "native_damage.hpp"

using namespace weaponxp;

static unsigned relocate(unsigned a) {
    return a + 0x4000;
}

int main() {
    std::vector<unsigned char> memory(0x2000000);
    openrac_api api{};
    api.memory = memory.data();
    api.relocate_data = relocate;
    for (const auto& disc : kDiscs) {
        const auto at = relocate(std::strcmp(disc.version, "pal") == 0 ? 0x1600C8 : 0x15FFC8);
        auto values = reinterpret_cast<float*>(memory.data() + at);
        values[0] = .667f;
        values[1] = .25f;
        NativeDamage damage(&api, disc);
        damage.restore();
        CHECK(values[0] == .667f && values[1] == .25f);
        damage.prepare(3);
        CHECK(values[0] == .667f * 3 && values[1] == .75f);
        damage.prepare(3);  // Never compound a pending reaction override.
        CHECK(values[0] == .667f * 3 && values[1] == .75f);
        damage.restore();
        CHECK(values[0] == .667f && values[1] == .25f);
        damage.prepare(1);
        CHECK(values[0] == .667f && values[1] == .25f);
        damage.prepare(3);
        values[1] = 7;  // Respect changes made by a different owner.
        damage.restore();
        CHECK(values[0] == .667f && values[1] == 7);
    }
    std::puts("native Blaster damage replacement and scoped restoration: ok");
}

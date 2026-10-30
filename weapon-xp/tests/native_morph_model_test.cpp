// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <vector>

#include "check.hpp"
#include "native_morph_model.hpp"
using namespace weaponxp;

static unsigned relocate(unsigned a) {
    return a + 0x4000;
}

static int current_level = 1;

static int loaded() {
    return current_level;
}

template <class T>
void put(std::vector<unsigned char>& memory, unsigned at, T value) {
    std::memcpy(memory.data() + at, &value, sizeof(value));
}

int main() {
    std::vector<unsigned char> memory(0x2000000);
    openrac_api api{};
    api.memory = memory.data();
    api.relocate_data = relocate;
    api.loaded_level = loaded;
    for (const auto& disc : kDiscs) {
        std::fill(memory.begin(), memory.end(), 0);
        current_level = 1;
        constexpr unsigned moby = 0x500000, vars = 0x600000;
        put(memory, relocate(disc.hero) + 0x1090, moby);
        put(memory, relocate(disc.item_in_hand), 21);
        put<short>(memory, moby + 0xA6, 185);
        put(memory, moby + 0x78, vars);
        const std::array<float, 12> identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}};
        std::memcpy(memory.data() + moby + 0xC0, identity.data(), sizeof(identity));
        const auto snapshot = memory;
        NativeMorphModel pose(&api, disc);
        pose.after(moby, false);
        CHECK(memory == snapshot);
        pose.after(moby, true);
        float flipped[12];
        std::memcpy(flipped, memory.data() + moby + 0xC0, sizeof(flipped));
        CHECK(flipped[0] == -1 && flipped[5] == 1 && flipped[10] == -1);
        std::memcpy(memory.data() + moby + 0xC0, identity.data(), sizeof(identity));
        CHECK(memory == snapshot);  // No beam/private state/position/aim changed.
        pose.reset();
        CHECK(memory == snapshot);  // Native owner already restored the basis.
        pose.after(moby, true);
        pose.reset();
        CHECK(memory == snapshot);
        pose.after(moby, true);
        put<float>(memory, moby + 0xC0, 7);
        pose.reset();
        float value;
        std::memcpy(&value, memory.data() + moby + 0xC0, 4);
        CHECK(value == 7);
        std::memcpy(memory.data() + moby + 0xC0, identity.data(), sizeof(identity));
        pose.after(moby, true);
        current_level = 2;
        pose.reset();
        std::memcpy(&value, memory.data() + moby + 0xC0, 4);
        CHECK(value == -1);  // Never touch a new overlay.
    }
    std::puts("optional Morph-o-Ray model orientation, state isolation and scoped restoration: ok");
}

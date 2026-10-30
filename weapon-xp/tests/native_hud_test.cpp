// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <array>
#include <cstdlib>
#include <cstring>

#include "check.hpp"
#include "guest_memory.hpp"
#include "native_hud.hpp"
using namespace weaponxp;
GuestMemory memory;

template <class T>
void put(unsigned at, T value) {
    std::memcpy(memory.data() + at, &value, sizeof value);
}

template <class T>
T get(unsigned at) {
    T value;
    std::memcpy(&value, memory.data() + at, sizeof value);
    return value;
}

unsigned identity(unsigned a) {
    return a;
}

unsigned overlay(int, unsigned a) {
    return a;
}

void dummy() {}

void* function(const char*) {
    return reinterpret_cast<void*>(dummy);
}

unsigned xy(int x, int y) {
    return unsigned(x & 0xFFFF) | (unsigned(y & 0xFFFF) << 16);
}

void rectangle(unsigned at, int top, int bottom) {
    put<unsigned>(at, 0x10000003);
    put<unsigned>(at + 48, xy(36000, top));
    put<unsigned>(at + 56, xy(36400, bottom));
    put<unsigned>(at + 52, 0xFFFFF0);
    put<unsigned>(at + 60, 0xFFFFF0);
}

int main() {
    for (const auto& disc : kDiscs) {
        openrac_api api{};
        api.memory = memory.data();
        api.function = function;
        api.relocate_data = identity;
        api.relocate_level_data = overlay;
        NativeHud renderer(&api, disc);
        const unsigned start = 0x400000,
                       cursor = std::strcmp(disc.version, "pal") == 0 ? 0x161000 : 0x160F00;
        rectangle(start, 28000, 30800);
        rectangle(start + 64, 29680, 30800);
        rectangle(start + 128, 30224, 30256);
        std::array<unsigned char, 128> frame;
        frame.fill(0xA5);
        std::memcpy(memory.data() + start + 192, frame.data(), frame.size());
        put<unsigned>(start + 192, 0x10000007);
        std::memcpy(frame.data(), memory.data() + start + 192, 128);
        put<unsigned>(cursor, start + 320);
        renderer.reflow_gauge(start, {512, 112, 42, 192});
        const int top = int(get<unsigned>(start + 48) >> 16),
                  bottom = int(get<unsigned>(start + 56) >> 16);
        const int fill_top = int(get<unsigned>(start + 64 + 48) >> 16);
        CHECK(top > 28000 && bottom == 30800);
        CHECK((get<unsigned>(start + 56) & 0xFFFF) == 36400 - 64);
        CHECK(
            std::abs((bottom - fill_top) * 5 - (bottom - top) * 2) <= 5
        );  // at most 1/16-pixel rounding
        CHECK(get<unsigned>(start + 52) == 0xFFFFF0);
        CHECK(std::memcmp(frame.data(), memory.data() + start + 192, 128) == 0);
        const auto before = get<unsigned>(start + 48);
        put<unsigned>(start, 0x10000001);
        renderer.reflow_gauge(start, {512, 112, 42, 192});
        CHECK(get<unsigned>(start + 48) == before);
    }
    std::puts(
        "native gauge header clearance, ammo ratio, original frame and depth preservation: ok"
    );
}

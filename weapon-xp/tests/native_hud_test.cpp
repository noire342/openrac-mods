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

const Disc* current_disc;
unsigned general_cursor;

void native_rectangle(int, int, int, int, std::uint64_t, int) {
    const auto packet = get<unsigned>(general_cursor);
    put<unsigned>(packet, 0x10000003);
    put<unsigned>(packet + 52, 0xFFFFF0);
    put<unsigned>(packet + 60, 0xFFFFF0);
    put<unsigned>(general_cursor, packet + 64);
}

void* function(const char* name) {
    if (std::strcmp(name, current_disc->rectangle) == 0) {
        return reinterpret_cast<void*>(native_rectangle);
    }
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
        std::memset(memory.data(), 0, 0x2000000);
        current_disc = &disc;
        openrac_api api{};
        api.memory = memory.data();
        api.function = function;
        api.relocate_data = identity;
        api.relocate_level_data = overlay;
        NativeHud renderer(&api, disc);
        const unsigned start = 0x400000,
                       cursor = std::strcmp(disc.version, "pal") == 0 ? 0x161000 : 0x160F00;
        general_cursor = cursor;
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

        // PAL's level-0 DMA cursor and texture group are not a uniform offset from NTSC.
        // Give the former, incorrect locations valid decoys: drawing into the wrong stream
        // or reading a nearby depth must fail even when both addresses look plausible.
        const bool pal = std::strcmp(disc.version, "pal") == 0;
        const unsigned hud = pal ? 0x161280 : 0x1611C0,
                       layer = pal ? 0x17E5D8 : 0x17E4D8;
        put<unsigned>(cursor, 0x500000);
        put<unsigned>(hud, 0x510000);
        put<unsigned>(layer + 12, 0xFFFFF5);
        if (pal) {
            put<unsigned>(0x161240, 0x520000);
            put<unsigned>(0x17E558 + 12, 0xFFFFF0);
        }
        renderer.draw_gauge({{Draw::Kind::Rectangle, 2, 3, 12, 8, 0x807F7F7F, {}}});
        CHECK(get<unsigned>(cursor) == 0x500000);
        CHECK(get<unsigned>(hud) == 0x510040);
        CHECK(get<unsigned>(0x510000) == 0x10000003);
        CHECK(get<unsigned>(0x510000 + 52) == 0xFFFFF6);
        CHECK(get<unsigned>(0x510000 + 60) == 0xFFFFF6);
        if (pal) {
            CHECK(get<unsigned>(0x161240) == 0x520000);
            CHECK(get<unsigned>(0x520000) == 0);
        }
    }
    std::puts(
        "native gauge clearance, ammo ratio, frame preservation and regional HUD stream/depth: ok"
    );
}

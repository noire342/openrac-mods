// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <cmath>
#include <cstring>
#include <vector>

#include "check.hpp"
#include "guest_memory.hpp"
#include "native_upgrades.hpp"
using namespace weaponxp;
GuestMemory memory;
const Disc* disc;
std::vector<int> scenes;
unsigned notifications = 0;

template <class T>
void put(unsigned a, T v) {
    std::memcpy(memory.data() + a, &v, sizeof v);
}

template <class T>
T get(unsigned a) {
    T v;
    std::memcpy(&v, memory.data() + a, sizeof v);
    return v;
}

unsigned identity(unsigned a) {
    return a;
}

unsigned level_address(int level, unsigned a) {
    CHECK(level == 0 || level == 1);
    return a;
}

unsigned allocate(int size) {
    CHECK(size == 1792);
    return 0x600000;
}

unsigned select(unsigned p, unsigned n) {
    return std::strcmp(disc->version, "pal") == 0 ? p : n;
}

void present(unsigned trigger, int item) {
    CHECK(trigger == 0x600000);
    CHECK(get<unsigned>(trigger + 0xA6) == 123);
    scenes.push_back(item);
    put<int>(disc->mode, 5);
    const auto scene = select(0x16CD60, 0x16CCE0);
    put<short>(scene + 0x44, 1);
    put<unsigned>(scene + 0x178, 0x5A0000);
    put<short>(0x5A00A6, 0);
    put<unsigned>(0x5A0078, 0x5B0000);
    put<unsigned>(scene + 0x54, 0x5B1000);
    put<float>(0x5B1000, 10.0f); // The native stage camera lies along +X from its hero.
    put<int>(disc->hero + 0x80, 987);
}

void* function(const char* name) {
    if (std::strstr(name, "FFAB8") || std::strstr(name, "ff288")) {
        return reinterpret_cast<void*>(allocate);
    }
    if (std::strstr(name, "217718") || std::strstr(name, "216f90")) {
        return reinterpret_cast<void*>(+[](unsigned pos, unsigned rot, int mode, int flag) {
            CHECK(mode == 0 && flag == 1);
            std::memcpy(memory.data() + disc->hero + 0x80, memory.data() + pos, 16);
            std::memcpy(memory.data() + disc->hero + 0x90, memory.data() + rot, 16);
        });
    }
    if (std::strstr(name, "264DB8") || std::strstr(name, "263d40")) {
        return reinterpret_cast<void*>(+[](int id, int frames) {
            CHECK(id == 999 && frames == 180);
            ++notifications;
        });
    }
    return reinterpret_cast<void*>(present);
}

int main() {
    for (const auto& d : kDiscs) {
        disc = &d;
        scenes.clear();
        std::memset(memory.data(), 0, 0x2000000);
        openrac_api api{};
        api.memory = memory.data();
        api.function = function;
        api.relocate_data = identity;
        api.relocate_level_data = level_address;
        const auto bank_global = select(0x15F780, 0x15F6A0), count = select(0x1997FC, 0x1996FC);
        const auto defs = select(0x1864D0, 0x1863D0), vendor = select(0x1CABC0, 0x1CA940);
        put<unsigned>(bank_global, 0x500000);
        put<int>(count, 3);
        put<int>(defs + 10 * 0x4C, 999);
        put<int>(defs + 16 * 0x4C, 1000);
        put<unsigned>(0x500010, 0x510100);
        put<int>(0x500014, 1000);
        std::memcpy(memory.data() + 0x510100, "Inceneritore", 13);
        put<unsigned>(0x500000, 0x510000);
        put<int>(0x500004, 999);
        std::memcpy(memory.data() + 0x510000, "Guanto Bomba", 13);
        put<int>(defs + 18 * 0x4C, 1001);
        put<unsigned>(0x500020, 0x510200);
        put<int>(0x500024, 1001);
        std::memcpy(memory.data() + 0x510200, "Walloper", 9);
        put<unsigned>(d.hero + 0x2080, 0x550000);
        put<unsigned>(0x5500A6, 123);
        put<unsigned short>(0x550034, 0x40);
        put<int>(d.hero + 0x22A8, 4);
        put<int>(d.hero + 0x80, 456);
        put<int>(d.item_in_hand, 15);
        put<int>(vendor, 1234);
        // The gameplay camera is along +Y; the acquisition stage must turn by pi/2.
        const auto camera = select(0x166D80, 0x166C80);
        put<float>(camera + 0x140, 0.0f);
        put<float>(camera + 0x144, 10.0f);
        if (std::strcmp(d.version, "pal") == 0) {
            // This nearby old PAL mapping would incorrectly leave the stage facing +X.
            put<float>(0x166D00 + 0x140, 10.0f);
        }
        Progression state;
        state.award(10, 2480);
        state.award(16, kThresholds.back());
        {
            NativeUpgrades upgrades(&api, d);
            upgrades.enqueue(10, 1, 3);
            upgrades.tick(state, false);
            CHECK(
                std::strcmp(
                    reinterpret_cast<char*>(memory.data() + get<unsigned>(0x500010)),
                    "Inceneritore v10"
                )
                == 0
            );
            const auto name = get<unsigned>(0x500000);
            CHECK(
                std::strcmp(reinterpret_cast<char*>(memory.data() + name), "Guanto Bomba v3") == 0
            );
            CHECK(scenes.empty());
            upgrades.tick(state, true);
            CHECK(upgrades.active() && scenes.empty());
            unsigned char report[]{0, 0, 0, 255, 0, 255};
            upgrades.filter_report(report, 6);
            CHECK(report[0] == 255 && report[1] == 255 && report[2] == 128 && report[5] == 128);
            put<unsigned char>(d.hero + 0x20A8, 1);
            for (int i = 0; i < 5; ++i) {
                upgrades.tick(state, true);
            }
            CHECK(scenes.empty());
            put<unsigned char>(d.hero + 0x20A8, 0);
            upgrades.tick(state, true);
            upgrades.tick(state, true);
            CHECK(upgrades.active() && scenes.size() == 1 && scenes[0] == 10);
            CHECK(std::abs(get<float>(vendor + 0x80)) < .0001f);
            CHECK(std::abs(get<float>(vendor + 0x84) - 1.0f) < .0001f);
            CHECK(std::abs(get<float>(vendor + 0xB8) - 1.5707963f) < .0001f);
            CHECK(get<unsigned short>(0x550034) == 0x41);
            CHECK(
                std::strcmp(reinterpret_cast<char*>(memory.data() + name), "Guanto Bomba v2") == 0
            );
            upgrades.tick(state, false);
            CHECK(scenes.size() == 1);
            put<int>(d.mode, 0);
            upgrades.tick(state, true);
            CHECK(upgrades.active());
            upgrades.tick(state, true);
            CHECK(!upgrades.active() && get<int>(d.hero + 0x80) == 456 && get<int>(vendor) == 1234);
            CHECK(get<unsigned short>(0x550034) == 0x40);
            CHECK(get<int>(d.hero + 0x20B8) == 15);
            for (int i = 0; i < 184; ++i) {
                upgrades.tick(state, true);
            }
            CHECK(upgrades.active() && scenes.size() == 2);
            CHECK(
                std::strcmp(reinterpret_cast<char*>(memory.data() + name), "Guanto Bomba v3") == 0
            );
            put<int>(d.mode, 0);
            upgrades.tick(state, true);
            upgrades.tick(state, true);
            CHECK(!upgrades.active());
            upgrades.tick(state, true);
            CHECK(scenes.size() == 2);
            // Another mod replacing a localized entry keeps its binding on our unload.
            put<unsigned>(0x500000, 0x520000);
        }
        CHECK(get<unsigned>(0x500000) == 0x520000);
        put<unsigned>(0x500000, 0x510000);
        {
            NativeUpgrades upgrades(&api, d);
            upgrades.tick(state, false);
            CHECK(get<unsigned>(0x500000) != 0x510000);
        }
        CHECK(get<unsigned>(0x500000) == 0x510000);
        {
            NativeUpgrades external(&api, d);
            external.tick(state, false);
            CHECK(!external.requested_presentation_pending());
            CHECK(!external.request_presentation(37));
            const auto archive = select(0x1CA720, 0x1CA4A0) + 18 * 4;
            put<int>(archive, -1);
            CHECK(!external.supports_presentation(18) && !external.request_presentation(18));
            put<int>(archive, 0);
            const auto native_xp = state.experience(18);
            const auto notification_count = notifications;
            const auto scene_count = scenes.size();
            // A separate logical weapon owns its caption; the native transport
            // must not rename it to Walloper v0 or issue the native XP message.
            std::memcpy(memory.data() + 0x520100, "Revolverator v7", 16);
            put<unsigned>(0x500020, 0x520100);
            CHECK(external.request_presentation(18) && external.requested_presentation_pending());
            CHECK(!external.active() && !external.request_presentation(10));
            external.tick(state, true);
            CHECK(external.active() && external.requested_presentation_pending());
            unsigned char report[]{0,0,0,255,0,255};
            external.filter_report(report, 6);
            CHECK(report[0] == 255 && report[1] == 255 && report[2] == 128);
            external.tick(state, true); external.tick(state, true);
            CHECK(scenes.size() == scene_count + 1 && scenes.back() == 18);
            CHECK(get<unsigned short>(0x550034) == 0x41);
            CHECK(get<unsigned>(0x500020) == 0x520100);
            CHECK(state.experience(18) == native_xp && state.weapon_level(18) == 1);
            put<int>(d.mode, 0);
            external.tick(state, true);
            CHECK(external.requested_presentation_pending() && external.active());
            external.tick(state, true);
            CHECK(!external.requested_presentation_pending() && !external.active());
            CHECK(get<unsigned short>(0x550034) == 0x40);
            CHECK(get<unsigned>(0x500020) == 0x520100 && notifications == notification_count);
            CHECK(state.experience(18) == native_xp && state.weapon_level(18) == 1);
        }
    }
    std::puts(
        "localized names, native/external scene lifecycle, no-XP presentation and reversible bindings: ok"
    );
}

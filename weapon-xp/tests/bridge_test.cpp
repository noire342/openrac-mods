// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <cstring>
#include <vector>

#include "check.hpp"
#include "hud_addresses.hpp"
#include "hud_bridge.hpp"

using namespace weaponxp;
std::vector<std::uint8_t> memory(0x02000000);
int loaded = 0;
const Disc* disc;
bool compatible = true;
int a, b;

std::uint32_t identity(std::uint32_t address) {
    return address;
}

int current_level() {
    return loaded;
}

void* function_at(std::uint32_t address) {
    const auto row = (std::strcmp(disc->version, "pal") == 0 ? kHudPal : kHudNtsc)[loaded];
    if (!compatible) {
        return nullptr;
    }
    if (address == row.weapon) {
        return &a;
    }
    if (address == row.relay) {
        return &b;
    }
    return nullptr;
}

void put(std::uint32_t address, std::uint32_t value) {
    std::memcpy(memory.data() + address, &value, 4);
}

std::uint32_t get(std::uint32_t address) {
    std::uint32_t value;
    std::memcpy(&value, memory.data() + address, 4);
    return value;
}

int main() {
    openrac_api api{};
    api.memory = memory.data();
    api.relocate_data = identity;
    api.loaded_level = current_level;
    api.function_at = function_at;
    for (const auto& selected : kDiscs) {
        disc = &selected;
        for (loaded = 0; loaded < 19; ++loaded) {
            const auto row = (std::strcmp(disc->version, "pal") == 0 ? kHudPal : kHudNtsc)[loaded];
            const auto slot = disc->hud_bank;
            put(slot + 0x18, row.weapon);
            put(slot + 0x20, 60015);
            put(slot + 0x68, 1);
            HudBridge bridge(&api, *disc, &a, &b);
            bridge.attach();
            CHECK(get(slot + 0x18) == row.weapon);  // Scheduler owns pending work.
            put(slot + 0x68, 0);
            compatible = false;
            bridge.attach();
            CHECK(get(slot + 0x18) == row.weapon);
            compatible = true;
            bridge.attach();
            CHECK(get(slot + 0x18) == row.relay && bridge.owns(slot));
            bridge.attach();
            CHECK(get(slot + 0x18) == row.relay);
            bridge.detach();
            CHECK(get(slot + 0x18) == row.weapon && !bridge.owns(slot));
            bridge.attach();
            put(slot + 0x18, 0xDEADBEEF);
            bridge.detach();
            CHECK(get(slot + 0x18) == 0xDEADBEEF);  // Never restore over another owner's callback.
            put(slot + 0x18, row.weapon);
            put(slot + 0x20, 60012);
            bridge.attach();
            CHECK(get(slot + 0x18) == row.weapon);  // No inventory gadget HUD is intercepted.
        }
    }
    std::puts(
        "stock SDK HUD relay: all 19 levels, both discs, scheduling, ownership and fail-closed "
        "checks: ok"
    );
}

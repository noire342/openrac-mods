// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hud_bridge.hpp"

#include <cstring>

#include "hud_addresses.hpp"
#include "progression.hpp"

namespace weaponxp {

HudBridge::HudBridge(
    const openrac_api* api, const Disc& disc, void* weapon_draw, void* relay, void* gauge_draw
)
    : m_api(api),
      m_disc(disc),
      m_draw(weapon_draw),
      m_relay(relay),
      m_gauge(gauge_draw) {}

HudBridge::~HudBridge() {
    detach();
}

std::uint32_t HudBridge::read(std::uint32_t address) const {
    std::uint32_t value = 0;
    if (address >= 0x10000 && address <= 0x02000000 - 4) {
        std::memcpy(&value, m_api->memory + address, 4);
    }
    return value;
}

void HudBridge::write(std::uint32_t address, std::uint32_t value) {
    if (address >= 0x10000 && address <= 0x02000000 - 4) {
        std::memcpy(m_api->memory + address, &value, 4);
    }
}

bool HudBridge::owns(std::uint32_t element) const {
    return m_slot != 0 && element == m_slot && m_api->loaded_level() == m_level
           && read(element + 0x18) == m_address;
}

void HudBridge::detach() {
    // A different mod or the game's scheduler may have replaced it since us: preserve that.
    if (m_slot && m_api->loaded_level() == m_level && read(m_slot + 0x18) == m_address) {
        write(m_slot + 0x18, m_original);
    }
    m_slot = m_original = m_address = 0;
    m_level = -1;
}

void HudBridge::attach(int held) {
    const int level = m_api->loaded_level();
    if (m_level != level) {
        detach();
    }
    if (level < 0 || level >= 19) {
        return;
    }
    const bool gauge = held == 9 && m_gauge;
    const auto slot = m_api->relocate_data(m_disc.hud_bank) + (gauge ? 4 * 0x90 : 0);
    if (m_slot && m_slot != slot) {
        detach();
    }
    if (slot < 0x10000 || slot > 0x02000000 - 0x90 || read(slot + 0x68) != 0) {
        return;
    }
    // next_id is the icon definition, 60000 + item, after pending animation has been applied.
    const int item = gauge ? 9 : static_cast<int>(read(slot + 0x20)) - 60000;
    if (weapon_index(item) < 0 || (gauge && read(slot + 0x20) != 0x753F)) {
        return;
    }
    const auto callback = read(slot + 0x18);
    if (!callback) {
        return;
    }
    if (m_slot == slot && callback == m_address) {
        return;
    }
    const auto& addresses = std::strcmp(m_disc.version, "pal") == 0 ? kHudPal : kHudNtsc;
    const auto relay = addresses[level].relay;
    // Check both identities against this port's actual function table, not just known numbers.
    if (m_api->function_at(callback) != (gauge ? m_gauge : m_draw)
        || m_api->function_at(relay) != m_relay) {
        return;
    }
    m_slot = slot;
    m_original = callback;
    m_address = relay;
    m_level = level;
    write(slot + 0x18, relay);
}

void HudBridge::refresh_capacity(int item, int capacity) {
    const auto slot = m_api->relocate_data(m_disc.hud_bank);
    if (slot < 0x10000 || slot > 0x02000000 - 0x90 || read(slot + 0x68) != 0
        || read(slot + 0x20) != static_cast<std::uint32_t>(60000 + item)) {
        return;
    }
    const auto callback = read(slot + 0x18);
    if (owns(slot) || (callback && m_api->function_at(callback) == m_draw)) {
        // The scheduler only copies max ammo when the icon changes. Level-ups must refresh
        // that existing field immediately, including when XP decorations are disabled.
        write(slot + 8, static_cast<std::uint32_t>(capacity));
    }
}

}  // namespace weaponxp

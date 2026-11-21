// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>

namespace revolverator::native_arsenal_layout {
// Canonical NTSC-U addresses. Memory relocates them for the active disc.
inline constexpr unsigned items_owned = 0x13d4c0;
inline constexpr unsigned items_acquired = 0x13d4e8;
inline constexpr unsigned weapon_ammo_counts = 0x13d428;
inline constexpr unsigned item_text_variant = 0x13e520;
inline constexpr unsigned quick_slots = 0x141ea0;
inline constexpr unsigned quick_slot_count = 8;
inline constexpr unsigned hand_selection = 0x141660;
inline constexpr unsigned vendor_stock = 0x15edd0;
inline constexpr unsigned vendor_stock_size = 12;
inline constexpr unsigned vendor_stock_item_mask = 0x3f;

inline constexpr unsigned hero = 0x13f350;
inline constexpr unsigned held_item_state = 0x10b4;
inline constexpr unsigned held_item = 0x10b8;
inline constexpr unsigned hero_moby = 0x2080;
inline constexpr unsigned hero_state = 0x2084;
inline constexpr unsigned pending_hand = 0x20b8;
inline constexpr unsigned hero_selected_item = 0x20d4;
inline constexpr unsigned hero_health = 0x22a8;
inline constexpr int empty_hand_item = 0;
inline constexpr int held_item_ready = 2;
inline constexpr int hero_dying = 0x3d;
inline constexpr int hero_fatal_fall = 0x77;
inline constexpr int hero_dying_zero_health = 0x82;
inline constexpr unsigned game_mode = 0x15f5c4;
inline constexpr int gameplay_mode = 0;
inline constexpr int gadgetron_mode = 5;
inline constexpr int loading_mode = 6;

inline constexpr unsigned moby_size = 0x100;
inline constexpr unsigned moby_state = 0x20;
inline constexpr unsigned moby_class_pointer = 0x24;
inline constexpr unsigned moby_spawn_frame = 0x38;
inline constexpr unsigned moby_pvars = 0x78;
inline constexpr unsigned moby_class = 0xa6;
inline constexpr std::uint8_t moby_unavailable_state = 0x80;
inline constexpr std::uint16_t ratchet_class = 0;

inline constexpr unsigned resident_item_definitions = 0x1863d0;
inline constexpr unsigned level_item_definitions = 0x179ac0;
inline constexpr unsigned item_definition_size = 0x4c;
inline constexpr unsigned item_definition_class = 0x10;
inline constexpr unsigned pickup_plural_text = 0x34;
inline constexpr unsigned pickup_singular_text = 0x36;
inline constexpr unsigned item_icon_bank = 0x38;
inline constexpr unsigned ammo_pickup_class = 0x3a;
inline constexpr unsigned item_definition_halfwords_end = 0x44;

inline constexpr unsigned vendor_item_info = 0x1c40b0;
inline constexpr unsigned vendor_item_info_size = 0x18;
inline constexpr unsigned vendor_weapon_price = 0;
inline constexpr unsigned discounted_weapon_price = 4;
inline constexpr unsigned ammo_unit_price = 0x08;
inline constexpr unsigned challenge_ammo_unit_price = 0x0a;
inline constexpr unsigned ammo_pickup_amount = 0x0c;
inline constexpr unsigned ammo_capacity = 0x0e;
inline constexpr unsigned initial_ammo = 0x12;
inline constexpr unsigned vendor_item_slots = 0x1c8ff0;
inline constexpr unsigned vendor_item_slot_size = 0x40;
inline constexpr unsigned vendor_preview_class = 0x30;

inline constexpr int last_campaign_level = 18;
inline constexpr std::uint16_t revolverator_icon_bank = 60037;
}

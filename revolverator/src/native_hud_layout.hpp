// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>
#include <limits>

namespace revolverator::native_hud {
// NTSC-U layout from rnc/ui/hud/hud_state.h and queue_animation_update.c.
// Memory relocates the two state addresses through the active disc profile.
inline constexpr unsigned resident_state = 0x19a3e8;
inline constexpr unsigned level_state = 0x17e4d8;
inline constexpr unsigned heap_cursor = 0x10;
inline constexpr unsigned heap_end = 0x14;
inline constexpr unsigned bank_header = 0x18;
inline constexpr unsigned animation_definitions = bank_header + sizeof(std::uint32_t);
inline constexpr unsigned frame_references = animation_definitions + sizeof(std::uint32_t);
inline constexpr unsigned image_pages = frame_references + sizeof(std::uint32_t);
inline constexpr unsigned palette_pages = image_pages + sizeof(std::uint32_t);
inline constexpr unsigned table_end = palette_pages + sizeof(std::uint32_t);
inline constexpr unsigned bank_palette_count = 0x24;
inline constexpr unsigned bank_image_count = 0x44;
inline constexpr unsigned bank_header_words = 45; // sizeof(HudTexCounts) / sizeof(s32)

struct AnimationDefinition {
    std::uint16_t id;
    std::uint16_t count;
    std::uint16_t start;
    std::uint8_t flags;
    std::uint8_t padding;
};

struct FrameReference {
    std::int16_t palette_index;
    std::int16_t image_index;
};

struct TexturePage {
    std::uint32_t source_address;
    std::uint16_t gs_block_offset;
    std::uint8_t width_log2;
    std::uint8_t height_log2;
};

static_assert(sizeof(AnimationDefinition) == 8);
static_assert(sizeof(FrameReference) == 4);
static_assert(sizeof(TexturePage) == 8);
inline constexpr unsigned animation_entry_bytes = sizeof(AnimationDefinition);
inline constexpr unsigned frame_reference_bytes = sizeof(FrameReference);
inline constexpr unsigned texture_page_bytes = sizeof(TexturePage);

inline constexpr auto animation_terminator = std::numeric_limits<std::uint16_t>::max();
inline constexpr unsigned max_animation_definitions = 1024;
inline constexpr unsigned max_frame_references = 16000;
inline constexpr unsigned palette_bytes = 256 * sizeof(std::uint32_t); // PSMT8 RGBA CLUT
inline constexpr int max_alpha = 0x80; // GS alpha scale
inline constexpr unsigned native_texture_log2 = 5;

// Weapon HUD callback and animation scheduler fields.
inline constexpr unsigned channel_slots = 0x199b60;
inline constexpr unsigned channel_size = 0x90;
inline constexpr unsigned channel_current_id = 0x00;
inline constexpr unsigned channel_callback = 0x18;
inline constexpr unsigned channel_next_id = 0x20;
inline constexpr unsigned channel_x = 0x50;
inline constexpr unsigned channel_y = 0x54;
inline constexpr unsigned channel_pending = 0x68;
inline constexpr unsigned channel_visibility = 0x70;
inline constexpr unsigned channel_ammunition = 0x74;
inline constexpr unsigned ammo_format = 0x15f7c8;
inline constexpr unsigned ammo_fade_frames = 0x15f91c;

inline constexpr int weapon_hud_bank_base = 60000;
inline constexpr std::uint16_t drill_icon_bank = 59901;
inline constexpr unsigned drill_icon_frame = 13;
inline constexpr unsigned generic_icon_frame_count = 2;
inline constexpr unsigned vendor_ammunition_frame = generic_icon_frame_count;
inline constexpr unsigned ammunition_frame = vendor_ammunition_frame + 1;
inline constexpr unsigned gold_preview_frame = ammunition_frame + 1;
inline constexpr unsigned custom_icon_frame_count = gold_preview_frame + 1;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_empty_weapon_sound.hpp"
#include "native_weapon_alias.hpp"

#include <cstdio>

namespace revolverator {
namespace {
constexpr int bomb_glove_class = 192;
constexpr int empty_sound_entry = 0;
constexpr int one_shot_flags = 0;

constexpr unsigned class_slot_by_id = 0x1b3ac0;
constexpr unsigned class_headers = 0x197300;
constexpr unsigned class_sound_count = 0x0d;
constexpr unsigned class_sound_definitions = 0x28;
constexpr unsigned sound_definition_bytes = 0x20;

constexpr unsigned level_bank_handle = 0x15ed5c;
constexpr unsigned swap_class_count = 0x160008;
constexpr unsigned swap_class_ids = 0x1afbc0;
constexpr unsigned swap_class_sounds = 0x1afe60;
constexpr int maximum_swap_classes = 24;
constexpr unsigned swap_sounds_per_class = 16;
constexpr std::uint16_t no_sound = 0xffff;
constexpr int native_unity_volume = 0x400;
constexpr int stock_near_volume = 2 * native_unity_volume;
}

NativeEmptyWeaponSound::NativeEmptyWeaponSound(const openrac_api* api, openrac_mod* mod)
    : Memory(api),
      mod_(mod),
      play_class_sound_(reinterpret_cast<PlayClassSound>(find_function("FUN_L00_0028dc90"))),
      play_bank_sound_(reinterpret_cast<PlayBankSound>(find_function("FUN_0012e308"))),
      trace_(api->log && api->setting_bool && api->setting_bool(mod, "trace")) {}

void NativeEmptyWeaponSound::play(unsigned emitter) const {
    if (level_transitioning()) return;
    if (!play_for_resident_class(emitter)) play_from_level_bank();
}

bool NativeEmptyWeaponSound::play_for_resident_class(unsigned emitter) const {
    if (!play_class_sound_) return false;
    const auto map = global(class_slot_by_id);
    if (!valid(map, bomb_glove_class + 1)) return false;
    const auto slot = read<std::uint8_t>(map + bomb_glove_class);
    const auto headers = level(class_headers);
    if (slot >= NativeWeaponAlias::slot_capacity ||
        !valid(headers, NativeWeaponAlias::slot_capacity * sizeof(unsigned))) return false;
    const auto header = read<unsigned>(headers + slot * sizeof(unsigned));
    if (!valid(header, class_sound_definitions + sizeof(unsigned)) ||
        read<std::uint8_t>(header + class_sound_count) <= empty_sound_entry ||
        !valid(read<unsigned>(header + class_sound_definitions), sound_definition_bytes)) return false;

    const auto voice = play_class_sound_(empty_sound_entry, one_shot_flags, emitter, bomb_glove_class);
    if (trace_) {
        char message[96];
        std::snprintf(message, sizeof message, "native empty weapon sound route=class voice=%d", voice);
        api_->log(mod_, OPENRAC_LOG_INFO, message);
    }
    return true;
}

void NativeEmptyWeaponSound::play_from_level_bank() const {
    if (!play_bank_sound_) return;
    const int bank = read<int>(global(level_bank_handle));
    const int count = read<int>(level(swap_class_count));
    const auto ids = level(swap_class_ids);
    const auto sounds = level(swap_class_sounds);
    constexpr unsigned row_bytes = swap_sounds_per_class * sizeof(std::uint16_t);
    if (bank <= 0 || count <= 0 || count > maximum_swap_classes ||
        !valid(ids, count * sizeof(int)) || !valid(sounds, count * row_bytes)) return;

    for (int row = 0; row < count; ++row) {
        if (read<int>(ids + row * sizeof(int)) != bomb_glove_class) continue;
        const auto sound = read<std::uint16_t>(sounds + row * row_bytes + empty_sound_entry * sizeof(std::uint16_t));
        if (sound == no_sound) return;

        // The level loader remaps the equipped class and this switchable
        // class table to the same loaded bank. The model can stay unloaded.
        play_bank_sound_(bank, sound, stock_near_volume, 0, 0, 0, 0, nullptr);
        if (trace_) {
            char message[108];
            std::snprintf(message, sizeof message, "native empty weapon sound route=bank bank=%d sound=%u", bank,
                          static_cast<unsigned>(sound));
            api_->log(mod_, OPENRAC_LOG_INFO, message);
        }
        return;
    }
}

}

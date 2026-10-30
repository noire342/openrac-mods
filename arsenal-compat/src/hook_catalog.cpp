// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "provider.hpp"
namespace arsenal_compat {
const std::array<HookSpec, OPENRAC_ARSENAL_REQUIRED_HOOKS>& hook_specs() noexcept {
    static constexpr std::array<HookSpec, OPENRAC_ARSENAL_REQUIRED_HOOKS> specs{{
        {"rac1.menu.item_grid.update","i32(u32)","FUN_0021b858"},
        {"rac1.menu.item_grid.draw","i32(u32)","FUN_0021d948"},
        {"rac1.menu.item_slots.update","i32(u32)","FUN_0021c7a0"},
        {"rac1.menu.quick_select.draw","i32(u32)","FUN_00220b20"},
        {"rac1.menu.quick_select.load","i32(u32)","FUN_0021c9c8"},
        {"rac1.menu.quick_select.store","i32(u32)","FUN_0021ca60"},
        {"rac1.menu.item_text.draw","i32(u32)","FUN_0021a328"},
        {"rac1.menu.item_ammo.draw","i32(i32)","FUN_0021f158"},
        {"rac1.menu.preview.bind","i32(u32)","FUN_0021e230"},
        {"rac1.menu.preview.update","i32(u32)","FUN_0021df98"},
        {"rac1.menu.preview.draw_available","i32(u32)","FUN_0021e608"},
        {"rac1.menu.preview.draw_selected","i32(u32)","FUN_0021e110"},
        {"rac1.menu.equipped_preview.update","i32(u32)","FUN_00224368"},
        {"rac1.menu.equipped_preview.draw","i32(u32)","FUN_0021ddf8"},
        {"rac1.menu.preview.pose","void(u32)","FUN_00224b70"},
        {"rac1.hud.quick_select.init","void(u32)","FUN_L00_00237ae8"},
        {"rac1.hud.quick_select.update","void(u32)","FUN_L00_00237ba0"},
        {"rac1.hud.quick_select.draw","i32(u32)","FUN_L00_002382a0"},
        {"rac1.vendor.menu_update","void()","FUN_L00_0029ba78"},
        {"rac1.vendor.initialize_selected","void()","FUN_L00_0029b680"},
        {"rac1.ammo.choose_drop","i32(u32,u32)","FUN_L00_00257218"},
        {"rac1.ammo.spawn_drop","u32(u32,i32,i32,i32)","FUN_L00_002c52a8"},
        {"rac1.save.prepare","void(i32,i32)","FUN_002269c0"},
        {"rac1.save.request_autosave","i32(i32,i32)","FUN_0020b178"},
        {"rac1.save.restore","void(u32)","FUN_00209298"},
        {"rac1.save.restore_data","i32(u32,i32,u32)","FUN_0020af20"},
        {"rac1.save.new_game","void()","FUN_00209370"},
        {"rac1.save.new_game_plus","void(i32)","FUN_00226b08"},
        {"rac1.save.serialize","void(u32)","FUN_0020abb0"},
        {"rac1.save.update_state","void()","FUN_002093d8"},
    }};
    return specs;
}
} // namespace arsenal_compat

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "provider.hpp"
namespace arsenal_compat {
const std::array<HookSpec, HookCatalogSize>& hook_specs() noexcept {
    static constexpr std::array<HookSpec, HookCatalogSize> specs{{
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
        {"rac1.vendor.icon_strip.draw","void()","FUN_002386e8"},
    }};
    return specs;
}
const std::array<HookSpec, HookCatalogSize>& hook_specs(Region region) noexcept {
    if (region == Region::Ntsc) return hook_specs();
    // OpenRAC 0.10.0 d015ea1c: games/rac1/config/overlays/us_map.tsv and
    // games/rac1/config/versions/us/map.tsv. IDs, signatures and required-prefix
    // order are copied unchanged; only the explicitly paired native names vary.
    static const auto specs = [] {
        auto pal = hook_specs();
        static constexpr std::array<const char*, HookCatalogSize> symbols{{
            // differs: PAL grid/selection layout; consumer must use PAL records.
            "func_0021C840",
            "func_0021E950",
            "func_0021D7A0",
            "func_00221B58",
            "func_0021D9C8",
            "func_0021DA60",
            // differs: localized/configured text layout; keep native drawing.
            "func_0021B298",
            "func_00220160",
            "func_0021F238",
            "func_0021EFA0",
            "func_0021F610",
            "func_0021F118",
            "func_002255F8",
            "func_0021EE00",
            "func_00225E00",
            "func_L00_00238478",
            "func_L00_00238530",
            "func_L00_00238C30",
            // differs: purchase state machine; the native boundary is void().
            "func_L00_0029CDE0",
            "func_L00_0029C9E8",
            "func_L00_00258250",
            "func_L00_002C6608",
            // differs: serialize native PAL packets/options before card IO.
            "func_00227C78",
            // differs: native autosave request/state; preserve its int result.
            "func_0020BFC8",
            "func_00209CE8", // Whole stock options-preserving host wrapper.
            // differs: packet restore; consumer must use the PAL descriptors.
            "func_0020BD70",
            // differs: new-game defaults, whole start-level host wrapper.
            "func_00209DC0",
            // differs: native progression-preserving new-game-plus reset.
            "func_00227DB0",
            "func_0020BA00",
            // differs: native memory-card state machine; no guessed offsets.
            "func_00209E68",
            "func_00239A00", // Optional icon strip: fingerprint match, void().
        }};
        for (std::size_t i = 0; i < pal.size(); ++i) pal[i].symbol = symbols[i];
        return pal;
    }();
    return specs;
}
} // namespace arsenal_compat

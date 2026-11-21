// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "native_rotor.hpp"
#include <array>
#include <cstdint>

namespace revolverator {
class NativeArsenal;
class NativeWeaponAlias;

// Price, transient vendor-list and preview policy. The native game owns its
// saved stock, planet visits, inventory grants, purchases, ammunition and bolts.
class NativeVendor final : public Memory {
public:
    static constexpr int item_id=18;
    static constexpr int first_level=12; // Hoven
    static constexpr int normal_price=15000;
    static constexpr const char* build_hook="rac1.vendor.build_item_list";
    static constexpr const char* build_signature="void(i32)";
    NativeVendor(const openrac_api* api,NativeWeaponAlias& weapon_alias,GuestArena& arena);
    ~NativeVendor();
    NativeVendor(const NativeVendor&)=delete;
    NativeVendor& operator=(const NativeVendor&)=delete;

    // Call after NativeAmmunition.frame and before native level unload.
    void frame();
    void reset();

    // Wrap build_hook: before_build(); original(skip_unflagged);
    // after_build(skip_unflagged). No stock-init or Item_Give hook is required.
    void before_build();
    void after_build(int skip_unflagged);
    bool unlocked() const;
    void set_arsenal(NativeArsenal* arsenal){arsenal_=arsenal;}
    unsigned selected_key()const;
    bool selected_custom()const;
    unsigned weapon_purchase_key(int native_item) const;
    void remember_weapon_purchase(unsigned key);
    bool presenting_custom_purchase() const;
    unsigned purchase_scene_weapon() const;
    bool showing_custom_ammo() const;
    bool has_custom_rows()const;
    unsigned icon_bank_for_row(unsigned row)const;
    void set_preview_class(unsigned klass){preview_class_=klass;}

    // Bracket the native purchase update and selection initialization. frame()
    // also restores the preview actors after the deferred draw.
    void before_vendor_update();
    void after_vendor_update();

private:
    struct Row {
        std::int32_t item=0,is_ammo=0,state=0,unused=0,sound=0;
    };
    static_assert(sizeof(Row)==20,"native vendor row layout");
    struct MenuPatch {
        unsigned rows=0,selection=0;
        std::array<Row,16> original{},applied{};
        int original_count=0,applied_count=0;
        int original_selection=0,applied_selection=0;
        bool active=false;
    } menu_;
    struct WeaponPreview {
        NativeRotor pose;
        unsigned actor=0;
        WeaponPreview(const openrac_api* api,GuestArena& arena):pose(api,arena,false,true){}
    };
    NativeWeaponAlias& weapon_alias_;
    std::array<WeaponPreview,2> weapon_previews_;
    int level_=-1;
    NativeArsenal* arsenal_{};
    unsigned build_previous_=18;
    bool rebuild_selected_custom_=false;
    bool purchased_custom_=false;
    static constexpr int custom_row_marker=0x52564f31;
    static bool custom_row(const Row& row){return row.item==item_id && row.unused==custom_row_marker;}
    unsigned price_address_=0;
    std::int32_t original_price_=0;
    bool price_owned_=false;
    unsigned preview_model_address_=0;
    std::int32_t original_preview_model_=0;
    bool preview_model_owned_=false;
    unsigned preview_class_=225,applied_preview_class_=225;
    struct PreviewLease {
        unsigned address=0,pclass=0,pvars=0,stamp=0;
        std::uint16_t original_class=0;
        float original_scale=0,applied_scale=0;
        std::array<float,3> original_position{},applied_position{};
        bool fitted=false;
    };
    std::array<PreviewLease,2> previews_{};
    unsigned native_icon_bank_{};
    bool native_icon_captured_=false;

    bool supported() const;
    void restore_menu();
    bool preview_valid(const PreviewLease& preview) const;
};
}

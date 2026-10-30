// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <array>
#include <cstdint>

namespace revolverator {

// Price, transient vendor-list and ammunition-preview policy. The native game owns its
// saved stock, planet visits, inventory grants, purchases, ammunition and bolts.
class NativeVendor final : public Memory {
public:
    static constexpr int item_id=18;
    static constexpr int first_level=12; // Hoven
    static constexpr int normal_price=15000;
    static constexpr const char* build_hook="rac1.vendor.build_item_list";
    static constexpr const char* build_signature="void(i32)";
    static constexpr const char* preview_hook="rac1.vendor.render_screens";
    static constexpr const char* preview_signature="void(u32)";

    explicit NativeVendor(const openrac_api* api);
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
    void set_preview_class(unsigned klass){preview_class_=klass;}

    // Bracket preview_hook with these calls. frame() restores the temporary
    // render class after the deferred draw, before the next native menu update.
    // Class11's world/entity update is not the open vendor menu's update.
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
    int level_=-1;
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
    } preview_;

    bool supported() const;
    void restore_menu();
    bool preview_valid() const;
};
}

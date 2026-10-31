// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <cstdint>
#include <vector>

namespace revolverator {

// The logical Revolverator uses the Walloper's native actor/animation carrier,
// but has a distinct renderer identity. It must never replace class180's art or
// be written into an item definition's compressed-resource class field: native
// resource streaming has no WAD for the alias. Only an owned held Moby is given
// the render ID after its native update and before the port captures its draw.
class NativeWeaponAlias final:public Memory {
public:
    static constexpr unsigned source_class=180;
    static constexpr unsigned native_class=2017;
    static constexpr unsigned golden_class=2018;
    static constexpr unsigned id_capacity=0x800;
    static constexpr unsigned slot_capacity=224;
    explicit NativeWeaponAlias(const openrac_api* api):Memory(api){}
    ~NativeWeaponAlias(){reset();}
    NativeWeaponAlias(const NativeWeaponAlias&)=delete;
    NativeWeaponAlias& operator=(const NativeWeaponAlias&)=delete;

    // Call once at the start of mods::frame, after the previous native draw was
    // captured and before the next native update. This returns the held actor
    // to class180 and maintains the separate class-id mapping.
    void frame();
    // actual_held must be hero.items[0].moby. The caller decides the logical
    // identity; an ordinary Walloper must always pass revolverator=false.
    bool apply(unsigned actual_held,bool revolverator,bool golden=false);
    // A menu preview uses the same safe post-update render projection, but has
    // an independent lease: it must not replace the actual held actor's lease.
    bool apply_preview(unsigned preview,bool revolverator,bool golden=false);
    bool fit_preview(unsigned preview,float factor);
    bool ready()const;
    bool owns(unsigned moby)const;
    void reset();
private:
    struct Visual {unsigned address{},pclass{},pvars{},stamp{};float original_scale{},applied_scale{};bool scaled=false;unsigned render_class=native_class;} visual_;
    std::vector<Visual> previews_;
    int level_=-1;
    unsigned map_{};
    std::uint8_t slot_=0xff;
    bool mapping_owned_=false;
    bool supported()const;
    bool source_valid(unsigned map,std::uint8_t slot)const;
    bool visual_valid(const Visual& visual)const;
    void restore_visual(Visual& visual);
    void restore_previews();
    bool ensure_mapping();
    bool apply_visual(Visual& lease,unsigned subject,unsigned render_class);
};
}

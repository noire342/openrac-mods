// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_state.hpp"
#include "arsenal_menu_model.hpp"
#include "arsenal_identity_view.hpp"
#include "memory.hpp"
#include <vector>

namespace revolverator {

// Projects one logical weapon into the game's existing Walloper transport.
// Every native index remains 18. Logical ownership, ammunition and favourites
// remain independent; native save scopes always select the original Walloper.
class NativeArsenal final:public Memory,public ArsenalMenuModel {
public:
    NativeArsenal(const openrac_api* api,arsenal::Registry& registry,ArsenalIdentityView& presentation);
    arsenal::Registry& registry()override{return registry_;}
    void project(arsenal::WeaponKey key)override;
    void commit()override;
    void restore_gameplay()override;
    arsenal::WeaponKey projected()const{return projected_;}
    bool revolverator_active()const;
    bool equipped_revolverator()const;
    bool ready()const{return ready_;}
    void begin_level();
    void finish_level();
    void reset();
    void import_native_inventory();
    void mark_snapshot_restored(){initialized_=true;}
    void project_quick_slots(bool native_save=false);
    void observe_equipped();
    arsenal::WalloperNativeState native_walloper()const;
private:
    struct Table {
        unsigned address{};
        std::vector<unsigned char> original,custom,mask,last;
    };
    arsenal::Registry& registry_;
    ArsenalIdentityView& presentation_;
    std::vector<Table> tables_;
    arsenal::WeaponKey projected_=arsenal::NativeWalloperKey;
    bool ready_=false,initialized_=false;
    int level_=-1;
    void capture_table(unsigned address,unsigned size);
    void apply_tables(bool custom);
    void apply_state(arsenal::WeaponKey key);
    void write_walloper(const arsenal::WalloperNativeState& state);
};

class ArsenalScope final {
public:
    ArsenalScope(NativeArsenal& arsenal,arsenal::WeaponKey key):arsenal_(arsenal),previous_(arsenal.projected()){arsenal_.project(key);}
    ~ArsenalScope(){arsenal_.project(previous_);}
    ArsenalScope(const ArsenalScope&)=delete;
    ArsenalScope& operator=(const ArsenalScope&)=delete;
private:
    NativeArsenal& arsenal_;
    arsenal::WeaponKey previous_;
};
}

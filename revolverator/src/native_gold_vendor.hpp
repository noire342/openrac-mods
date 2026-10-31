// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_state.hpp"
#include "memory.hpp"
#include <array>

namespace revolverator {
class NativeWeaponAlias;
// An additional real Novalis Gold-shop case. It owns only its dynamically
// allocated actors and uses the original NPC prompt channel/style. Foreign
// item_offer actors must continue through the original SDK callback.
class NativeGoldVendor final:public Memory {
public:
    static constexpr unsigned price=50000,case_class=304,display_class=2019;
    static constexpr const char* hook="rac1.moby.item_offer.update";
    static constexpr const char* signature="void(u32)";
    NativeGoldVendor(const openrac_api*,openrac_mod*,arsenal::Registry&,NativeWeaponAlias&);
    ~NativeGoldVendor();
    NativeGoldVendor(const NativeGoldVendor&)=delete;
    NativeGoldVendor& operator=(const NativeGoldVendor&)=delete;
    void frame();
    void reset();
    bool update(unsigned moby);
    bool poll_purchase();
    unsigned dome_moby()const;
    unsigned preview_moby()const;
    bool ready()const;
private:
    struct Actor {unsigned address{},pclass{},pvars{},stamp{};};
    arsenal::Registry& registry_;
    openrac_mod* mod_{};
    Actor dome_{},preview_{};
    int level_=-1,prompt_=-1;
    unsigned retry_{},frames_{};
    bool bought_=false,reported_=false,functions_ready_=false;
    unsigned display_map_{};
    unsigned char display_slot_=0xff;
    bool owns_display_map_=false;
    using Create=unsigned(*)(int);
    using Remove=void(*)(unsigned);
    using Basis=void(*)(unsigned,unsigned);
    using Queue=int(*)(int,int,unsigned,unsigned,unsigned,unsigned,int);
    using Keep=int(*)(int,int);
    using Autosave=int(*)(int,int);
    Create create_{};Remove remove_{};Basis basis_{};
    Queue queue_{};Keep keep_{};Autosave autosave_{};
    bool supported()const;
    bool accessible()const;
    bool valid_actor(const Actor&,bool dome)const;
    bool resource_available(unsigned klass)const;
    bool display_mapping();
    bool bind_functions();
    bool inherit_lighting(Actor&,bool dome);
    Actor spawn(unsigned klass,const std::array<float,4>& position,float yaw,bool dome);
    void retire(Actor&,bool dome);
    bool player_near()const;
    void prompt(const char* text);
};
}

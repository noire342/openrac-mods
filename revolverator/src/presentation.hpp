// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include "memory.hpp"
#include "hd_icon.hpp"
#include "arsenal_identity_view.hpp"
#include "arsenal_state.hpp"
#include "native_arsenal_layout.hpp"
#include "weapon_xp_api.h"
#include <array>
#include <optional>
#include <vector>
namespace revolverator {
class Presentation:public Memory,public ArsenalIdentityView {
public:
 Presentation(const openrac_api* api,openrac_mod* mod,GuestArena* arena=nullptr,bool hud_fallback=false);
 ~Presentation();
 void names();
 void select_identity(bool revolverator)override;
 void set_experience(bool enabled,unsigned level,float progress,bool versioned_names);
 using NameProvider=bool(*)(void*,unsigned,int,const char*,char*,unsigned);
 void set_name_provider(NameProvider provider,void* user){
  name_provider_=provider;name_user_=user;name_cache_.reset();
 }
 // Uses the game's existing acquisition subtitle; level0 selects current XP.
 bool notify_acquisition(unsigned level=0);
 void set_golden(bool golden){golden_identity_=golden;if(level_transitioning())return;hd_.set_golden(golden);apply_identity();}
 static constexpr unsigned custom_icon_bank=native_arsenal_layout::revolverator_icon_bank;
 void frame(bool equipped,bool alive=true);
 bool owns(unsigned element)const;
 struct AmmoCounter {
  unsigned rounds;
  unsigned capacity;
 };
 void draw_base(unsigned element,void(*previous_relay)(unsigned),std::optional<AmmoCounter> vendor_ammo={});
 void after_hud(unsigned element,const Controller& controller);
 int draw_quick_selector(unsigned element,int(*original)(unsigned),const std::array<arsenal::WeaponKey,8>& bindings);
 void reset();
private:
 HdIcon hd_;
 using Hud=int(*)(unsigned);
 using Rectangle=void(*)(int,int,int,int,std::uint64_t,int);
 using Quad=void(*)(int,int,int,int,int,int,int,int,std::uint64_t,std::uint64_t);
 using Texture=std::uint64_t(*)(int);
 Hud hud_;Rectangle rectangle_;Quad quad_;Texture texture_;void* relay_;
 bool enabled_;int level_=-1;unsigned slot_{},old_callback_{},callback_{},arena_{},bank_{},table_{};
 bool revolverator_identity_=true;
 bool golden_identity_=false;
 bool experience_enabled_=false,versioned_names_=false;
 unsigned experience_level_=1;
 float experience_progress_=0;
 NameProvider name_provider_{};
 void* name_user_{};
 struct CachedName{
  unsigned level;
  int language;
  bool golden;
  std::array<char,WEAPON_XP_NAME_MAX_BYTES> replacement{};
 };
 std::optional<CachedName> name_cache_;
 unsigned short old_hud_flag_{};
 char format_{};bool format_changed_{};
 struct Binding{unsigned entry,original,replacement;};std::vector<Binding> bindings_;
 unsigned icon_state_{},icon_defs_{},icon_refs_{},icon_copy_{};
 unsigned icon_capacity_{},spare_icons_{},spare_icon_capacity_{};
 std::vector<unsigned> icon_baseline_;
 unsigned icon_defs_copy_{},icon_defs_count_{};
 std::vector<std::uint64_t> icon_definitions_baseline_;
 void apply_identity();
 void format_name(char* name,unsigned capacity,unsigned version,int language);
 void bind_icon();
 void reset_icons(bool reuse_storage);
 void detach_ammo_hud();
 void rect(int x,int y,int w,int h,unsigned color)const;
 void text(int x,int y,const char* s,unsigned color,int pixels=10)const;
};
}

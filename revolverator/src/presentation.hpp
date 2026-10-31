// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include "memory.hpp"
#include "hd_icon.hpp"
#include "arsenal_identity_view.hpp"
#include "arsenal_state.hpp"
#include <array>
#include <vector>
namespace revolverator {
class Presentation:public Memory,public ArsenalIdentityView {
public:
 Presentation(const openrac_api* api,openrac_mod* mod,GuestArena* arena=nullptr,bool hud_fallback=false);
 ~Presentation();
 void names();
 void select_identity(bool revolverator)override;
 void set_experience(bool enabled,unsigned level,float progress,bool versioned_names);
 void set_golden(bool golden){golden_identity_=golden;hd_.set_golden(golden);apply_identity();}
 static constexpr unsigned custom_icon_bank=60037;
 void frame(bool equipped);
 bool owns(unsigned element)const;
 void before_hud(unsigned element);
 void draw_base(unsigned element,void(*previous_relay)(unsigned));
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
 unsigned short old_hud_flag_{};
 char format_{};bool format_changed_{};
 struct Binding{unsigned entry,original,replacement;};std::vector<Binding> bindings_;
 unsigned icon_state_{},icon_defs_{},icon_refs_{},icon_copy_{};
 std::vector<unsigned> icon_baseline_;
 unsigned icon_defs_copy_{},icon_defs_count_{};
 std::vector<std::uint64_t> icon_definitions_baseline_;
 void apply_identity();
 void bind_icon();
 void rect(int x,int y,int w,int h,unsigned color)const;
 void text(int x,int y,const char* s,unsigned color,int pixels=10)const;
};
}

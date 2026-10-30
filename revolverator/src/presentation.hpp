// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include "memory.hpp"
#include "hd_icon.hpp"
#include <array>
#include <vector>
namespace revolverator {
class Presentation:public Memory {
public:
 Presentation(const openrac_api* api,openrac_mod* mod,GuestArena* arena=nullptr,bool hud_fallback=false);
 ~Presentation();
 void names();
 void frame(bool equipped);
 bool owns(unsigned element)const;
 void before_hud(unsigned element);
 void draw_base(unsigned element,void(*previous_relay)(unsigned));
 void after_hud(unsigned element,const Controller& controller);
 void reset();
private:
 HdIcon hd_;
 using Hud=int(*)(unsigned);
 using Rectangle=void(*)(int,int,int,int,std::uint64_t,int);
 using Quad=void(*)(int,int,int,int,int,int,int,int,std::uint64_t,std::uint64_t);
 using Texture=std::uint64_t(*)(int);
 Hud hud_;Rectangle rectangle_;Quad quad_;Texture texture_;void* relay_;
 bool enabled_;int level_=-1;unsigned slot_{},old_callback_{},callback_{},arena_{},bank_{},table_{};
 unsigned short old_hud_flag_{};
 char format_{};bool format_changed_{};
 struct Binding{unsigned entry,original,replacement;};std::vector<Binding> bindings_;
 unsigned icon_state_{},icon_defs_{},icon_refs_{},icon_copy_{};
 std::vector<unsigned> icon_baseline_;
 unsigned short icon_count_{},icon_start_{};
 void bind_icon();
 void rect(int x,int y,int w,int h,unsigned color)const;
 void text(int x,int y,const char* s,unsigned color,int pixels=10)const;
};
}

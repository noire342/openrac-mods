// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openrac/openrac_mod.h>
#include <cstdint>
namespace arsenal::native {
// Exact stock render_localized_ui_entry_list preamble (PAL pause.c1640).
// Configure the capture target's text state before its independent GIF paging.
inline bool prepare_capture_text(const openrac_api* api){
 using Register=void(*)(int,std::uint64_t);const auto write=reinterpret_cast<Register>(api->function("func_00234C98"));if(!write)return false;write(0x42,0x44);write(0x47,0x2004b);return true;
}
// DrawDebugProfiler sets this same state immediately before update_hud. Reset
// registers only: the frame's texture paging is already active at that point.
inline bool prepare_hud_text(const openrac_api* api){using State=void(*)();const auto state=reinterpret_cast<State>(api->function("func_00234F40"));if(!state)return false;state();return true;}
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <array>
#include <vector>

namespace revolverator {
class GuestArena;
// Indexed PSMT8 data and a 256-entry RGBA CSM1 palette. The build tool creates
// these locally from the player's original icon; no game artwork is embedded
// in this adapter. Index zero must be transparent. Alpha uses the PS2 0..128
// scale. Texture resolution and logical on-screen dimensions are independent.
struct HdIconData {
 const std::uint8_t* pixels{};
 std::size_t pixel_bytes{};
 const std::uint8_t* palette{};
 std::size_t palette_bytes{};
};

class HdIcon:public Memory {
public:
 explicit HdIcon(const openrac_api* api,GuestArena* arena=nullptr);
 ~HdIcon();
 HdIcon(const HdIcon&)=delete;
 HdIcon& operator=(const HdIcon&)=delete;
 // first_frame/count must refer to the Presentation-owned, appended references,
 // never to the shared bank 59901 references. Existing icon pages stay intact.
 bool install(unsigned first_frame,unsigned count,const HdIconData& data);
 bool ready()const;
 void frame();
 // Bracket only the native ammo HUD callback. Its texture-derived size stays
 // 32x32 while its icon is hidden. Menus continue to use the HD references.
 bool before_base();
 void after_base();
 // Same native HUD sprite renderer, full 128x128 sampling, logical 32x32.
 bool draw(int x,int y,int alpha);
 void reset();
 static constexpr unsigned texture_size=128,logical_size=32;
private:
 using Draw=void(*)(int,int,int,int,int,int);
 Draw draw_;
 int level_=-1;
 unsigned state_{},refs_{},first_{},count_{};
 unsigned images_{},palettes_{},image_copy_{},palette_copy_{};
 unsigned image_count_{},palette_count_{},hd_page_{},blank_page_{},palette_page_{};
 unsigned hd_ref_{},blank_ref_{};
 bool hidden_{};
 std::array<unsigned,4> old_refs_{};
 std::vector<std::uint64_t> old_images_,old_palettes_;
 bool owns_tables()const;
 bool owns_refs()const;
 void merge_pages(unsigned original,unsigned copy,const std::vector<std::uint64_t>& baseline);
};
}

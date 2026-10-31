// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hd_icon.hpp"
#include "native_arena.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using revolverator::HdIcon;
unsigned checks{};
#define CHECK(expression) do{++checks;if(!(expression)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#expression);std::exit(1);}}while(0)

namespace {
constexpr unsigned state=0x19a3e8,defs=0x300000,refs=0x301000,images=0x302000,palettes=0x303000,header=0x304000;
struct Fixture;
Fixture* fixture;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000,0);
 std::array<std::uint8_t,16384> pixels{};
 std::array<std::uint8_t,1024> palette{};
 std::array<std::uint8_t,1024> hud_pixels{},vendor_pixels{},hud_palette{},vendor_palette{},gold_palette{};
 openrac_api api{};
 unsigned cursor=0x600000;
 int level=1,allocations=0,draws=0,last_frame=-1,last_x=0,last_y=0,last_w=0,last_h=0,last_alpha=0;
 bool fail_allocate=false;
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 Fixture(){
  fixture=this;api.memory=ram.data();api.loaded_level=+[]{return fixture->level;};
  api.relocate_data=+[](unsigned at){return at;};
  api.function=+[](const char* name)->void*{
   if(std::strcmp(name,"FUN_001ff288")==0)return reinterpret_cast<void*>(+[](unsigned bytes)->unsigned{
    auto& f=*fixture;++f.allocations;if(f.fail_allocate)return 0;
    const unsigned result=f.cursor;f.cursor=(f.cursor+bytes+63)&~63u;return result;
   });
   if(std::strcmp(name,"FUN_001ffc30")==0)return reinterpret_cast<void*>(+[](int id,int x,int y,int width,int height,int alpha){
    auto& f=*fixture;++f.draws;f.last_frame=id;f.last_x=x;f.last_y=y;f.last_w=width;f.last_h=height;f.last_alpha=alpha;
   });return nullptr;
  };
  put(state+0x1c,defs);put(state+0x20,refs);put(state+0x24,images);put(state+0x28,palettes);
  put(state+0x18,header);put<unsigned>(header+0x44,2);put<unsigned>(header+0x24,2);
  put<std::uint16_t>(defs,59901);put<std::uint16_t>(defs+2,6);put<std::uint16_t>(defs+4,0);
  put<std::uint16_t>(defs+8,60018);put<std::uint16_t>(defs+10,4);put<std::uint16_t>(defs+12,6);
  put<std::uint16_t>(defs+16,0xffff);
  for(unsigned i=0;i<10;++i)put(refs+i*4,0x00010001u);
  for(unsigned i=0;i<2;++i){put(images+i*8,0x400000+i*1024);put<std::uint16_t>(images+i*8+6,0x0505);put(palettes+i*8,0x410000+i*1024);}
  pixels.fill(1);palette[7]=128;
  hud_pixels.fill(2);vendor_pixels.fill(3);hud_palette[11]=128;vendor_palette[15]=128;
  gold_palette[4]=224;gold_palette[5]=176;gold_palette[6]=64;gold_palette[7]=128;
 }
 revolverator::HdIconData data()const{return {pixels.data(),pixels.size(),palette.data(),palette.size()};}
 revolverator::HdIconData ammunition()const{return {hud_pixels.data(),hud_pixels.size(),hud_palette.data(),hud_palette.size()};}
 revolverator::HdIconData vendor_ammo()const{return {vendor_pixels.data(),vendor_pixels.size(),vendor_palette.data(),vendor_palette.size()};}
 revolverator::HdIconData golden()const{return {pixels.data(),pixels.size(),gold_palette.data(),gold_palette.size()};}
};
}

int main(){
 {
  Fixture f;revolverator::GuestArena arena;CHECK(arena.reserve(0x600000,0x10000)==0x610000);
  f.fail_allocate=true;HdIcon icon(&f.api,&arena);
  CHECK(icon.install(6,4,f.data())&&icon.ready()&&f.allocations==0&&arena.used()>16384);
  const unsigned used=arena.used(),table=f.get<unsigned>(state+0x24);
  CHECK(arena.owns(table,8)&&icon.install(6,4,f.data())&&arena.used()==used);
  icon.reset();CHECK(f.get<unsigned>(state+0x24)==images&&arena.used()==used);
  // The session reclaims the reservation only after all table leases reset.
  arena.reset();CHECK(!arena.owns(table,8));
 }
 {
  Fixture f;revolverator::GuestArena arena;HdIcon icon(&f.api,&arena);
  // An absent/exhausted session reservation must not consume the game's HUD heap.
  CHECK(!icon.install(6,4,f.data())&&f.allocations==0&&f.get<unsigned>(state+0x24)==images);
  CHECK(arena.reserve(0x600000,1024)==0x600400);
  CHECK(!icon.install(6,4,f.data())&&f.allocations==0&&arena.used()==0);
 }
 {
  Fixture f;HdIcon icon(&f.api);CHECK(icon.install(6,4,f.data())&&icon.ready());
  const unsigned image_copy=f.get<unsigned>(state+0x24),palette_copy=f.get<unsigned>(state+0x28);
  CHECK(image_copy!=images&&palette_copy!=palettes&&f.allocations==1);
  const auto header_copy=f.get<unsigned>(state+0x18);CHECK(header_copy!=header);
  CHECK(f.get<unsigned>(header_copy+0x44)==4&&f.get<unsigned>(header_copy+0x24)==3);
  CHECK(f.get<unsigned>(header+0x44)==2&&f.get<unsigned>(header+0x24)==2);
  CHECK(f.get<unsigned>(refs)==0x00010001&&f.get<unsigned>(refs+6*4)==0x00020002);
  CHECK(f.get<std::uint16_t>(image_copy+2*8+6)==0x0707);
  CHECK(icon.install(6,4,f.data())&&f.allocations==1);
  CHECK(icon.before_base()&&f.get<unsigned>(refs+6*4)==0x00030002);
  CHECK(f.get<std::uint16_t>(image_copy+3*8+6)==0x0505);
  CHECK(icon.before_base());icon.after_base();CHECK(f.get<unsigned>(refs+6*4)==0x00020002);
  CHECK(icon.draw(23,19,89)&&f.draws==1&&f.last_frame==6&&f.last_x==23&&f.last_y==19);
  CHECK(f.last_w==32&&f.last_h==32&&f.last_alpha==89);
  CHECK(icon.draw(23,19,255)&&f.last_alpha==128);CHECK(!icon.draw(0,0,0)&&f.draws==2);
  f.put<std::uint16_t>(image_copy+2*8+4,123);f.put<std::uint16_t>(palette_copy+2*8+4,124);
  icon.frame();CHECK(f.get<std::uint16_t>(image_copy+2*8+4)==0&&f.get<std::uint16_t>(palette_copy+2*8+4)==0);
  f.put(image_copy,0x444400u); // Another owner's edit through the active table.
  f.put(palettes,0x555500u); // An edit in the original table wins over the clone.
  CHECK(icon.before_base());icon.reset();
  CHECK(f.get<unsigned>(state+0x24)==images&&f.get<unsigned>(state+0x28)==palettes);
  CHECK(f.get<unsigned>(state+0x18)==header&&f.get<unsigned>(header+0x44)==2&&f.get<unsigned>(header+0x24)==2);
  CHECK(f.get<unsigned>(images)==0x444400&&f.get<unsigned>(palettes)==0x555500);
  CHECK(f.get<unsigned>(refs+6*4)==0x00010001&&!icon.ready());
 }
 {
  Fixture f;HdIcon icon(&f.api);f.fail_allocate=true;
  CHECK(!icon.install(6,4,f.data())&&!icon.before_base()&&!icon.draw(0,0,128));
  CHECK(f.get<unsigned>(state+0x24)==images&&f.get<unsigned>(refs+6*4)==0x00010001);
  f.fail_allocate=false;CHECK(icon.install(6,4,f.data()));
  // Respect a later editor's replacement of one owned frame.
  f.put(refs+6*4,0x00000000u);CHECK(!icon.ready()&&!icon.before_base());
  CHECK(f.get<unsigned>(refs+6*4)==0x00000000&&f.get<unsigned>(refs+7*4)==0x00010001);
  CHECK(f.get<unsigned>(state+0x24)==images);icon.reset();
 }
 {
  Fixture f;HdIcon icon(&f.api);auto bad=f.data();bad.pixel_bytes=2048;
  CHECK(!icon.install(6,4,bad)&&!icon.install(6,5,f.data())&&!icon.install(9,4,f.data()));
  f.palette[3]=128;CHECK(!icon.install(6,4,f.data())&&f.allocations==0);
 }
 {
  Fixture f;HdIcon icon(&f.api);CHECK(icon.install(6,4,f.data()));
  const unsigned clone=f.get<unsigned>(state+0x24);
  f.put(state+0x20,0x500000u);icon.reset();
  // Foreign reference tables may still point at our appended pages.
  CHECK(f.get<unsigned>(state+0x24)==clone&&f.get<unsigned>(state+0x20)==0x500000);
 }
 {
  Fixture f;HdIcon icon(&f.api);CHECK(icon.install(6,4,f.data()));
  f.level=2;f.put(state+0x24,0x500000u);f.put(refs+6*4,88u);
  CHECK(!icon.ready());icon.reset();CHECK(f.get<unsigned>(state+0x24)==0x500000&&f.get<unsigned>(refs+6*4)==88);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto ammunition=f.data();ammunition.pixel_bytes=32*32;
  CHECK(icon.install(5,5,f.data(),&ammunition)&&icon.ready());
  const auto header_copy=f.get<unsigned>(state+0x18),image_copy=f.get<unsigned>(state+0x24);
  CHECK(f.get<unsigned>(header_copy+0x44)==5&&f.get<unsigned>(header_copy+0x24)==4);
  const auto drill=f.get<unsigned>(refs+5*4),ammo=f.get<unsigned>(refs+8*4);
  CHECK(drill!=ammo&&f.get<std::uint16_t>(image_copy+unsigned(ammo>>16)*8+6)==0x0505);
  CHECK(icon.before_base()&&f.get<unsigned>(refs+8*4)==ammo&&f.get<unsigned>(refs+5*4)!=drill);
  icon.after_base();CHECK(icon.ready()&&f.get<unsigned>(refs+8*4)==ammo&&f.get<unsigned>(refs+5*4)==drill);
  icon.reset();CHECK(f.get<unsigned>(state+0x18)==header&&f.get<unsigned>(refs+8*4)==0x00010001);
  auto native_drill=f.data();native_drill.pixel_bytes=32*32;
  CHECK(icon.install(5,5,native_drill,&ammunition));
 }
 {
  Fixture f;revolverator::GuestArena no_tail;auto native=f.data();native.pixel_bytes=32*32;
  f.put<unsigned>(state+0x10,0x600000);f.put<unsigned>(state+0x14,0x610000);
  HdIcon unavailable(&f.api,&no_tail);CHECK(!unavailable.install(5,5,native,&native)&&f.allocations==0);
  HdIcon compact(&f.api,&no_tail,true);
  CHECK(!compact.install(5,5,f.data(),&native)&&f.allocations==0); // HD never takes the bounded native pool.
  CHECK(compact.install(5,5,native,&native)&&compact.ready()&&f.allocations==1);
  CHECK(f.get<unsigned>(refs+8*4)!=f.get<unsigned>(refs+5*4));
 }
 {
  Fixture f;revolverator::GuestArena no_tail;auto native=f.data();native.pixel_bytes=32*32;
  f.put<unsigned>(state+0x10,0x600000);f.put<unsigned>(state+0x14,0x600400);
  HdIcon compact(&f.api,&no_tail,true);
  CHECK(!compact.install(5,5,native,&native)&&f.allocations==0&&f.get<unsigned>(state+0x18)==header);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto hud=f.ammunition(),vendor=f.vendor_ammo(),gold=f.golden();
  CHECK(icon.install(5,5,f.data(),&hud,&vendor,&gold)&&icon.ready());
  const unsigned image_copy=f.get<unsigned>(state+0x24),palette_copy=f.get<unsigned>(state+0x28),header_copy=f.get<unsigned>(state+0x18);
  const auto normal=f.get<unsigned>(refs+5*4),ammo=f.get<unsigned>(refs+7*4),cartridge=f.get<unsigned>(refs+8*4),golden=f.get<unsigned>(refs+9*4);
  CHECK(normal==f.get<unsigned>(refs+6*4));
  CHECK(normal!=ammo&&normal!=cartridge&&normal!=golden&&ammo!=cartridge&&ammo!=golden&&cartridge!=golden);
  CHECK((normal>>16)==(golden>>16)&&(normal&0xffff)!=(golden&0xffff));
  CHECK(f.get<unsigned>(header_copy+0x44)==6&&f.get<unsigned>(header_copy+0x24)==6);
  CHECK(f.get<unsigned>(header+0x44)==2&&f.get<unsigned>(header+0x24)==2);
  CHECK(f.get<std::uint16_t>(image_copy+(normal>>16)*8+6)==0x0707);
  CHECK(f.get<std::uint16_t>(image_copy+(ammo>>16)*8+6)==0x0505&&f.get<std::uint16_t>(image_copy+(cartridge>>16)*8+6)==0x0505);
  CHECK(std::memcmp(f.ram.data()+f.get<unsigned>(image_copy+(ammo>>16)*8),vendor.pixels,1024)==0);
  CHECK(std::memcmp(f.ram.data()+f.get<unsigned>(image_copy+(cartridge>>16)*8),hud.pixels,1024)==0);
  CHECK(std::memcmp(f.ram.data()+f.get<unsigned>(palette_copy+(golden&0xffff)*8),gold.palette,1024)==0);
  for(unsigned i=0;i<5;++i)CHECK(f.get<unsigned>(refs+i*4)==0x00010001);
  icon.set_golden(true);CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==golden&&f.get<unsigned>(refs+6*4)==golden);
  CHECK(f.get<unsigned>(refs+7*4)==ammo&&f.get<unsigned>(refs+8*4)==cartridge&&f.get<unsigned>(refs+9*4)==golden);
  CHECK(icon.before_base()&&icon.ready());
  const auto blank=f.get<unsigned>(refs+5*4);CHECK(blank!=normal&&blank!=golden);
  CHECK(f.get<unsigned>(refs+7*4)==blank&&f.get<unsigned>(refs+8*4)==cartridge&&f.get<unsigned>(refs+9*4)==blank);
  icon.set_golden(false);CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==blank);
  icon.after_base();CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==normal&&f.get<unsigned>(refs+6*4)==normal);
  CHECK(f.get<unsigned>(refs+7*4)==ammo&&f.get<unsigned>(refs+8*4)==cartridge&&f.get<unsigned>(refs+9*4)==golden);
  CHECK(icon.before_base());icon.set_golden(true);icon.after_base();
  CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==golden&&f.get<unsigned>(refs+9*4)==golden);
  icon.set_golden(false);CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==normal&&f.get<unsigned>(refs+9*4)==golden);
  for(unsigned i=2;i<6;++i){f.put<std::uint16_t>(image_copy+i*8+4,121);f.put<std::uint16_t>(palette_copy+i*8+4,122);}
  icon.frame();
  for(unsigned i=2;i<6;++i)CHECK(f.get<std::uint16_t>(image_copy+i*8+4)==0&&f.get<std::uint16_t>(palette_copy+i*8+4)==0);
  CHECK(icon.before_base());icon.reset();
  for(unsigned i=0;i<10;++i)CHECK(f.get<unsigned>(refs+i*4)==0x00010001);
  CHECK(f.get<unsigned>(state+0x24)==images&&f.get<unsigned>(state+0x28)==palettes&&f.get<unsigned>(state+0x18)==header);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto gold=f.golden();icon.set_golden(true);
  CHECK(icon.install(5,5,f.data(),nullptr,nullptr,&gold)&&icon.ready());
  const auto normal_image=f.get<unsigned>(refs+9*4)>>16,header_copy=f.get<unsigned>(state+0x18);
  CHECK(f.get<unsigned>(header_copy+0x44)==4&&f.get<unsigned>(header_copy+0x24)==4); // Only one new Gold CLUT.
  CHECK(f.get<unsigned>(refs+5*4)==f.get<unsigned>(refs+9*4));
  icon.set_golden(false);CHECK(icon.ready()&&(f.get<unsigned>(refs+5*4)>>16)==normal_image);
  CHECK(f.get<unsigned>(refs+5*4)!=f.get<unsigned>(refs+9*4));
  // Distinct texels are also represented safely, without aliasing the normal image.
  icon.reset();auto alternate=f.ammunition();gold.pixels=alternate.pixels;gold.pixel_bytes=1024;
  auto normal=f.data();normal.pixel_bytes=1024;
  CHECK(icon.install(5,5,normal,nullptr,nullptr,&gold)&&icon.ready());
  CHECK((f.get<unsigned>(refs+5*4)>>16)!=(f.get<unsigned>(refs+9*4)>>16));
  CHECK(f.get<unsigned>(f.get<unsigned>(state+0x18)+0x44)==5);
 }
 {
  Fixture f;HdIcon icon(&f.api);f.palette[3]=96;f.gold_palette[3]=112;f.vendor_palette[3]=128;
  auto hud=f.ammunition(),vendor=f.vendor_ammo(),gold=f.golden();
  icon.set_golden(true);CHECK(icon.install(5,5,f.data(),&hud,&vendor,&gold));
  const auto table=f.get<unsigned>(state+0x28),gold_ref=f.get<unsigned>(refs+9*4);
  const auto gold_clut=f.get<unsigned>(table+(gold_ref&0xffffu)*8);
  CHECK(f.get<std::uint8_t>(gold_clut+3)==112); // Preserve native Gold alpha, including index zero.
  CHECK(icon.before_base()&&icon.ready());
  const auto blank_ref=f.get<unsigned>(refs+5*4),hud_ref=f.get<unsigned>(refs+8*4);
  CHECK((blank_ref&0xffffu)==(hud_ref&0xffffu));
  const auto blank_clut=f.get<unsigned>(table+(blank_ref&0xffffu)*8);
  CHECK(f.get<std::uint8_t>(blank_clut+3)==0);
  icon.after_base();CHECK(icon.ready()&&f.get<unsigned>(refs+5*4)==gold_ref);
  icon.reset();CHECK(f.get<unsigned>(state+0x28)==palettes&&f.get<unsigned>(refs+9*4)==0x00010001);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto hud=f.ammunition(),vendor=f.vendor_ammo(),gold=f.golden();
  icon.set_golden(true);CHECK(icon.install(5,5,f.data(),&hud,&vendor,&gold));
  CHECK(icon.before_base());f.put<unsigned>(refs+7*4,0);icon.set_golden(false);
  CHECK(f.get<unsigned>(refs+7*4)==0&&!icon.ready());
  CHECK(f.get<unsigned>(state+0x24)==images&&f.get<unsigned>(state+0x18)==header);
  for(unsigned i=5;i<10;++i)if(i!=7)CHECK(f.get<unsigned>(refs+i*4)==0x00010001);
  CHECK(icon.install(5,5,f.data(),&hud,&vendor,&gold));
  CHECK(icon.before_base());f.put<unsigned>(refs+9*4,0);icon.after_base();
  CHECK(f.get<unsigned>(refs+9*4)==0&&f.get<unsigned>(refs+7*4)!=0x00010001&&!icon.ready());
  icon.frame();CHECK(f.get<unsigned>(refs+9*4)==0&&f.get<unsigned>(state+0x24)==images);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto hud=f.ammunition(),vendor=f.vendor_ammo(),gold=f.golden();
  icon.set_golden(true);CHECK(icon.install(5,5,f.data(),&hud,&vendor,&gold));
  const auto clone=f.get<unsigned>(state+0x24),palette_clone=f.get<unsigned>(state+0x28),header_clone=f.get<unsigned>(state+0x18);
  std::memcpy(f.ram.data()+0x500000,f.ram.data()+refs,10*4);f.put(state+0x20,0x500000u);
  const auto foreign_gold=f.get<unsigned>(0x500000+9*4),foreign_ammo=f.get<unsigned>(0x500000+7*4);
  icon.set_golden(false);icon.reset();
  CHECK(f.get<unsigned>(state+0x24)==clone&&f.get<unsigned>(state+0x28)==palette_clone&&f.get<unsigned>(state+0x18)==header_clone);
  CHECK(f.get<unsigned>(0x500000+9*4)==foreign_gold&&f.get<unsigned>(0x500000+7*4)==foreign_ammo);
 }
 {
  Fixture f;HdIcon icon(&f.api);auto hud=f.ammunition(),vendor=f.vendor_ammo(),gold=f.golden();
  auto invalid=vendor;invalid.pixel_bytes=16384;
  CHECK(!icon.install(5,5,f.data(),&hud,&invalid,&gold));
  invalid=vendor;invalid.palette_bytes=512;CHECK(!icon.install(5,5,f.data(),&hud,&invalid,&gold));
  invalid=vendor;invalid.pixels=nullptr;CHECK(!icon.install(5,5,f.data(),&hud,&invalid,&gold));
  invalid=vendor;invalid.palette=nullptr;CHECK(!icon.install(5,5,f.data(),&hud,&invalid,&gold));
  invalid=gold;invalid.pixel_bytes=1024;CHECK(!icon.install(5,5,f.data(),&hud,&vendor,&invalid));
  invalid=gold;invalid.palette_bytes=512;CHECK(!icon.install(5,5,f.data(),&hud,&vendor,&invalid));
  invalid=gold;invalid.pixels=nullptr;CHECK(!icon.install(5,5,f.data(),&hud,&vendor,&invalid));
  invalid=gold;invalid.palette=nullptr;CHECK(!icon.install(5,5,f.data(),&hud,&vendor,&invalid));
  CHECK(!icon.install(5,2,f.data(),nullptr,&vendor));
  CHECK(!icon.install(5,4,f.data(),&hud,&vendor,&gold));
  CHECK(f.allocations==0&&f.get<unsigned>(state+0x18)==header&&f.get<unsigned>(state+0x24)==images);
  f.fail_allocate=true;CHECK(!icon.install(5,5,f.data(),&hud,&vendor,&gold));
  CHECK(f.get<unsigned>(state+0x18)==header&&f.get<unsigned>(refs+5*4)==0x00010001);
 }
 {
  Fixture f;revolverator::GuestArena no_tail;auto native=f.data(),gold=f.golden(),hud=f.ammunition(),vendor=f.vendor_ammo();
  native.pixel_bytes=gold.pixel_bytes=1024;f.put<unsigned>(state+0x10,0x600000);f.put<unsigned>(state+0x14,0x610000);
  HdIcon compact(&f.api,&no_tail,true);compact.set_golden(true);
  CHECK(compact.install(5,5,native,&hud,&vendor,&gold)&&compact.ready()&&f.allocations==1);
  CHECK(f.get<unsigned>(f.get<unsigned>(state+0x18)+0x44)==6&&f.get<unsigned>(f.get<unsigned>(state+0x18)+0x24)==6);
  CHECK(f.get<unsigned>(refs+5*4)==f.get<unsigned>(refs+9*4));
 }
 std::printf("%u HD icon checks passed: native AMMO/HUD/Gold frames, shared texels, hiding, ownership, allocation and lifecycle\n",checks);
}

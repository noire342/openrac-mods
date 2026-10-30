// SPDX-License-Identifier: GPL-3.0-or-later
#include "hd_icon.hpp"
#include "native_arena.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using revolverator::HdIcon;
#define CHECK(expression) do{if(!(expression)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#expression);std::exit(1);}}while(0)

namespace {
constexpr unsigned state=0x19a3e8,defs=0x300000,refs=0x301000,images=0x302000,palettes=0x303000;
struct Fixture;
Fixture* fixture;
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000,0);
 std::array<std::uint8_t,16384> pixels{};
 std::array<std::uint8_t,1024> palette{};
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
  put<std::uint16_t>(defs,59901);put<std::uint16_t>(defs+2,6);put<std::uint16_t>(defs+4,0);
  put<std::uint16_t>(defs+8,60018);put<std::uint16_t>(defs+10,4);put<std::uint16_t>(defs+12,6);
  put<std::uint16_t>(defs+16,0xffff);
  for(unsigned i=0;i<10;++i)put(refs+i*4,0x00010001u);
  for(unsigned i=0;i<2;++i){put(images+i*8,0x400000+i*1024);put<std::uint16_t>(images+i*8+6,0x0505);put(palettes+i*8,0x410000+i*1024);}
  pixels.fill(1);palette[7]=128;
 }
 revolverator::HdIconData data()const{return {pixels.data(),pixels.size(),palette.data(),palette.size()};}
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
  Fixture f;HdIcon icon(&f.api);auto bad=f.data();bad.pixel_bytes=32*32;
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
 std::puts("HD icon: PSMT8 pages, logical32px draw, native HUD suppression, cache invalidation, ownership, failure and level lifecycle passed");
}

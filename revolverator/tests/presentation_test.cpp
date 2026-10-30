// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "presentation.hpp"
#include "ammunition_icon_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* description){
 ++checks;if(!condition){std::fprintf(stderr,"presentation: %s\n",description);std::exit(1);}
}
constexpr unsigned State=0x19a3e8,Cached=0x17e4d8,Slot=0x199b60;
constexpr unsigned Defs=0x300000,Refs=0x301000,Images=0x302000,Palettes=0x303000,Header=0x304000;
constexpr unsigned TextBank=0x310000,Name=0x311000,Description=0x311100,ForeignName=0x311200;
constexpr unsigned Definition=0x1863d0+18*76,NativeCode=0x230000,ForeignCode=0x230100,RelayCode=0x2bb758;
constexpr unsigned FrameCount=20;
struct Fixture;Fixture* live;
struct Sprite{int frame,width,height;};
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};GuestArena arena;
 std::array<unsigned,5> cached_original{{0x450000,0x451000,0x452000,0x453000,0x454000}};
 int level=1,native_calls=0,previous_xp_calls=0,quick_calls=0,rectangles=0,quads=0,heap_calls=0;
 bool steal_cached_image=false,change_current_bank=false,cache_available=true;
 std::vector<Sprite> sprites;
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<std::size_t N>std::array<unsigned char,N> bytes(unsigned at)const{
  std::array<unsigned char,N> result{};std::memcpy(result.data(),ram.data()+at,N);return result;
 }
 static int find_frame(int bank,int frame){
  const auto definitions=live->get<unsigned>(State+0x1c);
  for(unsigned i=0;i<1024;++i){
   const auto entry=definitions+i*8;const auto id=live->get<unsigned short>(entry);
   if(id==0xffff)return 0;
   if(id==bank)return frame>=0&&frame<live->get<unsigned short>(entry+2)
      ?int(live->get<unsigned short>(entry+4))+frame:0;
  }
  return 0;
 }
 static void verify_cache(){
  if(live->cache_available)
   require(live->get<std::array<unsigned,5>>(Cached+0x18)==live->get<std::array<unsigned,5>>(State+0x18),
           "native draws see one coherent header/definition/reference/image/palette view");
 }
 static void sprite(int frame,int,int,int width,int height,int){live->sprites.push_back({frame,width,height});}
 static int native_hud(unsigned element){
  auto& f=*live;++f.native_calls;verify_cache();
  require(f.get<int>(element)==int(Presentation::custom_icon_bank),"only the current native draw uses the custom icon bank");
  require(f.get<int>(element+0x20)==60018,"the native next-item scheduler remains Walloper transport18");
  // The retail HUD requests bank(60000 + item), frame3. A native Walloper
  // bank has only two frames; the historical wrong path resolves frame0.
  const int frame=find_frame(f.get<int>(element),3);
  require(frame==int(FrameCount+3),"native frame3 resolves the appended ammunition instead of missing frame0");
  const auto references=f.get<unsigned>(State+0x20),images=f.get<unsigned>(State+0x24),palettes=f.get<unsigned>(State+0x28);
  const auto reference=f.get<unsigned>(references+unsigned(frame)*4);
  const auto image=images+(reference>>16)*8,palette=palettes+(reference&0xffffu)*8;
  const auto pixels=f.get<unsigned>(image),clut=f.get<unsigned>(palette);
  const auto ammunition=generated::ammunition_icon_data();
  require(f.get<unsigned short>(image+6)==0x0505,"native HUD dimensions are32x32 rather than the128px weapon glyph");
  require(std::memcmp(f.ram.data()+pixels,ammunition.pixels,1024)==0,"native ammo pixels are the authored cartridge");
  require(std::memcmp(f.ram.data()+clut,ammunition.palette,1024)==0,"native ammo uses its own transparent CSM1 palette");
  sprite(frame,0,0,1<<f.get<unsigned char>(image+6),1<<f.get<unsigned char>(image+7),128);
  if(f.steal_cached_image)f.put(Cached+0x24,0x490000u);
  if(f.change_current_bank)f.put<int>(element,60015);
  return 73;
 }
 static void previous_xp(unsigned){++live->previous_xp_calls;}
 static void relay(unsigned){}
 static int quick(unsigned){
  ++live->quick_calls;verify_cache();
  const auto references=live->get<unsigned>(State+0x20);
  require(live->get<unsigned>(references+(FrameCount+3)*4)!=live->get<unsigned>(references+FrameCount*4),
          "hiding weapon glyphs in the original quick ring never hides the cartridge frame");
  return 8;
 }
 Fixture(){
  live=this;require(arena.reserve(0x600000,0x20000)==0x620000,"fixture reserves its owned native arena");
  api.memory=ram.data();api.game="rac1";api.version="ntsc";
  api.loaded_level=+[]{return live->level;};
  api.relocate_data=+[](unsigned at){return at;};
  api.relocate_level_data=+[](int,unsigned at){return at==Cached&&!live->cache_available?0u:at;};
  api.setting_bool=+[](openrac_mod*,const char*){return 1;};
  api.function_at=+[](unsigned at)->void*{
   if(at==NativeCode)return reinterpret_cast<void*>(native_hud);
   if(at==RelayCode)return reinterpret_cast<void*>(relay);
   return nullptr;
  };
  api.function=+[](const char* name)->void*{
   if(!std::strcmp(name,"FUN_L00_0023a7a8"))return reinterpret_cast<void*>(native_hud);
   if(!std::strcmp(name,"FUN_L00_002a5dd8"))return reinterpret_cast<void*>(relay);
   if(!std::strcmp(name,"FUN_001ffc30"))return reinterpret_cast<void*>(sprite);
   if(!std::strcmp(name,"FUN_001ff960"))return reinterpret_cast<void*>(find_frame);
   if(!std::strcmp(name,"FUN_001ff288"))return reinterpret_cast<void*>(+[](unsigned)->unsigned{++live->heap_calls;return 0;});
   if(!std::strcmp(name,"FUN_00200e08"))return reinterpret_cast<void*>(+[](int,int,int,int,std::uint64_t,int){++live->rectangles;});
   if(!std::strcmp(name,"FUN_001f5450"))return reinterpret_cast<void*>(+[](int,int,int,int,int,int,int,int,std::uint64_t,std::uint64_t){++live->quads;});
   if(!std::strcmp(name,"FUN_001f44b8"))return reinterpret_cast<void*>(+[](int)->std::uint64_t{return 7;});
   if(!std::strcmp(name,"FUN_L00_00235a70"))return reinterpret_cast<void*>(+[](unsigned,unsigned,unsigned){return 0;});
   if(!std::strcmp(name,"FUN_001f9dc8"))return reinterpret_cast<void*>(+[](float value){return std::cos(value);});
   if(!std::strcmp(name,"FUN_001f9de0"))return reinterpret_cast<void*>(+[](float value){return std::sin(value);});
   if(!std::strcmp(name,"FUN_001fa580"))return reinterpret_cast<void*>(+[](float left,float right){return left+right;});
   return nullptr;
  };
  api.stack_push=+[](unsigned bytes,unsigned* saved){require(bytes==16,"quick ring uses a bounded stack temporary");*saved=17;return 0x500000u;};
  api.stack_pop=+[](unsigned saved){require(saved==17,"quick ring restores its stack marker");};
  put(State+0x18,Header);put(State+0x1c,Defs);put(State+0x20,Refs);put(State+0x24,Images);put(State+0x28,Palettes);
  put(Header+0x44,2u);put(Header+0x24,2u);put(Cached+0x18,cached_original);
  const auto bank=[&](unsigned index,unsigned short id,unsigned short count,unsigned short start){
   put(Defs+index*8,id);put(Defs+index*8+2,count);put(Defs+index*8+4,start);
  };
  bank(0,59901,14,0);bank(1,60018,2,14);bank(2,60015,4,16);bank(3,0xffff,0,0);
  for(unsigned i=0;i<FrameCount;++i)put(Refs+i*4,0x00010001u);
  put(Refs+13*4,0u);
  for(unsigned i=0;i<2;++i){
   put(Images+i*8,0x400000+i*1024);put<unsigned short>(Images+i*8+6,0x0505);
   put(Palettes+i*8,0x410000+i*1024);
  }
  for(unsigned i=0;i<1024;++i)ram[0x400000+i]=static_cast<unsigned char>(i%3+1);
  for(unsigned i=1;i<256;++i){ram[0x410000+i*4]=static_cast<unsigned char>(i);ram[0x410000+i*4+3]=128;}
  put(0x15f6a0,TextBank);put<int>(0x1996fc,2);
  put(TextBank,std::array<unsigned,4>{{Name,20037,7,9}});
  put(TextBank+16,std::array<unsigned,4>{{Description,20345,1,2}});
  std::memcpy(ram.data()+Name,"Walloper",9);std::memcpy(ram.data()+Description,"Original localized description",31);
  std::memcpy(ram.data()+ForeignName,"Another mod's Walloper v6",25);
  put<int>(Definition,20037);put<short>(Definition+0x40,20345);put<unsigned short>(Definition+0x38,60037);
  put<int>(Slot,60018);put<int>(Slot+0x20,60018);put(Slot+0x18,NativeCode);
  put<int>(Slot+8,20);put<int>(Slot+0x74,20);put<int>(Slot+0x50,20);
  put<unsigned char>(Slot+0x70,30);put<int>(0x15f91c,30);put<unsigned>(0x15f92c,0x00ffffff);
  for(unsigned i=32;i<128;++i)put<signed char>(0x1df3f0+i*4+3,8);
 }
};
struct IdleWorld final:World {
 bool begin_attack()override{return false;}
 std::optional<Target> find_target(Vec3,float,float)override{return {};}
 bool valid(Target)const override{return false;}
 bool capture(Target)override{return false;}
 std::optional<Vec3> carry_anchor(float)override{return {};}
 void move(Target,Vec3,float)override{}
 void release(Target,bool)override{}
 std::optional<Contact> sweep(Target,Vec3,Vec3)override{return {};}
 void impact(Target,std::optional<Target>,float)override{}
 void animate(Motion)override{}
};
}

int main(){
 {
  Fixture f;const auto original_name=f.bytes<128>(Name),original_description=f.bytes<128>(Description);
  const auto definition=f.bytes<76>(Definition);
  const auto definitions=f.bytes<32>(Defs);
  const auto references=f.bytes<80>(Refs);
  Presentation presentation(&f.api,nullptr,&f.arena);
  presentation.select_identity(false);presentation.frame(false);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+0x18)==NativeCode,"a stock Walloper never attaches the Revolverator HUD relay");
  f.put<int>(Slot+0x20,60037);presentation.frame(true);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+0x18)==NativeCode,"item-definition icon60037 is not mistaken for the native scheduler60018");
  f.put<int>(Slot+0x20,60018);presentation.select_identity(true);presentation.frame(true);
  require(presentation.owns(Slot)&&f.get<unsigned>(Slot+0x18)==RelayCode,"equipped Revolverator attaches to the real native60018 HUD request");
  const auto copied_defs=f.get<unsigned>(State+0x1c),copied_refs=f.get<unsigned>(State+0x20);
  require(copied_defs!=Defs&&copied_refs!=Refs&&f.get<unsigned short>(copied_defs+3*8+2)==5,"custom bank has five distinct view slots without replacing native definitions");
  require(Fixture::find_frame(60018,3)==0,"fixture reproduces the actual two-frame Walloper lookup failure");
  const auto drill=f.get<unsigned>(copied_refs+FrameCount*4),ammo=f.get<unsigned>(copied_refs+(FrameCount+3)*4);
  require(drill!=ammo,"native ammo and inventory weapon glyph are separate image/palette references");
  for(unsigned offset:{0u,1u,2u,4u})require(f.get<unsigned>(copied_refs+(FrameCount+offset)*4)==drill,"all weapon/menu/gold frames retain the drill glyph");
  presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.native_calls==1&&f.previous_xp_calls==0,"custom HUD draws native base exactly once and bypasses the previous Walloper XP relay");
  require(f.get<int>(Slot)==60018&&f.get<int>(Slot+0x20)==60018,"current render bank restores without altering the next-item scheduler");
  require(f.get<std::array<unsigned,5>>(Cached+0x18)==f.cached_original,"overlay HUD table mapping restores after the bounded draw");
  require(f.sprites.size()==1&&f.sprites[0].frame==int(FrameCount+3)&&f.sprites[0].width==32&&f.sprites[0].height==32,"the visible native HUD sprite is the32px cartridge");
  IdleWorld world;Controller controller(world);
  presentation.set_experience(true,4,.5f,true);presentation.after_hud(Slot,controller);
  require(f.sprites.size()==1&&f.quads>0&&f.rectangles>0,"custom XP decorations never overlay a weapon glyph on the ammunition icon");
  presentation.names();
  require(std::strcmp(reinterpret_cast<const char*>(f.ram.data()+f.get<unsigned>(TextBank)),"Revolverator v4")==0,"custom level caption projects through the localized entry");
  require(f.bytes<128>(Name)==original_name&&f.bytes<128>(Description)==original_description,"original localized strings remain read-only");
  require(f.bytes<76>(Definition)==definition,"presentation never changes native item price/capacity/definition fields");
  constexpr unsigned quick_element=0x500100;
  f.put<int>(0x15fa90,8);f.put<int>(0x15f780,30);f.put<float>(0x15f78c,100);f.put<float>(0x15f7c4,1);f.put<float>(0x15f7c0,.75f);
  f.put<unsigned char>(quick_element+0x70,30);f.put<int>(quick_element+0x74,1);
  std::array<arsenal::WeaponKey,8> quick{{arsenal::NativeWalloperKey,arsenal::RevolveratorKey}};
  require(presentation.draw_quick_selector(quick_element,Fixture::quick,quick)==8&&f.quick_calls==1,
          "native quick-select draw and return value remain intact");
  require(f.sprites.size()==2&&f.sprites.back().frame==int(FrameCount)&&f.sprites.back().width==32,
          "quick select overlays the separate drill glyph at its logical32px size");
  require(f.get<unsigned>(copied_refs+FrameCount*4)==drill&&f.get<unsigned>(copied_refs+(FrameCount+3)*4)==ammo,
          "quick-select hiding restores drill references without modifying ammunition");
  require(f.get<std::array<unsigned,5>>(Cached+0x18)==f.cached_original,"quick-select cached HUD views restore after drawing");
  presentation.reset();
  require(f.get<std::array<unsigned,5>>(State+0x18)==std::array<unsigned,5>{{Header,Defs,Refs,Images,Palettes}},"owned header and HUD tables restore together");
  require(f.get<unsigned>(Slot+0x18)==NativeCode&&f.get<unsigned>(TextBank)==Name&&f.get<unsigned>(TextBank+16)==Description,"native callback and localized entries restore on teardown");
  require(f.bytes<32>(Defs)==definitions&&f.bytes<80>(Refs)==references&&f.heap_calls==0,"original tables stay intact and all storage belongs to the injected arena");
 }
 {
  Fixture f;f.put(Slot+0x18,RelayCode);Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  require(presentation.owns(Slot),"an existing shared XP relay can be observed without replacing another callback");
  f.steal_cached_image=true;f.change_current_bank=true;presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.native_calls==1&&f.previous_xp_calls==0,"existing XP relay ownership cannot duplicate native/custom HUD output");
  require(f.get<int>(Slot)==60015,"a current-icon replacement made by the native callback is preserved");
  auto expected=f.cached_original;expected[3]=0x490000;
  require(f.get<std::array<unsigned,5>>(Cached+0x18)==expected,"an overlay table replacement made during the draw is preserved");
  f.put(TextBank,ForeignName);presentation.names();presentation.select_identity(false);
  require(f.get<unsigned>(TextBank)==ForeignName,"a newer localized-name owner becomes the stock identity baseline");
  f.put(Slot+0x18,ForeignCode);presentation.reset();
  require(f.get<unsigned>(Slot+0x18)==ForeignCode&&f.get<unsigned>(TextBank)==ForeignName,"teardown preserves later callback and name owners");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const auto copied_images=f.get<unsigned>(State+0x24),copied_definitions=f.get<unsigned>(State+0x1c);
  f.put(State+0x20,0x480000u);presentation.reset();
  require(f.get<unsigned>(State+0x20)==0x480000&&f.get<unsigned>(State+0x24)==copied_images&&f.get<unsigned>(State+0x1c)==copied_definitions,
          "foreign reference tables cannot be left dangling into reclaimed appended pages");
 }
 {
  Fixture f;f.cache_available=false;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const auto low_memory=f.bytes<64>(0);presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.bytes<64>(0)==low_memory&&f.native_calls==1,"an unavailable overlay cache never causes a low-address write");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  f.put(Slot+0x18,ForeignCode);presentation.frame(true);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+0x18)==ForeignCode,"frame updates never replace an unknown newer HUD callback");
 }
 std::printf("presentation: %u scheduler, native ammo, XP relay, localized-name and table-ownership checks passed\n",checks);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "presentation.hpp"
#include "localization.hpp"
#include "ammunition_icon_data.hpp"
#include "native_arsenal_layout.hpp"
#include "native_hud_layout.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* description){
 ++checks;if(!condition){std::fprintf(stderr,"presentation: %s\n",description);std::exit(1);}
}
namespace hud = native_hud;
namespace item = native_arsenal_layout;
constexpr auto State = hud::resident_state;
constexpr auto Cached = hud::level_state;
constexpr auto Slot = hud::channel_slots;
constexpr int walloper_hud_bank = hud::weapon_hud_bank_base + arsenal::NativeWalloperKey;
constexpr int other_weapon_hud_bank = 60015;
constexpr unsigned Defs=0x300000,Refs=0x301000,Images=0x302000,Palettes=0x303000,Header=0x304000;
constexpr unsigned TextBank=0x310000,Name=0x311000,Description=0x311100,ForeignName=0x311200;
constexpr auto Definition = item::resident_item_definitions + arsenal::NativeWalloperKey * item::item_definition_size;
constexpr unsigned NativeCode=0x230000,ForeignCode=0x230100,RelayCode=0x2bb758;
constexpr unsigned foreign_images = 0x490000;
constexpr unsigned foreign_references = 0x480000;
constexpr unsigned FrameCount = 20;
constexpr unsigned ammunition_reference = FrameCount + hud::ammunition_frame;
constexpr unsigned frame_reference_size = sizeof(hud::FrameReference);
constexpr unsigned texture_page_size = sizeof(hud::TexturePage);
constexpr unsigned cached_image_index = (hud::image_pages - hud::bank_header) / sizeof(std::uint32_t);
using HudTables = std::array<std::uint32_t,(hud::table_end - hud::bank_header) / sizeof(std::uint32_t)>;
constexpr HudTables stock_tables{Header,Defs,Refs,Images,Palettes};
constexpr unsigned hud_icon_log2 = 5;
constexpr int hud_icon_pixels = 1 << hud_icon_log2;
constexpr std::size_t ammo_pixel_bytes = hud_icon_pixels * hud_icon_pixels;

constexpr unsigned text_bank_address = 0x15f6a0;
constexpr unsigned text_count_address = 0x1996fc;
constexpr unsigned text_entry_size = 4 * sizeof(std::uint32_t);
constexpr unsigned text_name_field = 0;
constexpr unsigned text_description_field = 0x40;
constexpr int name_id = 20037;
constexpr int description_id = 20345;
constexpr int native_counter_ammo = 20;
constexpr int vendor_counter_ammo = 20;
constexpr int vendor_idle_ammo = 0;
constexpr int quick_ring_draw_result = 8;
constexpr int quick_ring_mode = 8;
constexpr unsigned quick_ring_type = 0x15fa90;
constexpr unsigned quick_ring_fade_frames = 0x15f780;
constexpr unsigned quick_ring_radius = 0x15f78c;
constexpr unsigned quick_ring_width_scale = 0x15f7c4;
constexpr unsigned quick_ring_unselected_alpha = 0x15f7c0;
constexpr int fully_visible = 30;
constexpr int selected_quick_slot = 1;
constexpr unsigned quick_slot_selection = 0x74;
constexpr unsigned custom_definition_index = 3;
struct Fixture;Fixture* live;
struct Sprite{int frame,width,height;};
struct Fixture {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};GuestArena arena;
 HudTables cached_original{{0x450000,0x451000,0x452000,0x453000,0x454000}};
 int level=1,native_calls=0,previous_xp_calls=0,quick_calls=0,rectangles=0,quads=0,heap_calls=0;
 int drawn_ammo=0;
 bool steal_cached_image=false,change_current_bank=false,cache_available=true;
 std::vector<Sprite> sprites;
 std::vector<std::array<int,4>> drawn_rectangles;
 std::string notified_text;int notifications=0,notified_duration=0;
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
 template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<std::size_t N>std::array<unsigned char,N> bytes(unsigned at)const{
  std::array<unsigned char,N> result{};std::memcpy(result.data(),ram.data()+at,N);return result;
 }
 static int find_frame(int bank,int frame){
  const auto definitions=live->get<unsigned>(State+hud::animation_definitions);
  for(unsigned i=0;i<hud::max_animation_definitions;++i){
   const auto entry=live->get<hud::AnimationDefinition>(definitions+i*sizeof(hud::AnimationDefinition));
   if(entry.id==hud::animation_terminator)return 0;
   if(entry.id==bank)return frame>=0&&frame<entry.count ? int(entry.start)+frame : 0;
  }
  return 0;
 }
 static void verify_cache(){
  if(live->cache_available)
   require(live->get<HudTables>(Cached+hud::bank_header)==live->get<HudTables>(State+hud::bank_header),
           "native draws see one coherent header/definition/reference/image/palette view");
 }
 static void sprite(int frame,int,int,int width,int height,int){live->sprites.push_back({frame,width,height});}
 static int native_hud(unsigned element){
  auto& f=*live;++f.native_calls;verify_cache();
  f.drawn_ammo=f.get<int>(element+hud::channel_ammunition);
  require(f.get<int>(element+hud::channel_current_id)==int(Presentation::custom_icon_bank),"only the current native draw uses the custom icon bank");
  require(f.get<int>(element+hud::channel_next_id)==walloper_hud_bank,"the native next-item scheduler remains Walloper transport18");
  // The retail HUD requests bank(60000 + item), frame3. A native Walloper
  // bank has only two frames; the historical wrong path resolves frame0.
  const int frame=find_frame(f.get<int>(element+hud::channel_current_id),hud::ammunition_frame);
  require(frame==int(ammunition_reference),"native frame3 resolves the appended ammunition instead of missing frame0");
  const auto references=f.get<unsigned>(State+hud::frame_references),images=f.get<unsigned>(State+hud::image_pages),palettes=f.get<unsigned>(State+hud::palette_pages);
  const auto reference=f.get<hud::FrameReference>(references+unsigned(frame)*frame_reference_size);
  const auto image=f.get<hud::TexturePage>(images+reference.image_index*texture_page_size);
  const auto palette=f.get<hud::TexturePage>(palettes+reference.palette_index*texture_page_size);
  const auto ammunition=generated::ammunition_icon_data();
  require(image.width_log2==hud_icon_log2&&image.height_log2==hud_icon_log2,
          "native HUD dimensions match the cartridge image");
  require(std::memcmp(f.ram.data()+image.source_address,ammunition.pixels,ammo_pixel_bytes)==0,
          "native ammo pixels are the authored cartridge");
  require(std::memcmp(f.ram.data()+palette.source_address,ammunition.palette,hud::palette_bytes)==0,
          "native ammo uses its own transparent CSM1 palette");
  sprite(frame,0,0,1<<image.width_log2,1<<image.height_log2,hud::max_alpha);
  if(f.steal_cached_image)f.put(Cached+hud::image_pages,foreign_images);
  if(f.change_current_bank)f.put<int>(element+hud::channel_current_id,other_weapon_hud_bank);
  return 73;
 }
 static void previous_xp(unsigned){++live->previous_xp_calls;}
 static void relay(unsigned){}
 static int quick(unsigned){
  ++live->quick_calls;verify_cache();
  const auto references=live->get<unsigned>(State+hud::frame_references);
  require(live->get<unsigned>(references+ammunition_reference*frame_reference_size)!=live->get<unsigned>(references+FrameCount*frame_reference_size),
          "hiding weapon glyphs in the original quick ring never hides the cartridge frame");
  return quick_ring_draw_result;
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
   if(!std::strcmp(name,"FUN_L00_00263d40"))return reinterpret_cast<void*>(+[](int id,int duration){
    require(id==name_id,"native notification selects the localized weapon-name ID");
    live->notified_text=reinterpret_cast<const char*>(live->ram.data()+live->get<unsigned>(TextBank));
    live->notified_duration=duration;++live->notifications;
   });
   if(!std::strcmp(name,"FUN_L00_0023a7a8"))return reinterpret_cast<void*>(native_hud);
   if(!std::strcmp(name,"FUN_L00_002a5dd8"))return reinterpret_cast<void*>(relay);
   if(!std::strcmp(name,"FUN_001ffc30"))return reinterpret_cast<void*>(sprite);
   if(!std::strcmp(name,"FUN_001ff960"))return reinterpret_cast<void*>(find_frame);
   if(!std::strcmp(name,"FUN_001ff288"))return reinterpret_cast<void*>(+[](unsigned)->unsigned{++live->heap_calls;return 0;});
   if(!std::strcmp(name,"FUN_00200e08"))return reinterpret_cast<void*>(+[](int x,int y,int right,int bottom,std::uint64_t,int){++live->rectangles;live->drawn_rectangles.push_back({x,y,right-x,bottom-y});});
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
  put(State+hud::bank_header,stock_tables);
  put(Header+hud::bank_image_count,2u);put(Header+hud::bank_palette_count,2u);put(Cached+hud::bank_header,cached_original);
  const auto bank=[&](unsigned index,std::uint16_t id,std::uint16_t count,std::uint16_t start){
   put(Defs+index*sizeof(hud::AnimationDefinition),hud::AnimationDefinition{id,count,start,0,0});
  };
  constexpr std::uint16_t drill_frame_count = hud::drill_icon_frame + 1;
  constexpr std::uint16_t other_weapon_first_frame = drill_frame_count + hud::generic_icon_frame_count;
  bank(0,hud::drill_icon_bank,drill_frame_count,0);
  bank(1,walloper_hud_bank,hud::generic_icon_frame_count,drill_frame_count);
  bank(2,other_weapon_hud_bank,FrameCount-other_weapon_first_frame,other_weapon_first_frame);
  bank(custom_definition_index,hud::animation_terminator,0,0);
  for(unsigned i=0;i<FrameCount;++i)put(Refs+i*frame_reference_size,hud::FrameReference{1,1});
  put(Refs+hud::drill_icon_frame*frame_reference_size,hud::FrameReference{0,0});
  constexpr unsigned pixels = 0x400000;
  constexpr unsigned palette = 0x410000;
  constexpr unsigned stock_pages = 2;
  for(unsigned i=0;i<stock_pages;++i){
   put(Images+i*texture_page_size,hud::TexturePage{pixels+i*unsigned(ammo_pixel_bytes),0,hud_icon_log2,hud_icon_log2});
   put(Palettes+i*texture_page_size,hud::TexturePage{palette+i*hud::palette_bytes,0,0,0});
  }
  for(unsigned i=0;i<ammo_pixel_bytes;++i)ram[pixels+i]=static_cast<unsigned char>(i%3+1);
  for(unsigned i=1;i<hud::palette_bytes/sizeof(std::uint32_t);++i){
   const auto rgba=palette+i*sizeof(std::uint32_t);
   ram[rgba]=static_cast<unsigned char>(i);
   ram[rgba+sizeof(std::uint32_t)-1]=hud::max_alpha;
  }
  put(text_bank_address,TextBank);put<int>(text_count_address,2);
  put(TextBank,std::array<unsigned,4>{{Name,name_id,7,9}});
  put(TextBank+text_entry_size,std::array<unsigned,4>{{Description,description_id,1,2}});
  std::memcpy(ram.data()+Name,"Walloper",9);std::memcpy(ram.data()+Description,"Original localized description",31);
  std::memcpy(ram.data()+ForeignName,"Another mod's Walloper v6",25);
  put<int>(Definition+text_name_field,name_id);put<short>(Definition+text_description_field,description_id);put(Definition+item::item_icon_bank,item::revolverator_icon_bank);
  put<int>(Slot,walloper_hud_bank);put<int>(Slot+hud::channel_next_id,walloper_hud_bank);put(Slot+hud::channel_callback,NativeCode);
  put<int>(Slot+8,20);put<int>(Slot+hud::channel_ammunition,native_counter_ammo);put<int>(Slot+hud::channel_x,20);
  put<unsigned char>(Slot+hud::channel_visibility,fully_visible);put<int>(hud::ammo_fade_frames,fully_visible);put<unsigned>(0x15f92c,0x00ffffff);
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
struct ChargingWorld final:World {
 const Target enemy{0x40000,0x80000,459,1};
 bool begin_attack()override{return true;}
 std::optional<Target> find_target(Vec3,float,float)override{return enemy;}
 bool valid(Target target)const override{return target==enemy;}
 bool capture(Target)override{return true;}
 std::optional<Vec3> carry_anchor(float)override{return Vec3{};}
 void move(Target,Vec3,float)override{}
 void release(Target,bool)override{}
 std::optional<Contact> sweep(Target,Vec3,Vec3)override{return {};}
 void impact(Target,std::optional<Target>,float)override{}
 void animate(Motion)override{}
};
}

int main(){
 for(bool with_xp:{false,true}){
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);
  presentation.select_identity(true);presentation.frame(true);presentation.set_experience(with_xp,2,.5f,true);
  ChargingWorld world;Settings settings;settings.full_charge=1.f;Controller controller(world,settings);
  Frame frame;frame.equipped=frame.playing=frame.alive=frame.grounded=frame.primary=true;
  for(unsigned i=0;i<30&&controller.attack_phase()!=Phase::Spinning;++i)controller.tick(frame);
  require(controller.attack_phase()==Phase::Spinning,"charge-bar fixture captures through the real controller");
  frame.seconds=.0999f;for(unsigned i=0;i<10;++i)controller.tick(frame);
  require(controller.charge()<1.f&&controller.charge()>.99f,"fixture is just below the actual full-charge threshold");
  f.drawn_rectangles.clear();presentation.after_hud(Slot,controller);
  constexpr int xp_charge_fill_width = 35;
  constexpr int standalone_charge_fill_width = 68;
  const int maximum=with_xp?xp_charge_fill_width:standalone_charge_fill_width;
  require(!f.drawn_rectangles.empty()&&f.drawn_rectangles.back()[2]==maximum-1,
          "a partial charge never paints the last pixel, with or without XP");
  frame.seconds=.01f;controller.tick(frame);
  f.drawn_rectangles.clear();presentation.after_hud(Slot,controller);
  require(controller.charge()==1.f&&f.drawn_rectangles.back()[2]==maximum,
          "the final charge pixel appears at the same threshold that permits throwing");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);
  f.put<int>(localization::native_language_address,5);
  presentation.set_experience(true,10,1,true);presentation.select_identity(false);
  const auto stock_entry=f.get<unsigned>(TextBank);
  for(unsigned level=2;level<=10;++level){
   require(presentation.notify_acquisition(level),"every supported XP level uses the native completion subtitle");
   require(f.notified_text=="Rotatore v"+std::to_string(level)&&f.notified_duration==180,"Italian completed titles show the exact queued vN through the native subtitle channel");
   require(f.get<unsigned>(TextBank)==stock_entry,"native Walloper identity restores immediately after copying the custom subtitle");
  }
  presentation.select_identity(true);presentation.names();
  const auto projected=f.get<unsigned>(TextBank);
  presentation.set_golden(true);require(presentation.notify_acquisition(2)&&f.notified_text=="Rotatore d'oro v2","Golden level completion has its localized Golden title");
  require(f.get<unsigned>(TextBank)==projected&&std::strcmp(reinterpret_cast<const char*>(f.ram.data()+projected),"Rotatore d'oro v10")==0,"temporary v2 notification never downgrades the current v10 inventory name");
  require(presentation.notify_acquisition()&&f.notified_text=="Rotatore d'oro v10","Gold purchase uses the current level when no XP event version is supplied");
  presentation.set_experience(true,4,.5f,false);
  require(presentation.notify_acquisition(4)&&f.notified_text=="Rotatore d'oro","disabling versioned names preserves the user's native name preference");
  const int before=f.notifications;
  require(!presentation.notify_acquisition(1)&&!presentation.notify_acquisition(11)&&f.notifications==before,"invalid caption versions never reach the native subtitle call");
  f.level=2;f.put(TextBank,ForeignName);
  const auto old_name=f.bytes<256>(projected);
  require(!presentation.notify_acquisition(2)&&f.notifications==before,"stale text banks cannot notify after overlay travel");
  presentation.set_golden(false);
  require(f.get<unsigned>(TextBank)==ForeignName&&f.bytes<256>(projected)==old_name,
          "reusing a text-bank address in another level cannot rewrite its entries or the retired name storage");
 }
 {
  Fixture f;const auto original_name=f.bytes<128>(Name),original_description=f.bytes<128>(Description);
  const auto definition=f.bytes<item::item_definition_size>(Definition);
  const auto definitions=f.bytes<32>(Defs);
  const auto references=f.bytes<80>(Refs);
  Presentation presentation(&f.api,nullptr,&f.arena);
  presentation.select_identity(false);presentation.frame(false);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+hud::channel_callback)==NativeCode,"a stock Walloper never attaches the Revolverator HUD relay");
  f.put<int>(Slot+hud::channel_next_id,item::revolverator_icon_bank);presentation.frame(true);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+hud::channel_callback)==NativeCode,"item-definition icon60037 is not mistaken for the native scheduler60018");
  f.put<int>(Slot+hud::channel_next_id,walloper_hud_bank);presentation.select_identity(true);presentation.frame(true);
  require(presentation.owns(Slot)&&f.get<unsigned>(Slot+hud::channel_callback)==RelayCode,"equipped Revolverator attaches to the real native60018 HUD request");
  const auto copied_defs=f.get<unsigned>(State+hud::animation_definitions),copied_refs=f.get<unsigned>(State+hud::frame_references);
  const auto custom_definition=f.get<hud::AnimationDefinition>(copied_defs+custom_definition_index*sizeof(hud::AnimationDefinition));
  require(copied_defs!=Defs&&copied_refs!=Refs&&custom_definition.count==hud::custom_icon_frame_count,
          "custom bank has five distinct view slots without replacing native definitions");
  require(Fixture::find_frame(walloper_hud_bank,hud::ammunition_frame)==0,"fixture reproduces the actual two-frame Walloper lookup failure");
  const auto drill=f.get<unsigned>(copied_refs+FrameCount*frame_reference_size),ammo=f.get<unsigned>(copied_refs+ammunition_reference*frame_reference_size);
  require(drill!=ammo,"native ammo and inventory weapon glyph are separate image/palette references");
  for(unsigned offset:{0u,1u,hud::vendor_ammunition_frame,hud::gold_preview_frame})require(f.get<unsigned>(copied_refs+(FrameCount+offset)*frame_reference_size)==drill,"all weapon/menu/gold frames retain the drill glyph");
  presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.native_calls==1&&f.previous_xp_calls==0,"custom HUD draws native base exactly once and bypasses the previous Walloper XP relay");
  require(f.get<int>(Slot+hud::channel_current_id)==walloper_hud_bank&&f.get<int>(Slot+hud::channel_next_id)==walloper_hud_bank,"current render bank restores without altering the next-item scheduler");
  require(f.get<HudTables>(Cached+hud::bank_header)==f.cached_original,"overlay HUD table mapping restores after the bounded draw");
  require(f.sprites.size()==1&&f.sprites[0].frame==int(ammunition_reference)&&f.sprites[0].width==hud_icon_pixels&&f.sprites[0].height==hud_icon_pixels,"the visible native HUD sprite is the 32px cartridge");
  f.put(Slot+hud::channel_callback,NativeCode);f.put<int>(Slot+hud::channel_pending,1);
  presentation.frame(true);
  require(presentation.owns(Slot),"a queued refresh of the active ammo bank cannot bypass the custom relay");
  presentation.frame(false);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+hud::channel_callback)==NativeCode,"a different weapon restores its native callback");
  require(f.get<unsigned char>(Slot+hud::channel_visibility)==0,
          "the retired cartridge tile cannot draw with the stock icon on unequip");
  f.put<int>(Slot+hud::channel_pending,0);presentation.frame(true);
  f.put<unsigned char>(Slot+hud::channel_visibility,fully_visible);
  IdleWorld world;Controller controller(world);
  presentation.set_experience(true,4,.5f,true);presentation.after_hud(Slot,controller);
  require(f.sprites.size()==1&&f.quads>0&&f.rectangles>0,"custom XP decorations never overlay a weapon glyph on the ammunition icon");
  presentation.names();
  require(std::strcmp(reinterpret_cast<const char*>(f.ram.data()+f.get<unsigned>(TextBank)),"Revolverator v4")==0,"custom level caption projects through the localized entry");
  require(f.bytes<128>(Name)==original_name&&f.bytes<128>(Description)==original_description,"original localized strings remain read-only");
  require(f.bytes<item::item_definition_size>(Definition)==definition,"presentation never changes native item price/capacity/definition fields");
  constexpr unsigned quick_element=0x500100;
  f.put<int>(quick_ring_type,quick_ring_mode);
  f.put<int>(quick_ring_fade_frames,fully_visible);
  f.put<float>(quick_ring_radius,100);
  f.put<float>(quick_ring_width_scale,1);
  f.put<float>(quick_ring_unselected_alpha,.75f);
  f.put<unsigned char>(quick_element+hud::channel_visibility,fully_visible);
  f.put<int>(quick_element+quick_slot_selection,selected_quick_slot);
  std::array<arsenal::WeaponKey,8> quick{{arsenal::NativeWalloperKey,arsenal::RevolveratorKey}};
  require(presentation.draw_quick_selector(quick_element,Fixture::quick,quick)==quick_ring_draw_result&&f.quick_calls==1,
          "native quick-select draw and return value remain intact");
  require(f.sprites.size()==2&&f.sprites.back().frame==int(FrameCount)&&f.sprites.back().width==hud_icon_pixels,
          "quick select overlays the separate drill glyph at its logical 32px size");
  require(f.get<unsigned>(copied_refs+FrameCount*frame_reference_size)==drill&&f.get<unsigned>(copied_refs+ammunition_reference*frame_reference_size)==ammo,
          "quick-select hiding restores drill references without modifying ammunition");
  require(f.get<HudTables>(Cached+hud::bank_header)==f.cached_original,"quick-select cached HUD views restore after drawing");
  f.put<int>(Slot+hud::channel_ammunition,vendor_idle_ammo);
  presentation.draw_base(Slot,Fixture::previous_xp,vendor_counter_ammo);
  require(f.drawn_ammo==vendor_counter_ammo&&f.get<int>(Slot+hud::channel_ammunition)==vendor_idle_ammo,
          "the vendor counter reads its own ammo as the cursor moves, without altering native state");
  presentation.reset();
  require(f.get<HudTables>(State+hud::bank_header)==stock_tables,"owned header and HUD tables restore together");
  require(f.get<unsigned>(Slot+hud::channel_callback)==NativeCode&&f.get<unsigned>(TextBank)==Name&&f.get<unsigned>(TextBank+text_entry_size)==Description,"native callback and localized entries restore on teardown");
  require(f.bytes<32>(Defs)==definitions&&f.bytes<80>(Refs)==references&&f.heap_calls==0,"original tables stay intact and all storage belongs to the injected arena");
 }
 {
  Fixture f;f.put(Slot+hud::channel_callback,RelayCode);Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  require(presentation.owns(Slot),"an existing shared XP relay can be observed without replacing another callback");
  f.steal_cached_image=true;f.change_current_bank=true;presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.native_calls==1&&f.previous_xp_calls==0,"existing XP relay ownership cannot duplicate native/custom HUD output");
  require(f.get<int>(Slot+hud::channel_current_id)==other_weapon_hud_bank,"a current-icon replacement made by the native callback is preserved");
  auto expected=f.cached_original;expected[cached_image_index]=foreign_images;
  require(f.get<HudTables>(Cached+hud::bank_header)==expected,"an overlay table replacement made during the draw is preserved");
  f.put(TextBank,ForeignName);presentation.names();presentation.select_identity(false);
  require(f.get<unsigned>(TextBank)==ForeignName,"a newer localized-name owner becomes the stock identity baseline");
  f.put(Slot+hud::channel_callback,ForeignCode);presentation.reset();
  require(f.get<unsigned>(Slot+hud::channel_callback)==ForeignCode&&f.get<unsigned>(TextBank)==ForeignName,"teardown preserves later callback and name owners");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const auto copied_images=f.get<unsigned>(State+hud::image_pages),copied_definitions=f.get<unsigned>(State+hud::animation_definitions);
  f.put(State+hud::frame_references,foreign_references);presentation.reset();
  require(f.get<unsigned>(State+hud::frame_references)==foreign_references&&f.get<unsigned>(State+hud::image_pages)==copied_images&&f.get<unsigned>(State+hud::animation_definitions)==copied_definitions,
          "foreign reference tables cannot be left dangling into reclaimed appended pages");
 }
 {
  Fixture f;f.cache_available=false;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const auto low_memory=f.bytes<64>(0);presentation.draw_base(Slot,Fixture::previous_xp);
  require(f.bytes<64>(0)==low_memory&&f.native_calls==1,"an unavailable overlay cache never causes a low-address write");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  f.put(Slot+hud::channel_callback,ForeignCode);presentation.frame(true);
  require(!presentation.owns(Slot)&&f.get<unsigned>(Slot+hud::channel_callback)==ForeignCode,"frame updates never replace an unknown newer HUD callback");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  f.put<int>(Slot+hud::channel_current_id,other_weapon_hud_bank);
  f.put<int>(Slot+hud::channel_next_id,other_weapon_hud_bank);
  presentation.frame(false,false);
  require(f.get<unsigned char>(Slot+hud::channel_visibility)==fully_visible,
          "retiring the custom HUD preserves another weapon's native fade");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  f.put<int>(Slot+hud::channel_next_id,other_weapon_hud_bank);
  f.put<int>(Slot+hud::channel_pending,1);
  f.put<std::uint8_t>(Slot+hud::channel_text_fade,fully_visible);
  f.put<int>(Slot+hud::channel_display_timer,fully_visible);
  presentation.frame(false);
  require(f.get<std::uint8_t>(Slot+hud::channel_visibility)==0&&f.get<std::uint8_t>(Slot+hud::channel_text_fade)==0&&
          f.get<int>(Slot+hud::channel_display_timer)==0&&f.get<int>(Slot+hud::channel_pending)==1,
          "a newly queued weapon retires the active Revolverator fade and keeps the native queue");
  require(f.get<unsigned>(Slot+hud::channel_callback)==NativeCode&&f.get<int>(Slot+hud::channel_next_id)==other_weapon_hud_bank,
          "retiring an active tile restores the native callback and keeps the next item");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  f.put<int>(Slot+hud::channel_current_id,other_weapon_hud_bank);
  require(!presentation.owns(Slot),"a carrier request does not override an active stock weapon tile");
  presentation.frame(true);
  require(f.get<unsigned>(Slot+hud::channel_callback)==NativeCode&&
          f.get<unsigned char>(Slot+hud::channel_visibility)==fully_visible,
          "the active stock tile keeps its callback and fade while the carrier is queued");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const auto clone=f.get<HudTables>(State+hud::bank_header);
  f.put(Cached+hud::bank_header,clone);
  f.put<int>(item::game_mode,item::loading_mode);
  require(!presentation.owns(Slot),"the native callback keeps control during level loading");
  f.put<int>(quick_ring_type,quick_ring_mode);
  f.put<int>(quick_ring_fade_frames,fully_visible);
  require(presentation.draw_quick_selector(Slot,+[](unsigned){return quick_ring_draw_result;},
                                           {arsenal::RevolveratorKey})==quick_ring_draw_result&&f.sprites.empty(),
          "loading leaves the native quick-selector draw in control");
  const auto text_entry=f.get<unsigned>(TextBank);
  std::memset(f.ram.data()+f.arena.begin(),0x9d,f.arena.used());
  const auto incoming=std::vector<unsigned char>(f.ram.begin()+f.arena.begin(),f.ram.begin()+f.arena.begin()+f.arena.used());
  const auto stock_header=f.bytes<hud::bank_header_words*sizeof(std::uint32_t)>(Header);
  const auto stock_definitions=f.bytes<32>(Defs);
  const auto stock_references=f.bytes<80>(Refs);
  const auto stock_images=f.bytes<2*texture_page_size>(Images);
  const auto stock_palettes=f.bytes<2*texture_page_size>(Palettes);
  presentation.reset();
  require(std::equal(incoming.begin(),incoming.end(),f.ram.begin()+f.arena.begin())&&
          f.get<unsigned>(TextBank)==text_entry,
          "loading cleanup avoids recycled level storage and retired text entries");
  require(f.bytes<hud::bank_header_words*sizeof(std::uint32_t)>(Header)==stock_header&&
          f.bytes<32>(Defs)==stock_definitions&&f.bytes<80>(Refs)==stock_references&&
          f.bytes<2*texture_page_size>(Images)==stock_images&&f.bytes<2*texture_page_size>(Palettes)==stock_palettes,
          "loading cleanup never merges input bytes into native sprite tables");
  require(f.get<HudTables>(State+hud::bank_header)==stock_tables&&f.get<HudTables>(Cached+hud::bank_header)==stock_tables,
          "loading cleanup withdraws owned pointers from both HUD state views");
 }
 {
  Fixture f;Presentation presentation(&f.api,nullptr,&f.arena);presentation.frame(true);
  const unsigned used=f.arena.used();
  constexpr unsigned respawn_cycles = 30;
  constexpr std::uint16_t resident_sprite_gs_block = 42;
  constexpr auto gs_block_field = offsetof(hud::TexturePage,gs_block_offset);
  for(unsigned death=0;death<respawn_cycles;++death){
   const auto custom=f.get<HudTables>(State+hud::bank_header);
   f.put(Cached+hud::bank_header,custom);
   f.put(custom[cached_image_index]+gs_block_field,resident_sprite_gs_block);
   f.put<unsigned char>(Slot+hud::channel_visibility,fully_visible);
   f.put<int>(Slot+hud::channel_display_timer,fully_visible);
   presentation.frame(false,false);
   require(f.get<unsigned char>(Slot+hud::channel_visibility)==0&&f.get<int>(Slot+hud::channel_display_timer)==0,
           "death retires the cartridge fade before withdrawing its sprite bank");
   require(f.get<HudTables>(State+hud::bank_header)==stock_tables&&
           f.get<HudTables>(Cached+hud::bank_header)==stock_tables,
           "the checkpoint owns the original sprite tables throughout death");
   require(f.get<unsigned>(Slot+hud::channel_callback)==NativeCode&&
           f.get<std::uint16_t>(Images+gs_block_field)==resident_sprite_gs_block,
           "the native HUD relay and resident sprite upload survive suspension");
   presentation.frame(false,false);
   presentation.frame(true,true);
   require(presentation.owns(Slot)&&f.arena.used()==used,
           "respawn reattaches the cartridge HUD without consuming another arena allocation");
  }
 }
 std::printf("presentation: %u scheduler, native ammo, XP relay, localized-name and table-ownership checks passed\n",checks);
}

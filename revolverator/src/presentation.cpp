// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "presentation.hpp"
#include "hud_table_scope.hpp"
#include "native_ammunition.hpp"
#include "eqs_probe.hpp"
#include "native_icon_variants.hpp"
#include "localization.hpp"
#include "ammunition_icon_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef REVOLVERATOR_HD_ICON
#include "revolverator_hd_icon_data.hpp"
#endif
namespace revolverator {
namespace {
using namespace native_hud;

constexpr int walloper_hud_bank=weapon_hud_bank_base+NativeAmmunition::item_id;
constexpr unsigned weapon_tile_text_color=ammo_fade_frames+0x10;
constexpr unsigned pal_mode_address=0x15ed80;
constexpr int ammo_tile_pal_y=10;
constexpr int ammo_tile_ntsc_y=18;
constexpr unsigned color_rgb_mask=0x00ffffff;
constexpr unsigned color_alpha_shift=24;

bool active_ammo_tile(int bank){
 return bank==walloper_hud_bank||bank==animation_terminator;
}

// Canonical resident relay addresses, checked against the running function table.
// Source: OpenRAC's NTSC-U bindings; the same callback technique as Weapon XP.
constexpr std::array<unsigned,native_arsenal_layout::last_campaign_level+1> relays{{
 0x2a5dd8,0x2bb758,0x2a5d10,0x296298,0x297fe0,0x2cfce0,0x2b2dc8,0x2ceee8,0x2ae8b8,0x2c3fe8,
 0x296cc0,0x2c9cc0,0x2bd798,0x2b2038,0x2afe30,0x293c18,0x29ec40,0x29df30,0x2a5878}};
}
Presentation::Presentation(const openrac_api* a,openrac_mod* m,GuestArena* arena,bool hud_fallback):Memory(a,arena),hd_(a,arena,hud_fallback),
 hud_(function<Hud>("FUN_L00_0023a7a8")),
 rectangle_(function<Rectangle>("FUN_00200e08")),quad_(function<Quad>("FUN_001f5450")),texture_(function<Texture>("FUN_001f44b8")),
 relay_(find_function("FUN_L00_002a5dd8")),enabled_(a->setting_bool(m,"hud")!=0){hud_fallback_=hud_fallback;}
Presentation::~Presentation(){reset();}
void Presentation::names(){
 if(level_transitioning())return;
 const auto bank=read<unsigned>(global(0x15f6a0));const int count=read<int>(global(0x1996fc));
 if(!Memory::valid(bank,16)||count<=0||count>20000||!Memory::valid(bank,std::size_t(count)*16))return;
 if(bank_==bank){apply_identity();return;}
 if(bank_)reset();
 bank_=bank;level_=api_->loaded_level();
 arena_=allocate_guest(512);if(!Memory::valid(arena_,512)){arena_=0;bank_=0;return;}
 const auto& translated=localization::strings_for_language(read<int>(global(localization::native_language_address)));
 const char* strings[]={translated.weapon_name,translated.description,translated.weapon_name,translated.weapon_name};
 const auto defs=global(0x1863d0);const int ids[]={read<int>(defs+18*76),read<short>(defs+18*76+0x40),read<short>(defs+18*76+0x46),read<short>(defs+18*76+0x48)};
 for(unsigned n=0;n<4;++n){
  if(ids[n]<=0)continue;
  for(int i=0;i<count;++i){auto entry=bank+unsigned(i)*16;if(read<int>(entry+4)!=ids[n])continue;
   if(std::any_of(bindings_.begin(),bindings_.end(),[entry](const Binding& binding){return binding.entry==entry;}))break;
   const auto original=read<unsigned>(entry),replacement=arena_+(n==1?256:0);
   if(!Memory::valid(original,1))break;
   std::memcpy(api_->memory+replacement,strings[n],std::strlen(strings[n])+1);
   bindings_.push_back({entry,original,replacement});break;
  }
 }
 apply_identity();
}
void Presentation::select_identity(bool revolverator){revolverator_identity_=revolverator;names();}
void Presentation::set_experience(bool enabled,unsigned level,float progress,bool versioned_names){
 experience_enabled_=enabled;experience_level_=std::clamp(level,1u,10u);
 experience_progress_=std::isfinite(progress)?std::clamp(progress,0.f,1.f):0;
 versioned_names_=versioned_names;
}
bool Presentation::notify_acquisition(unsigned level){
 if(level_transitioning())return false;
 if(level==1||level>10)return false;
 const auto notify=reinterpret_cast<void(*)(int,int)>(find_function("FUN_L00_00263d40"));
 if(!notify)return false;
 names();
 if(!arena_||api_->loaded_level()!=level_)return false;
 const int id=read<int>(global(0x1863d0)+18*76);
 const auto binding=std::find_if(bindings_.begin(),bindings_.end(),[&](const Binding& b){return read<int>(b.entry+4)==id;});
 if(id<=0||binding==bindings_.end())return false;
 const auto entry=binding->entry,replacement=binding->replacement,previous=read<unsigned>(entry);
 std::array<unsigned char,256> previous_name{};
 std::memcpy(previous_name.data(),api_->memory+replacement,previous_name.size());
 const auto version=level?level:experience_level_;
 const auto localized=localization::weapon_name_for_language(read<int>(global(localization::native_language_address)),golden_identity_);
 char name[256];
 if(versioned_names_&&version>1)std::snprintf(name,sizeof name,"%s v%u",localized,version);
 else std::snprintf(name,sizeof name,"%s",localized);
 std::memcpy(api_->memory+replacement,name,std::strlen(name)+1);
 write(entry,replacement);
 // This native call copies the string into the stock subtitle buffer, so the
 // logical Walloper/name projection can be restored immediately afterward.
 try{notify(id,180);}catch(...){
  if(read<unsigned>(entry)==replacement)write(entry,previous);
  std::memcpy(api_->memory+replacement,previous_name.data(),previous_name.size());throw;
 }
 if(read<unsigned>(entry)==replacement)write(entry,previous);
 std::memcpy(api_->memory+replacement,previous_name.data(),previous_name.size());
 return true;
}
void Presentation::apply_identity(){
 if(level_transitioning()||api_->loaded_level()!=level_)return;
 if(arena_){
  const int language=read<int>(global(localization::native_language_address));
  const char* localized_name=localization::weapon_name_for_language(language,golden_identity_);
  char name[256];
  if(versioned_names_&&experience_level_>1)std::snprintf(name,sizeof name,"%s v%u",localized_name,experience_level_);
  else std::snprintf(name,sizeof name,"%s",localized_name);
  std::memcpy(api_->memory+arena_,name,std::strlen(name)+1);
  const char* description=localization::description_for_language(language,golden_identity_);
  std::memcpy(api_->memory+arena_+256,description,std::strlen(description)+1);
 }
 for(auto& binding:bindings_){
  const auto current=read<unsigned>(binding.entry);
  if(current!=binding.original&&current!=binding.replacement)binding.original=current;
  write(binding.entry,revolverator_identity_?binding.replacement:binding.original);
 }
}
void Presentation::frame(bool equipped,bool alive){
 if(level_transitioning())return;
 if(!alive){reset_icons(true);return;}
 names();const int current=api_->loaded_level();if(current<0||unsigned(current)>=relays.size())return;
 bind_icon();
 hd_.frame();
 // NativeAmmunition owns the native ammo price/capacity fields and enables the
 // game's own ammo HUD. Presentation never overwrites those economy values.
 const auto slot=global(channel_slots);
 // Native ammo HUD requests60000+item, independently of the item-definition
 // icon. Keep its scheduler's18 identity and scope the render-only bank below.
 const int active_bank=read<int>(slot+channel_current_id);
 if(!equipped||!icon_copy_||!Memory::valid(slot,channel_size)||read<int>(slot+channel_next_id)!=walloper_hud_bank||
     !active_ammo_tile(active_bank)||(read<int>(slot+channel_pending)!=0&&active_bank!=walloper_hud_bank)){
  detach_ammo_hud();
  return;
 }
 const auto callback=read<unsigned>(slot+channel_callback),relay=guest_callback(unsigned(current),relays[unsigned(current)]);
 if(!relay||!relay_||api_->function_at(relay)!=relay_)return;
 if(callback==relay){slot_=slot;callback_=relay;return;}
 if(api_->function_at(callback)!=reinterpret_cast<void*>(hud_))return;
 slot_=slot;old_callback_=callback;callback_=relay;write(slot+channel_callback,relay);
}
void Presentation::bind_icon(){
 const auto state=global(resident_state);
 const auto defs=read<unsigned>(state+animation_definitions),refs=read<unsigned>(state+frame_references);
 if(icon_copy_||!Memory::valid(defs,animation_entry_bytes)||!Memory::valid(refs,frame_reference_bytes))return;
 unsigned count=0,definitions=0;bool terminated=false;
 AnimationDefinition drill{};
 for(unsigned i=0;i<max_animation_definitions;++i){
  const auto entry=defs+i*animation_entry_bytes;if(!Memory::valid(entry,animation_entry_bytes))return;
  const auto animation=read<AnimationDefinition>(entry);
  if(animation.id==custom_icon_bank)return;
  if(animation.id==animation_terminator){definitions=i;terminated=true;break;}
  const unsigned end=unsigned(animation.start)+animation.count;
  count=std::max(count,end);
  if(animation.id==drill_icon_bank)drill=animation;
 }
 const unsigned original_reference_bytes=count*frame_reference_bytes;
 if(!terminated||!definitions||drill.count<=drill_icon_frame||count==0||count>max_frame_references||
    !Memory::valid(refs,original_reference_bytes))return;
 const unsigned reference_bytes=original_reference_bytes+custom_icon_frame_count*frame_reference_bytes;
 const unsigned original_definition_bytes=definitions*animation_entry_bytes;
 // Reserve one entry for the custom animation and one for its terminator.
 const unsigned definition_bytes=original_definition_bytes+2*animation_entry_bytes;
 const unsigned size=reference_bytes+definition_bytes;
 const bool reuse=guest_arena_&&size<=spare_icon_capacity_&&guest_arena_->owns(spare_icons_,spare_icon_capacity_);
 const auto copy=reuse?spare_icons_:allocate_guest(size);if(!Memory::valid(copy,size))return;
 const auto definitions_copy=copy+reference_bytes;
 std::memcpy(api_->memory+copy,api_->memory+refs,original_reference_bytes);
 icon_baseline_.resize(count);
 std::memcpy(icon_baseline_.data(),api_->memory+refs,original_reference_bytes);
 // Append a separate icon definition. Walloper's bank and its two original
 // frames remain intact; HUD, quick select and gold/locked menu views can
 // request frame4 as well as the ordinary ammo frame3.
 const auto source=unsigned(drill.start)+drill_icon_frame;
 const auto drill_reference=read<unsigned>(refs+source*frame_reference_bytes);
 for(unsigned i=0;i<custom_icon_frame_count;++i){
  // Exact native atlas glyph selected by the user, checked at full resolution:
  // bank 59901, frame 13 (the highlighted white spiral/cone symbol).
  write(copy+(count+i)*frame_reference_bytes,drill_reference);
 }
 std::memcpy(api_->memory+definitions_copy,api_->memory+defs,original_definition_bytes);
 icon_definitions_baseline_.resize(definitions);
 std::memcpy(icon_definitions_baseline_.data(),api_->memory+defs,original_definition_bytes);
 const auto extra=definitions_copy+original_definition_bytes;
 write(extra,AnimationDefinition{custom_icon_bank,custom_icon_frame_count,static_cast<std::uint16_t>(count),0,0});
 write(extra+animation_entry_bytes,AnimationDefinition{animation_terminator,0,0,0,0});
 icon_state_=state;icon_defs_=defs;icon_refs_=refs;icon_copy_=copy;
 icon_capacity_=reuse?spare_icon_capacity_:size;spare_icons_=spare_icon_capacity_=0;
 icon_defs_copy_=definitions_copy;icon_defs_count_=definitions;
 write(state+animation_definitions,definitions_copy);
 write(state+frame_references,copy);
 const auto ammunition=generated::ammunition_icon_data();
 const auto install=[&](const HdIconData& normal){
  NativeIconVariants variants;
  const bool complete=variants.build(*this,normal);
  const auto clean=variants.normal(),vendor_ammo=variants.ammo(),golden=variants.golden();
  return hd_.install(count,custom_icon_frame_count,complete?clean:normal,&ammunition,
                     complete?&vendor_ammo:nullptr,complete?&golden:nullptr);
 };
 bool installed=false;
#ifdef REVOLVERATOR_HD_ICON
 installed=install(generated::hd_icon_data());
#endif
 // Source-only builds keep the player's original 32px weapon glyph. The
 // ammunition tile is original authored art and ships with the source.
 const auto frame=read<FrameReference>(refs+source*frame_reference_bytes);
 const auto images=read<unsigned>(state+image_pages),palettes=read<unsigned>(state+palette_pages);
 const auto page=read<TexturePage>(images+std::uint16_t(frame.image_index)*texture_page_bytes);
 const auto clut=read<unsigned>(palettes+std::uint16_t(frame.palette_index)*texture_page_bytes);
 constexpr unsigned native_pixel_bytes=HdIcon::logical_size*HdIcon::logical_size;
 if(!installed&&Memory::valid(page.source_address,native_pixel_bytes)&&Memory::valid(clut,palette_bytes)&&
    page.width_log2==native_texture_log2&&page.height_log2==native_texture_log2){
  const HdIconData icon{api_->memory+page.source_address,native_pixel_bytes,api_->memory+clut,palette_bytes};
  install(icon);
 }
}
bool Presentation::owns(unsigned e)const{
 return slot_==e&&callback_&&api_->loaded_level()==level_&&!level_transitioning()&&
        read<unsigned>(e+channel_callback)==callback_&&read<int>(e+channel_next_id)==walloper_hud_bank&&
        active_ammo_tile(read<int>(e+channel_current_id));
}
void Presentation::draw_base(unsigned e,void(*previous_relay)(unsigned),std::optional<unsigned> vendor_ammo){
 (void)previous_relay;
 // EQS can upload another wheel's atlas before the ammunition HUD draws.
 if(eqs_probe::installed())hd_.frame();
 HudTableScope tables(*this);
 const int before=read<int>(e+channel_current_id);
 // The stock bank can have no suitable HUD frame. The selector and callback
 // own this draw even if native initialization resolved the active ID to FFFF.
 const bool custom=owns(e)||before==walloper_hud_bank;
 if(custom)write<int>(e+channel_current_id,custom_icon_bank);
 const int previous_ammo=read<int>(e+channel_ammunition);
 // During the vendor fade the cursor can already have selected a stock item.
 // The counter still represents the custom purchase until its native draw ends.
 if(custom&&vendor_ammo)write<int>(e+channel_ammunition,int(*vendor_ammo));
 // The shared relay may already belong to native Walloper XP decorations.
 // Custom progression owns its own decorations, so draw the native base once.
 hud_(e);
 if(custom&&vendor_ammo&&read<int>(e+channel_ammunition)==int(*vendor_ammo))write(e+channel_ammunition,previous_ammo);
 if(custom&&read<int>(e+channel_current_id)==int(custom_icon_bank))write(e+channel_current_id,before);
}
int Presentation::draw_quick_selector(unsigned e,int(*original)(unsigned),const std::array<arsenal::WeaponKey,8>& bindings){
 if(!original)return 0;
 if(level_transitioning()||read<int>(level(0x15fa90))!=8)return original(e);
 HudTableScope tables(*this);
 const bool hidden=hd_.before_base();const int result=original(e);hd_.after_base();
 if(!hidden)return result;
 const int timer=read<int>(level(0x15f780));if(timer<=0)return result;
 const float open=std::clamp(read<unsigned char>(e+channel_visibility)/float(timer),0.f,1.f);
 const int alpha=int(open*max_alpha);if(alpha<=0)return result;
 int x=read<int>(e+channel_x),y=read<int>(e+channel_y);unsigned saved=0;
 const auto stack=api_->stack_push(16,&saved);
 if(stack){
  std::memcpy(api_->memory+stack,&x,4);std::memcpy(api_->memory+stack+4,&y,4);
  function<int(*)(unsigned,unsigned,unsigned)>("FUN_L00_00235a70")(e,stack,stack+4);
  std::memcpy(&x,api_->memory+stack,4);std::memcpy(&y,api_->memory+stack+4,4);api_->stack_pop(saved);
 }
 const auto cosine=function<float(*)(float)>("FUN_001f9dc8"),sine=function<float(*)(float)>("FUN_001f9de0");
 const auto add=function<float(*)(float,float)>("FUN_001fa580");
 for(unsigned i=0;i<bindings.size();++i){
  if(bindings[i]!=arsenal::RevolveratorKey)continue;
  const float angle=add((float(i)+float(i))*3.1415927f/8.f-3.1415927f,1.5707964f);
  const int px=x+read<int>(level(0x15f720))+int(read<float>(level(0x15f78c))*cosine(angle)*read<float>(level(0x15f7c4)));
  const int py=y+read<int>(level(0x15f724))+int(read<float>(level(0x15f78c))*sine(angle));
  const int a=read<int>(e+0x74)==int(i)?alpha:int(float(alpha)*read<float>(level(0x15f7c0)));
  hd_.draw(px+read<short>(e+0x48)-16,py+read<short>(e+0x4a)-16,a);
 }
 return result;
}
void Presentation::rect(int x,int y,int w,int h,unsigned color)const{if(w>0&&h>0)rectangle_(x,y,x+w,y+h,color,0);}
void Presentation::text(int x,int y,const char* s,unsigned color,int pixels)const{
 const auto metrics=global(0x1df3f0);if(!Memory::valid(metrics,0x400))return;
 const auto tex=texture_(2);float cursor=float(x),scale=pixels/16.f;
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s);*p;++p){
  if(*p<32||*p>=128)continue;
  const auto glyph=metrics+*p*4;
  const int top=read<signed char>(glyph+2),advance=read<signed char>(glyph+3);
  if(*p!=32&&advance>0)quad_(int(std::round(cursor)),y+int(std::round(top*scale)),pixels,pixels,read<unsigned char>(glyph),read<unsigned char>(glyph+1),16,16,color,tex);
  cursor+=advance*scale;
 }
}
void Presentation::after_hud(unsigned e,const Controller& c){
 if(format_changed_){write(level(ammo_format),format_);format_changed_=false;}
 if(!owns(e))return;
 const int timer=read<int>(level(ammo_fade_frames));if(timer<=0)return;
 const float visibility=std::clamp(read<unsigned char>(e+channel_visibility)/float(timer),0.f,1.f);
 const unsigned alpha=unsigned(visibility*max_alpha);if(!alpha)return;
 const int x=read<int>(e+channel_x),y=read<int>(global(pal_mode_address))?ammo_tile_pal_y:ammo_tile_ntsc_y;
 const unsigned rgb=read<unsigned>(level(weapon_tile_text_color))&color_rgb_mask;
 const unsigned color=rgb|(alpha<<color_alpha_shift);
 // Native frame3 is the separate32px cartridge icon. The weapon glyph is
 // reserved for inventory and quick select; native HUD draws ammunition.
 if(experience_enabled_){
  char label[16];std::snprintf(label,sizeof label,"LV %u",experience_level_);
  text(x+35,y+22,label,(alpha*3/4)<<color_alpha_shift,10);text(x+34,y+21,label,color,10);
  const int left=x+68,width=37;
  rect(left+1,y+24,width-2,1,rgb|((alpha/3)<<color_alpha_shift));
  rect(left,y+25,width,3,rgb|((alpha/3)<<color_alpha_shift));
  rect(left+1,y+28,width-2,1,rgb|((alpha/5)<<color_alpha_shift));
  rect(left+1,y+25,width-2,3,(alpha*3/4)<<color_alpha_shift);
  rect(left+1,y+25,int(std::round((width-2)*experience_progress_)),3,color);
 }
 if(!enabled_||read<int>(level(native_arsenal_layout::game_mode))!=native_arsenal_layout::gameplay_mode||
    c.phase()!=Phase::Spinning)return;
 const int left=x+(experience_enabled_?68:35),width=experience_enabled_?37:70;
 // A fine charge channel inside the original tile, below the native ammo
 // number and above Weapon XP's existing bottom channel. No added panel.
 const int height=experience_enabled_?2:4;
 rect(left,y+21,width,height,rgb|((alpha/3)<<color_alpha_shift));
 rect(left+1,y+22,width-2,height-1,(alpha*3/4)<<color_alpha_shift);
 // The last pixel is a gameplay promise: show it only when a release can
 // actually throw. Rounding a partial charge made the bar look full early.
 const float charge=c.charge();const int maximum_fill=width-2;
 const int fill=charge>=1.f?maximum_fill:std::clamp(int(maximum_fill*charge),0,maximum_fill-1);
 rect(left+1,y+22,fill,height-1,color);
}
void Presentation::detach_ammo_hud(){
 if(slot_&&api_->loaded_level()==level_&&read<unsigned>(slot_+channel_callback)==callback_){
  const int active_bank=read<int>(slot_+channel_current_id);
  const bool departing=active_bank==walloper_hud_bank||
     (active_bank==animation_terminator&&read<int>(slot_+channel_next_id)==walloper_hud_bank);
  if(departing&&!level_transitioning()){
    // Fade belongs to the active tile. The native queue may already hold the
    // next weapon, which keeps its pending flag and all other scheduler fields.
    write<std::uint8_t>(slot_+channel_visibility,0);
    write<std::uint8_t>(slot_+channel_text_fade,0);
    write<int>(slot_+channel_display_timer,0);
  }
  if(old_callback_)write(slot_+channel_callback,old_callback_);
 }
 slot_=old_callback_=callback_=0;
}
void Presentation::reset_icons(bool reuse_storage){
 detach_ammo_hud();
 hd_.reset(reuse_storage);
 if(!reuse_storage)spare_icons_=spare_icon_capacity_=0;
 if(api_->loaded_level()==level_){
  if(icon_copy_&&read<unsigned>(icon_state_+native_hud::frame_references)==icon_copy_&&read<unsigned>(icon_state_+native_hud::animation_definitions)==icon_defs_copy_){
   if(!level_transitioning()){
    for(unsigned i=0;i<icon_baseline_.size();++i){
     const unsigned offset=i*frame_reference_bytes;
     const unsigned at=icon_refs_+offset;
     if(read<unsigned>(at)==icon_baseline_[i])write(at,read<unsigned>(icon_copy_+offset));
    }
    for(unsigned i=0;i<icon_defs_count_;++i){
     const unsigned offset=i*animation_entry_bytes;
     const auto at=icon_defs_+offset;
     if(read<std::uint64_t>(at)==icon_definitions_baseline_[i])write(at,read<std::uint64_t>(icon_defs_copy_+offset));
    }
    if(reuse_storage&&guest_arena_&&guest_arena_->owns(icon_copy_,icon_capacity_)){
     spare_icons_=icon_copy_;spare_icon_capacity_=icon_capacity_;
    }
   }
   // The resident and overlay pointers can still be ours after the loader
   // has reused the level arena. Withdraw them without merging stale data.
   write(icon_state_+native_hud::frame_references,icon_refs_);write(icon_state_+native_hud::animation_definitions,icon_defs_);
   restore_cached_hud_pointer(*this,native_hud::frame_references,icon_copy_,icon_refs_);
   restore_cached_hud_pointer(*this,native_hud::animation_definitions,icon_defs_copy_,icon_defs_);
  }
 }
 icon_state_=icon_defs_=icon_refs_=icon_copy_=icon_defs_copy_=icon_defs_count_=0;
 icon_capacity_=0;
 icon_baseline_.clear();icon_definitions_baseline_.clear();
}
void Presentation::reset(){
 reset_icons(false);
 if(api_->loaded_level()==level_&&!level_transitioning()){
  if(format_changed_)write(level(ammo_format),format_);
  if(table_&&read<unsigned short>(table_+8)==1)write(table_+8,old_hud_flag_);
  for(const auto& b:bindings_)if(read<unsigned>(b.entry)==b.replacement)write(b.entry,b.original);
 }
 bindings_.clear();slot_=old_callback_=callback_=arena_=bank_=table_=0;level_=-1;format_changed_=false;
}
}

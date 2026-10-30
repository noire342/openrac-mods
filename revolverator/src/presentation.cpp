// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "presentation.hpp"
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
// Canonical resident relay addresses, checked against the running function table.
// Source: OpenRAC's NTSC-U bindings; the same callback technique as Weapon XP.
constexpr std::array<unsigned,19> relays{{0x2a5dd8,0x2bb758,0x2a5d10,0x296298,0x297fe0,0x2cfce0,0x2b2dc8,0x2ceee8,0x2ae8b8,0x2c3fe8,0x296cc0,0x2c9cc0,0x2bd798,0x2b2038,0x2afe30,0x293c18,0x29ec40,0x29df30,0x2a5878}};
// hud_frame reads the overlay's cached tables, while get_icon_frame and the
// sprite renderer read the resident tables. Keep both views coherent during
// each native draw; appended frame indices must never index the old tables.
class HudTableScope {
 const Memory& memory_;unsigned overlay_;std::array<unsigned,5> before_{},applied_{};
public:
 explicit HudTableScope(const Memory& memory):memory_(memory),overlay_(memory.level(0x17e4d8)){
  const auto resident=memory.global(0x19a3e8);
  if(!Memory::valid(overlay_+0x18,20)||!Memory::valid(resident+0x18,20)){overlay_=0;return;}
  before_=memory.read<decltype(before_)>(overlay_+0x18);applied_=memory.read<decltype(applied_)>(resident+0x18);
  memory.write(overlay_+0x18,applied_);
 }
 ~HudTableScope(){if(overlay_)for(unsigned i=0;i<before_.size();++i)
  if(memory_.read<unsigned>(overlay_+0x18+i*4)==applied_[i])memory_.write(overlay_+0x18+i*4,before_[i]);}
};
}
Presentation::Presentation(const openrac_api* a,openrac_mod* m,GuestArena* arena,bool hud_fallback):Memory(a,arena),hd_(a,arena,hud_fallback),
 hud_(function<Hud>("FUN_L00_0023a7a8")),
 rectangle_(function<Rectangle>("FUN_00200e08")),quad_(function<Quad>("FUN_001f5450")),texture_(function<Texture>("FUN_001f44b8")),
 relay_(a->function("FUN_L00_002a5dd8")),enabled_(a->setting_bool(m,"hud")!=0){hud_fallback_=hud_fallback;}
Presentation::~Presentation(){reset();}
void Presentation::names(){
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
void Presentation::apply_identity(){
 if(arena_){
  const auto& translated=localization::strings_for_language(read<int>(global(localization::native_language_address)));
  char name[256];
  if(versioned_names_&&experience_level_>1)std::snprintf(name,sizeof name,"%s v%u",translated.weapon_name,experience_level_);
  else std::snprintf(name,sizeof name,"%s",translated.weapon_name);
  std::memcpy(api_->memory+arena_,name,std::strlen(name)+1);
 }
 for(auto& binding:bindings_){
  const auto current=read<unsigned>(binding.entry);
  if(current!=binding.original&&current!=binding.replacement)binding.original=current;
  write(binding.entry,revolverator_identity_?binding.replacement:binding.original);
 }
}
void Presentation::frame(bool equipped){
 names();const int current=api_->loaded_level();if(current<0||current>=19)return;
 bind_icon();
 hd_.frame();
 // NativeAmmunition owns the native ammo price/capacity fields and enables the
 // game's own ammo HUD. Presentation never overwrites those economy values.
 if(!equipped)return;
 const auto slot=global(0x199b60);
 // Native ammo HUD requests60000+item, independently of the item-definition
 // icon. Keep its scheduler's18 identity and scope the render-only bank below.
 if(!Memory::valid(slot,0x90)||read<int>(slot+0x20)!=60018||read<int>(slot+0x68)!=0)return;
 const auto callback=read<unsigned>(slot+0x18),relay=relays[unsigned(current)];
 if(api_->function_at(relay)!=relay_)return;
 if(callback==relay){slot_=slot;callback_=relay;return;}
 if(api_->function_at(callback)!=reinterpret_cast<void*>(hud_))return;
 slot_=slot;old_callback_=callback;callback_=relay;write(slot+0x18,relay);
}
void Presentation::bind_icon(){
 const auto state=global(0x19a3e8),defs=read<unsigned>(state+0x1c),refs=read<unsigned>(state+0x20);
 if(icon_copy_||!Memory::valid(defs,8)||!Memory::valid(refs,4))return;
 unsigned count=0,definitions=0,drill_icon_def=0;bool terminated=false;
 for(unsigned i=0;i<1024;++i){
  const auto entry=defs+i*8;if(!Memory::valid(entry,8))return;
  const auto id=read<unsigned short>(entry);if(id==custom_icon_bank)return;
  if(id==0xffff){definitions=i;terminated=true;break;}
  const unsigned end=read<unsigned short>(entry+4)+unsigned(read<unsigned short>(entry+2));
  count=std::max(count,end);
  if(id==59901)drill_icon_def=entry;
 }
 if(!terminated||!definitions||!drill_icon_def||count==0||count>16000||!Memory::valid(refs,count*4))return;
 const auto copy=allocate_guest((count+5)*4);if(!Memory::valid(copy,(count+5)*4))return;
 const auto definitions_copy=allocate_guest((definitions+2)*8);if(!Memory::valid(definitions_copy,(definitions+2)*8))return;
 std::memcpy(api_->memory+copy,api_->memory+refs,count*4);
 icon_baseline_.resize(count);
 std::memcpy(icon_baseline_.data(),api_->memory+refs,count*4);
 // Append a separate icon definition. Walloper's bank and its two original
 // frames remain intact; HUD, quick select and gold/locked menu views can
 // request frame4 as well as the ordinary ammo frame3.
 const bool has_drill=drill_icon_def&&read<unsigned short>(drill_icon_def+2)>13;
 if(!has_drill)return;
 const auto source=unsigned(read<unsigned short>(drill_icon_def+4))+13;
 for(unsigned i=0;i<5;++i){
  // Exact native atlas glyph selected by the user, checked at full resolution:
  // bank 59901, frame 13 (the highlighted white spiral/cone symbol).
  write(copy+(count+i)*4,read<unsigned>(refs+source*4));
 }
 std::memcpy(api_->memory+definitions_copy,api_->memory+defs,definitions*8);
 icon_definitions_baseline_.resize(definitions);
 std::memcpy(icon_definitions_baseline_.data(),api_->memory+defs,definitions*8);
 const auto extra=definitions_copy+definitions*8;
 write<unsigned short>(extra,custom_icon_bank);write<unsigned short>(extra+2,5);
 write<unsigned short>(extra+4,static_cast<unsigned short>(count));write<unsigned short>(extra+6,0);
 write<std::uint64_t>(extra+8,0xffff);
 icon_state_=state;icon_defs_=defs;icon_refs_=refs;icon_copy_=copy;
 icon_defs_copy_=definitions_copy;icon_defs_count_=definitions;
 write(state+0x1c,definitions_copy);
 write(state+0x20,copy);
 const auto ammunition=generated::ammunition_icon_data();
 bool installed=false;
#ifdef REVOLVERATOR_HD_ICON
 installed=hd_.install(count,5,generated::hd_icon_data(),&ammunition);
#endif
 // Source-only builds keep the player's original 32px weapon glyph. The
 // ammunition tile is original authored art and ships with the source.
 const auto frame=read<unsigned>(refs+source*4),images=read<unsigned>(state+0x24),palettes=read<unsigned>(state+0x28);
 const auto page=images+(frame>>16)*8,palette=palettes+(frame&0xffffu)*8;
 const auto pixels=read<unsigned>(page),clut=read<unsigned>(palette);
 if(!installed&&Memory::valid(pixels,1024)&&Memory::valid(clut,1024)&&read<unsigned short>(page+6)==0x0505){
  const HdIconData drill{api_->memory+pixels,1024,api_->memory+clut,1024};
  hd_.install(count,5,drill,&ammunition);
 }
}
bool Presentation::owns(unsigned e)const{return slot_==e&&callback_&&api_->loaded_level()==level_&&read<unsigned>(e+0x18)==callback_&&read<int>(e+0x20)==60018;}
void Presentation::before_hud(unsigned){
 // Preserve the native formatted ammo numerator/denominator.
}
void Presentation::draw_base(unsigned e,void(*previous_relay)(unsigned)){
 (void)previous_relay;
 HudTableScope tables(*this);
 const int before=read<int>(e);
 if(before==60018)write<int>(e,custom_icon_bank);
 // The shared relay may already belong to native Walloper XP decorations.
 // Custom progression owns its own decorations, so draw the native base once.
 hud_(e);
 if(before==60018&&read<int>(e)==int(custom_icon_bank))write<int>(e,before);
}
int Presentation::draw_quick_selector(unsigned e,int(*original)(unsigned),const std::array<arsenal::WeaponKey,8>& bindings){
 if(!original)return 0;
 if(read<int>(level(0x15fa90))!=8)return original(e);
 HudTableScope tables(*this);
 const bool hidden=hd_.before_base();const int result=original(e);hd_.after_base();
 if(!hidden)return result;
 const int timer=read<int>(level(0x15f780));if(timer<=0)return result;
 const float open=std::clamp(read<unsigned char>(e+0x70)/float(timer),0.f,1.f);
 const int alpha=int(open*128);if(alpha<=0)return result;
 int x=read<int>(e+0x50),y=read<int>(e+0x54);unsigned saved=0;
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
 if(format_changed_){write(level(0x15f7c8),format_);format_changed_=false;}
 if(!owns(e))return;
 const int timer=read<int>(level(0x15f91c));if(timer<=0)return;
 const float visibility=std::clamp(read<unsigned char>(e+0x70)/float(timer),0.f,1.f);
 const unsigned alpha=unsigned(visibility*128);if(!alpha)return;
 const int x=read<int>(e+0x50),y=read<int>(global(0x15ed80))?10:18;
 const unsigned rgb=read<unsigned>(level(0x15f91c+16))&0xffffff;
 const unsigned color=rgb|(alpha<<24);
 // Native frame3 is the separate32px cartridge icon. The weapon glyph is
 // reserved for inventory and quick select; native HUD draws ammunition.
 if(experience_enabled_){
  char label[16];std::snprintf(label,sizeof label,"LV %u",experience_level_);
  text(x+35,y+22,label,(alpha*3/4)<<24,10);text(x+34,y+21,label,color,10);
  const int left=x+68,width=37;
  rect(left+1,y+24,width-2,1,rgb|((alpha/3)<<24));
  rect(left,y+25,width,3,rgb|((alpha/3)<<24));
  rect(left+1,y+28,width-2,1,rgb|((alpha/5)<<24));
  rect(left+1,y+25,width-2,3,(alpha*3/4)<<24);
  rect(left+1,y+25,int(std::round((width-2)*experience_progress_)),3,color);
 }
 if(!enabled_||read<int>(level(0x15f5c4))!=0||c.phase()!=Phase::Spinning)return;
 const int left=x+(experience_enabled_?68:35),width=experience_enabled_?37:70;
 // A fine charge channel inside the original tile, below the native ammo
 // number and above Weapon XP's existing bottom channel. No added panel.
 const int height=experience_enabled_?2:4;
 rect(left,y+21,width,height,rgb|((alpha/3)<<24));rect(left+1,y+22,width-2,height-1,(alpha*3/4)<<24);
 const int fill=int(std::round((width-2)*c.charge()));
 rect(left+1,y+22,fill,height-1,color);
}
void Presentation::reset(){
 hd_.reset();
 if(format_changed_&&api_->loaded_level()==level_)write(level(0x15f7c8),format_);
 if(api_->loaded_level()==level_){
  if(icon_copy_&&read<unsigned>(icon_state_+0x20)==icon_copy_&&read<unsigned>(icon_state_+0x1c)==icon_defs_copy_){
   for(unsigned i=0;i<icon_baseline_.size();++i){
    const unsigned at=icon_refs_+i*4;
    if(read<unsigned>(at)==icon_baseline_[i])write(at,read<unsigned>(icon_copy_+i*4));
   }
   for(unsigned i=0;i<icon_defs_count_;++i){
    const auto at=icon_defs_+i*8;
    if(read<std::uint64_t>(at)==icon_definitions_baseline_[i])write(at,read<std::uint64_t>(icon_defs_copy_+i*8));
   }
   write(icon_state_+0x20,icon_refs_);write(icon_state_+0x1c,icon_defs_);
  }
  if(slot_&&old_callback_&&read<unsigned>(slot_+0x18)==callback_)write(slot_+0x18,old_callback_);
  if(table_&&read<unsigned short>(table_+8)==1)write(table_+8,old_hud_flag_);
  for(const auto& b:bindings_)if(read<unsigned>(b.entry)==b.replacement)write(b.entry,b.original);
 }
 bindings_.clear();slot_=old_callback_=callback_=arena_=bank_=table_=0;level_=-1;format_changed_=false;
 icon_state_=icon_defs_=icon_refs_=icon_copy_=icon_defs_copy_=icon_defs_count_=0;
 icon_baseline_.clear();icon_definitions_baseline_.clear();
}
}

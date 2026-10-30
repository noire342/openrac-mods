// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "localization.hpp"
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
}
Presentation::Presentation(const openrac_api* a,openrac_mod* m,GuestArena* arena,bool hud_fallback):Memory(a,arena),hd_(a,arena),
 hud_(function<Hud>("FUN_L00_0023a7a8")),
 rectangle_(function<Rectangle>("FUN_00200e08")),quad_(function<Quad>("FUN_001f5450")),texture_(function<Texture>("FUN_001f44b8")),
 relay_(a->function("FUN_L00_002a5dd8")),enabled_(a->setting_bool(m,"hud")!=0){hud_fallback_=hud_fallback;}
Presentation::~Presentation(){reset();}
void Presentation::names(){
 const auto bank=read<unsigned>(global(0x15f6a0));const int count=read<int>(global(0x1996fc));
 if(!Memory::valid(bank,16)||count<=0||count>20000||!Memory::valid(bank,std::size_t(count)*16))return;
 if(bank_==bank)return;
 if(bank_)reset();bank_=bank;level_=api_->loaded_level();
 arena_=allocate_guest(512);if(!Memory::valid(arena_,512)){arena_=0;bank_=0;return;}
 const auto& translated=localization::strings_for_language(read<int>(global(localization::native_language_address)));
 const char* strings[]={translated.weapon_name,translated.description};
 const auto defs=global(0x1863d0);const int ids[]={read<int>(defs+18*76),read<short>(defs+18*76+0x40)};
 for(unsigned n=0;n<2;++n){
  if(ids[n]<=0)continue;
  for(int i=0;i<count;++i){auto entry=bank+unsigned(i)*16;if(read<int>(entry+4)!=ids[n])continue;
   const auto original=read<unsigned>(entry),replacement=arena_+n*256;
   if(!Memory::valid(original,1))break;
   std::memcpy(api_->memory+replacement,strings[n],std::strlen(strings[n])+1);
   write(entry,replacement);bindings_.push_back({entry,original,replacement});break;
  }
 }
}
void Presentation::frame(bool equipped){
 names();const int current=api_->loaded_level();if(current<0||current>=19)return;
 bind_icon();
 hd_.frame();
 // NativeAmmunition owns the native ammo price/capacity fields and enables the
 // game's own ammo HUD. Presentation never overwrites those economy values.
 if((!equipped||!enabled_)&&!hd_.ready())return;
 const auto slot=global(0x199b60);
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
 unsigned count=0,weapon_def=0,drill_icon_def=0;
 for(unsigned i=0;i<1024;++i){
  const auto entry=defs+i*8;if(!Memory::valid(entry,8))return;
  const auto id=read<unsigned short>(entry);if(id==0xffff)break;
  const unsigned end=read<unsigned short>(entry+4)+unsigned(read<unsigned short>(entry+2));
  count=std::max(count,end);if(id==60018)weapon_def=entry;
  if(id==59901)drill_icon_def=entry;
 }
 if(!weapon_def||count==0||count>16000||!Memory::valid(refs,count*4))return;
 const auto original_count=read<unsigned short>(weapon_def+2),original_start=read<unsigned short>(weapon_def+4);
 if(!original_count||original_start>=count)return;
 const auto copy=allocate_guest((count+4)*4);if(!Memory::valid(copy,(count+4)*4))return;
 std::memcpy(api_->memory+copy,api_->memory+refs,count*4);
 icon_baseline_.resize(count);
 std::memcpy(icon_baseline_.data(),api_->memory+refs,count*4);
 // Walloper never showed an ammo tile and has only two icon frames. The native
 // ammo tile asks for frame three. Give this item four valid references without
 // moving or changing any other weapon's frames.
 const bool has_drill=drill_icon_def&&read<unsigned short>(drill_icon_def+2)>13;
 const auto source=has_drill?unsigned(read<unsigned short>(drill_icon_def+4))+13:original_start;
 for(unsigned i=0;i<4;++i){
  // Exact native atlas glyph selected by the user, checked at full resolution:
  // bank 59901, frame 13 (the highlighted white spiral/cone symbol).
  write(copy+(count+i)*4,read<unsigned>(refs+source*4));
 }
 icon_state_=state;icon_defs_=weapon_def;icon_refs_=refs;icon_copy_=copy;
 icon_count_=original_count;icon_start_=original_start;
 write(weapon_def+2,static_cast<unsigned short>(4));write(weapon_def+4,static_cast<unsigned short>(count));
 write(state+0x20,copy);
#ifdef REVOLVERATOR_HD_ICON
 hd_.install(count,4,generated::hd_icon_data());
#endif
}
bool Presentation::owns(unsigned e)const{return slot_==e&&callback_&&api_->loaded_level()==level_&&read<unsigned>(e+0x18)==callback_&&read<int>(e+0x20)==60018;}
void Presentation::before_hud(unsigned){
 // Preserve the native formatted ammo numerator/denominator.
}
void Presentation::draw_base(unsigned e,void(*previous_relay)(unsigned)){
 hd_.before_base();
 if(old_callback_)hud_(e);else previous_relay(e);
 hd_.after_base();
}
void Presentation::rect(int x,int y,int w,int h,unsigned color)const{if(w>0&&h>0)rectangle_(x,y,x+w,y+h,color,0);}
void Presentation::text(int x,int y,const char* s,unsigned color,int pixels)const{
 const auto metrics=global(0x1df3f0);if(!Memory::valid(metrics,0x400))return;
 const auto tex=texture_(2);float cursor=float(x),scale=pixels/16.f;
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s);*p;++p){
  if(*p<32||*p>=128)continue;const auto glyph=metrics+*p*4;
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
 hd_.draw(x+read<short>(e+0x48),y+read<short>(e+0x4a),int(alpha));
 if(!enabled_||read<int>(level(0x15f5c4))!=0||c.phase()!=Phase::Spinning)return;
 const int left=x+35,width=70;
 // A fine charge channel inside the original tile, below the native ammo
 // number and above Weapon XP's existing bottom channel. No added panel.
 rect(left,y+21,width,4,rgb|((alpha/3)<<24));rect(left+1,y+22,width-2,2,(alpha*3/4)<<24);
 const int fill=int(std::round((width-2)*c.charge()));
 rect(left+1,y+22,fill,2,color);
}
void Presentation::reset(){
 hd_.reset();
 if(format_changed_&&api_->loaded_level()==level_)write(level(0x15f7c8),format_);
 if(api_->loaded_level()==level_){
  if(icon_copy_&&read<unsigned>(icon_state_+0x20)==icon_copy_&&
     read<unsigned short>(icon_defs_+2)==4&&read<unsigned short>(icon_defs_+4)==icon_baseline_.size()){
   for(unsigned i=0;i<icon_baseline_.size();++i){
    const unsigned at=icon_refs_+i*4;
    if(read<unsigned>(at)==icon_baseline_[i])write(at,read<unsigned>(icon_copy_+i*4));
   }
   write(icon_state_+0x20,icon_refs_);write(icon_defs_+2,icon_count_);write(icon_defs_+4,icon_start_);
  }
  if(slot_&&old_callback_&&read<unsigned>(slot_+0x18)==callback_)write(slot_+0x18,old_callback_);
  if(table_&&read<unsigned short>(table_+8)==1)write(table_+8,old_hud_flag_);
  for(const auto& b:bindings_)if(read<unsigned>(b.entry)==b.replacement)write(b.entry,b.original);
 }
 bindings_.clear();slot_=old_callback_=callback_=arena_=bank_=table_=0;level_=-1;format_changed_=false;
 icon_state_=icon_defs_=icon_refs_=icon_copy_=0;icon_count_=icon_start_=0;
 icon_baseline_.clear();
}
}

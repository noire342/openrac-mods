// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hd_icon.hpp"
#include "hud_table_scope.hpp"
#include "native_arsenal_layout.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace revolverator {
namespace {
using namespace native_hud;

// Bounds on original tables before adding the icon pages.
constexpr unsigned max_original_images=8190;
constexpr unsigned max_original_palettes=8191;
constexpr unsigned hud_heap_alignment=16;
constexpr unsigned texture_data_alignment=64;
constexpr unsigned bank_header_alignment=alignof(std::uint32_t);
constexpr unsigned palette_alpha_byte=3;
constexpr unsigned normal_page_index=0;
constexpr unsigned blank_page_index=normal_page_index+1;
enum IconInput : unsigned { weapon_icon, ammo_icon, vendor_ammo_icon, gold_icon, icon_input_count };
}

HdIcon::HdIcon(const openrac_api* api,GuestArena* arena,bool compact_hud_fallback):Memory(api,arena),
 draw_(function<Draw>("FUN_001ffc30")),compact_hud_fallback_(compact_hud_fallback){}
HdIcon::~HdIcon(){reset();}

bool HdIcon::owns_tables()const {
 return image_copy_&&api_->loaded_level()==level_&&
  read<unsigned>(state_+native_hud::frame_references)==refs_&&
  read<unsigned>(state_+native_hud::bank_header)==header_copy_&&
  read<unsigned>(state_+native_hud::image_pages)==image_copy_&&
  read<unsigned>(state_+native_hud::palette_pages)==palette_copy_;
}
bool HdIcon::owns_refs()const {
 if(!owns_tables())return false;
 for(unsigned i=0;i<count_;++i)if(read<unsigned>(refs_+(first_+i)*frame_reference_bytes)!=expected_ref(i))return false;
 return true;
}
unsigned HdIcon::visible_ref(unsigned frame)const {
 if(ammunition_enabled_&&frame==ammunition_frame)return ammunition_ref_;
 if(vendor_ammunition_enabled_&&frame==vendor_ammunition_frame)return vendor_ammunition_ref_;
 if(golden_enabled_&&(frame==gold_preview_frame||(golden_selected_&&frame<generic_icon_frame_count)))return golden_ref_;
 return hd_ref_;
}
unsigned HdIcon::expected_ref(unsigned frame)const {
 return hidden_&&(!ammunition_enabled_||frame!=ammunition_frame)?blank_ref_:visible_ref(frame);
}
bool HdIcon::ready()const {return owns_refs();}

void HdIcon::set_golden(bool enabled){
 if(golden_selected_==enabled)return;
 if(image_copy_&&!ready())reset();
 golden_selected_=enabled;
 if(!image_copy_||hidden_)return;
 for(unsigned i=0;i<std::min(count_,generic_icon_frame_count);++i)
  write(refs_+(first_+i)*frame_reference_bytes,visible_ref(i));
}

bool HdIcon::install(unsigned first,unsigned count,const HdIconData& data,const HdIconData* ammunition,
                     const HdIconData* vendor_ammo,const HdIconData* golden){
 if(image_copy_)return first_==first&&count_==count&&ready();
 const int current=api_->loaded_level();
 if(current<0||current>native_arsenal_layout::last_campaign_level||!count||count>old_refs_.size()||
    !data.pixels||(data.pixel_bytes!=texture_size*texture_size&&data.pixel_bytes!=logical_size*logical_size)||
    !data.palette||data.palette_bytes!=palette_bytes||(!ammunition&&data.palette[palette_alpha_byte]!=0))return false;
 if(ammunition&&(count<=ammunition_frame||!ammunition->pixels||ammunition->pixel_bytes!=logical_size*logical_size||
    !ammunition->palette||ammunition->palette_bytes!=palette_bytes||ammunition->palette[palette_alpha_byte]!=0))return false;
 if(vendor_ammo&&(count<=vendor_ammunition_frame||!vendor_ammo->pixels||vendor_ammo->pixel_bytes!=logical_size*logical_size||
    !vendor_ammo->palette||vendor_ammo->palette_bytes!=palette_bytes))return false;
 if(golden&&(count<=gold_preview_frame||!golden->pixels||golden->pixel_bytes!=data.pixel_bytes||
    !golden->palette||golden->palette_bytes!=palette_bytes))return false;
 const unsigned state=global(native_hud::resident_state),defs=read<unsigned>(state+native_hud::animation_definitions),refs=read<unsigned>(state+native_hud::frame_references);
 const unsigned images=read<unsigned>(state+native_hud::image_pages),palettes=read<unsigned>(state+native_hud::palette_pages);
 const unsigned header=read<unsigned>(state+native_hud::bank_header);
 if(!Memory::valid(defs,animation_entry_bytes)||!Memory::valid(refs,frame_reference_bytes)||
    !Memory::valid(header,sizeof old_header_))return false;
 unsigned frames=0;bool terminated=false;
 for(unsigned i=0;i<max_animation_definitions;++i){
  const unsigned entry=defs+i*animation_entry_bytes;
  if(!Memory::valid(entry,animation_entry_bytes))return false;
  const auto animation=read<AnimationDefinition>(entry);
  if(animation.id==animation_terminator){terminated=true;break;}
  frames=std::max(frames,unsigned(animation.start)+animation.count);
 }
 if(!terminated||!frames||frames>max_frame_references||first>=frames||count>frames-first||
    !Memory::valid(refs,frames*frame_reference_bytes))return false;
 unsigned image_count=0,palette_count=0;
 for(unsigned i=0;i<frames;++i){
  const auto frame=read<FrameReference>(refs+i*frame_reference_bytes);
  if(frame.palette_index<0||frame.image_index<0)return false;
  image_count=std::max(image_count,unsigned(frame.image_index)+1);
  palette_count=std::max(palette_count,unsigned(frame.palette_index)+1);
 }
 image_count=std::max(image_count,read<unsigned>(header+native_hud::bank_image_count));
 palette_count=std::max(palette_count,read<unsigned>(header+native_hud::bank_palette_count));
 if(image_count>max_original_images||palette_count>max_original_palettes||
    !Memory::valid(images,image_count*texture_page_bytes)||!Memory::valid(palettes,palette_count*texture_page_bytes))return false;
 // The Gold image normally shares the normal glyph's texels and only adds a
 // palette. Share every identical source page, keeping the blank page separate.
 const std::array<const HdIconData*,icon_input_count> inputs{{&data,ammunition,vendor_ammo,golden}};
 std::array<const HdIconData*,icon_input_count+1> image_data{{&data}};
 std::array<unsigned,icon_input_count> image_slots{},palette_slots{},input_refs{};
 unsigned added_images=blank_page_index+1,added_palettes=0,pixel_bytes=unsigned(data.pixel_bytes)+logical_size*logical_size;
 for(unsigned i=0;i<inputs.size();++i){
  const auto* input=inputs[i];if(!input)continue;
  palette_slots[i]=added_palettes++;
  if(i==weapon_icon)continue;
  unsigned page=0;
  for(;page<added_images;++page)if(image_data[page]&&image_data[page]->pixels==input->pixels&&
      image_data[page]->pixel_bytes==input->pixel_bytes)break;
  if(page==added_images){image_data[added_images++]=input;pixel_bytes+=unsigned(input->pixel_bytes);}
  image_slots[i]=page;
 }
 // One allocation: complete table clones, aligned texels and every new CLUT.
 // The session arena reserves this storage through the level loader.
 const unsigned image_bytes=(image_count+added_images)*texture_page_bytes;
 const unsigned palette_table_bytes=(palette_count+added_palettes)*texture_page_bytes;
 const unsigned header_offset=(image_bytes+palette_table_bytes+bank_header_alignment-1)&~(bank_header_alignment-1);
 const unsigned tables=(header_offset+unsigned(sizeof old_header_)+texture_data_alignment-1)&~(texture_data_alignment-1);
 const unsigned size=tables+pixel_bytes+added_palettes*palette_bytes;
 unsigned allocation=0,capacity=size;
 if(guest_arena_&&size<=spare_capacity_&&guest_arena_->owns(spare_,spare_capacity_)){
  allocation=spare_;capacity=spare_capacity_;
 }else allocation=allocate_guest(size);
 if(!Memory::valid(allocation,size)&&compact_hud_fallback_&&data.pixel_bytes==logical_size*logical_size){
  const auto cursor=read<unsigned>(state+native_hud::heap_cursor),end=read<unsigned>(state+native_hud::heap_end);
  if(Memory::valid(cursor,size)&&end>=cursor&&Memory::valid(end,0)&&end-cursor>=size+hud_heap_alignment)
   allocation=function<unsigned(*)(unsigned)>("FUN_001ff288")(size);
 }
 if(!Memory::valid(allocation,size))return false;
 const unsigned palette_copy=allocation+image_bytes,pixels=allocation+tables;
 const unsigned cluts=pixels+pixel_bytes;
 old_images_.resize(image_count);old_palettes_.resize(palette_count);
 std::memcpy(old_images_.data(),api_->memory+images,image_count*texture_page_bytes);
 std::memcpy(old_palettes_.data(),api_->memory+palettes,palette_count*texture_page_bytes);
 std::memcpy(api_->memory+allocation,api_->memory+images,image_count*texture_page_bytes);
 std::memcpy(api_->memory+palette_copy,api_->memory+palettes,palette_count*texture_page_bytes);
 const auto header_copy=allocation+header_offset;
 std::memcpy(old_header_.data(),api_->memory+header,sizeof old_header_);
 std::memcpy(api_->memory+header_copy,api_->memory+header,sizeof old_header_);
 write<unsigned>(header_copy+native_hud::bank_image_count,image_count+added_images);write<unsigned>(header_copy+native_hud::bank_palette_count,palette_count+added_palettes);
 const unsigned hd_page=allocation+image_count*texture_page_bytes;
 const unsigned blank_page=hd_page+blank_page_index*texture_page_bytes;
 const unsigned palette_page=palette_copy+palette_count*texture_page_bytes;
 unsigned cursor=pixels;
 for(unsigned i=0;i<added_images;++i){
  const auto* input=image_data[i];const unsigned bytes=input?unsigned(input->pixel_bytes):logical_size*logical_size;
  if(input)std::memcpy(api_->memory+cursor,input->pixels,bytes);else std::memset(api_->memory+cursor,0,bytes);
  const unsigned page=hd_page+i*texture_page_bytes;
  const auto dimension=std::uint8_t(bytes==texture_size*texture_size?texture_log2:native_texture_log2);
  write(page,TexturePage{cursor,0,dimension,dimension});
  cursor+=bytes;
 }
 for(unsigned i=0;i<inputs.size();++i)if(const auto* input=inputs[i]){
  const unsigned clut=cluts+palette_slots[i]*palette_bytes,palette=palette_page+palette_slots[i]*texture_page_bytes;
  std::memcpy(api_->memory+clut,input->palette,palette_bytes);
  write(palette,TexturePage{clut,0,0,0});
  input_refs[i]=((image_count+image_slots[i])<<std::numeric_limits<std::uint16_t>::digits)|(palette_count+palette_slots[i]);
 }
 state_=state;refs_=refs;first_=first;count_=count;level_=current;
 allocation_capacity_=capacity;spare_=spare_capacity_=0;
 images_=images;palettes_=palettes;image_copy_=allocation;palette_copy_=palette_copy;
 header_=header;header_copy_=header_copy;
 image_count_=image_count;palette_count_=palette_count;hd_page_=hd_page;blank_page_=blank_page;palette_page_=palette_page;
 appended_images_=added_images;appended_palettes_=added_palettes;
 hd_ref_=input_refs[weapon_icon];
 const unsigned blank_palette=palette_count+palette_slots[ammunition?ammo_icon:weapon_icon];
 blank_ref_=((image_count+blank_page_index)<<std::numeric_limits<std::uint16_t>::digits)|blank_palette;
 ammunition_ref_=input_refs[ammo_icon];ammunition_enabled_=ammunition!=nullptr;
 vendor_ammunition_ref_=input_refs[vendor_ammo_icon];vendor_ammunition_enabled_=vendor_ammo!=nullptr;
 golden_ref_=input_refs[gold_icon];golden_enabled_=golden!=nullptr;
 for(unsigned i=0;i<count;++i){
  const unsigned entry=refs+(first+i)*frame_reference_bytes;
  old_refs_[i]=read<unsigned>(entry);write(entry,visible_ref(i));
 }
 write(state+native_hud::bank_header,header_copy_);write(state+native_hud::image_pages,image_copy_);write(state+native_hud::palette_pages,palette_copy_);
 return true;
}

void HdIcon::frame(){
 if(!ready()){if(image_copy_)reset();return;}
 // Counts in the cloned header also cover every appended page. Each native
 // paging pass can therefore invalidate them, including multiple UI panels in
 // the same frame. Clear once here as well before the next native tick.
 constexpr unsigned gs_offset=offsetof(TexturePage,gs_block_offset);
 for(unsigned i=0;i<appended_images_;++i)write<std::uint16_t>(hd_page_+i*texture_page_bytes+gs_offset,0);
 for(unsigned i=0;i<appended_palettes_;++i)write<std::uint16_t>(palette_page_+i*texture_page_bytes+gs_offset,0);
}
bool HdIcon::before_base(){
 if(hidden_)return ready();
 if(!ready()){if(image_copy_)reset();return false;}
 for(unsigned i=0;i<count_;++i)if(!ammunition_enabled_||i!=ammunition_frame)
  write(refs_+(first_+i)*frame_reference_bytes,blank_ref_);
 hidden_=true;return true;
}
void HdIcon::after_base(){
 if(!hidden_)return;
 if(!owns_tables()){reset();return;}
 if(owns_tables())for(unsigned i=0;i<count_;++i){
  const unsigned at=refs_+(first_+i)*frame_reference_bytes;
  if(read<unsigned>(at)==blank_ref_)write(at,visible_ref(i));
 }
 hidden_=false;
}
bool HdIcon::draw(int x,int y,int alpha){
 after_base();if(!ready()||alpha<=0)return false;
 draw_(int(first_),x,y,logical_size,logical_size,std::min(alpha,max_alpha));return true;
}
void HdIcon::merge_pages(unsigned original,unsigned copy,const std::vector<std::uint64_t>& baseline){
 for(unsigned i=0;i<baseline.size();++i){
  const unsigned at=original+i*texture_page_bytes;
  // Preserve another owner's edit made directly in the original table. Edits
  // made through our active table survive teardown. The native pager uses the
  // same GS pages through either view, including resident uploads.
  if(read<std::uint64_t>(at)!=baseline[i])continue;
  write(at,read<std::uint64_t>(copy+i*texture_page_bytes));
 }
}
void HdIcon::reset(bool reuse_storage){
 if(!reuse_storage)spare_=spare_capacity_=0;
 if(api_->loaded_level()==level_&&image_copy_){
  const bool own_refs=read<unsigned>(state_+native_hud::frame_references)==refs_;
  if(own_refs)for(unsigned i=0;i<count_;++i){
   const unsigned at=refs_+(first_+i)*frame_reference_bytes,value=read<unsigned>(at);
   if(value==hd_ref_||value==blank_ref_||(ammunition_enabled_&&value==ammunition_ref_)||
      (vendor_ammunition_enabled_&&value==vendor_ammunition_ref_)||(golden_enabled_&&value==golden_ref_))write(at,old_refs_[i]);
  }
  // A later owner may have copied references containing our appended indices.
  // In that case leave the level-owned pages alive instead of dangling its refs.
  if(own_refs&&owns_tables()){
   merge_pages(images_,image_copy_,old_images_);merge_pages(palettes_,palette_copy_,old_palettes_);
   for(unsigned i=0;i<old_header_.size();++i){
    const unsigned offset=i*sizeof(old_header_[i]);
    if(offset==native_hud::bank_palette_count||offset==native_hud::bank_image_count)continue;
    if(read<unsigned>(header_+offset)==old_header_[i])write(header_+offset,read<unsigned>(header_copy_+offset));
   }
   write(state_+native_hud::bank_header,header_);write(state_+native_hud::image_pages,images_);write(state_+native_hud::palette_pages,palettes_);
   if(api_->relocate_level_data){
    restore_cached_hud_pointer(*this,native_hud::bank_header,header_copy_,header_);
    restore_cached_hud_pointer(*this,native_hud::image_pages,image_copy_,images_);
    restore_cached_hud_pointer(*this,native_hud::palette_pages,palette_copy_,palettes_);
   }
   if(reuse_storage&&guest_arena_&&guest_arena_->owns(image_copy_,allocation_capacity_)){
    spare_=image_copy_;spare_capacity_=allocation_capacity_;
   }
  }
 }
 level_=-1;state_=refs_=first_=count_=images_=palettes_=image_copy_=palette_copy_=0;
 allocation_capacity_=0;
 image_count_=palette_count_=hd_page_=blank_page_=palette_page_=hd_ref_=blank_ref_=0;
 appended_images_=appended_palettes_=0;
 header_=header_copy_=0;old_header_.fill(0);
 ammunition_ref_=0;ammunition_enabled_=false;
 vendor_ammunition_ref_=golden_ref_=0;vendor_ammunition_enabled_=golden_enabled_=false;
 hidden_=false;old_refs_.fill(0);old_images_.clear();old_palettes_.clear();
}
}

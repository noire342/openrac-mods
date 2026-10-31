// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hd_icon.hpp"
#include <algorithm>
#include <cstring>

namespace revolverator {
HdIcon::HdIcon(const openrac_api* api,GuestArena* arena,bool compact_hud_fallback):Memory(api,arena),
 draw_(function<Draw>("FUN_001ffc30")),compact_hud_fallback_(compact_hud_fallback){}
HdIcon::~HdIcon(){reset();}

bool HdIcon::owns_tables()const {
 return image_copy_&&api_->loaded_level()==level_&&
  read<unsigned>(state_+0x20)==refs_&&
  read<unsigned>(state_+0x18)==header_copy_&&
  read<unsigned>(state_+0x24)==image_copy_&&
  read<unsigned>(state_+0x28)==palette_copy_;
}
bool HdIcon::owns_refs()const {
 if(!owns_tables())return false;
 for(unsigned i=0;i<count_;++i)if(read<unsigned>(refs_+(first_+i)*4)!=expected_ref(i))return false;
 return true;
}
unsigned HdIcon::visible_ref(unsigned frame)const {
 if(ammunition_enabled_&&frame==3)return ammunition_ref_;
 if(vendor_ammunition_enabled_&&frame==2)return vendor_ammunition_ref_;
 if(golden_enabled_&&(frame==4||(golden_selected_&&frame<2)))return golden_ref_;
 return hd_ref_;
}
unsigned HdIcon::expected_ref(unsigned frame)const {
 return hidden_&&(!ammunition_enabled_||frame!=3)?blank_ref_:visible_ref(frame);
}
bool HdIcon::ready()const {return owns_refs();}

void HdIcon::set_golden(bool enabled){
 if(golden_selected_==enabled)return;
 if(image_copy_&&!ready())reset();
 golden_selected_=enabled;
 if(!image_copy_||hidden_)return;
 for(unsigned i=0;i<std::min(count_,2u);++i)write(refs_+(first_+i)*4,visible_ref(i));
}

bool HdIcon::install(unsigned first,unsigned count,const HdIconData& data,const HdIconData* ammunition,
                     const HdIconData* vendor_ammo,const HdIconData* golden){
 if(image_copy_)return first_==first&&count_==count&&ready();
 const int current=api_->loaded_level();
 if(current<0||current>=19||!count||count>old_refs_.size()||
    !data.pixels||(data.pixel_bytes!=texture_size*texture_size&&data.pixel_bytes!=logical_size*logical_size)||
    !data.palette||data.palette_bytes!=1024||(!ammunition&&data.palette[3]!=0))return false;
 if(ammunition&&(count<4||!ammunition->pixels||ammunition->pixel_bytes!=logical_size*logical_size||
    !ammunition->palette||ammunition->palette_bytes!=1024||ammunition->palette[3]!=0))return false;
 if(vendor_ammo&&(count<3||!vendor_ammo->pixels||vendor_ammo->pixel_bytes!=logical_size*logical_size||
    !vendor_ammo->palette||vendor_ammo->palette_bytes!=1024))return false;
 if(golden&&(count<5||!golden->pixels||golden->pixel_bytes!=data.pixel_bytes||
    !golden->palette||golden->palette_bytes!=1024))return false;
 const unsigned state=global(0x19a3e8),defs=read<unsigned>(state+0x1c),refs=read<unsigned>(state+0x20);
 const unsigned images=read<unsigned>(state+0x24),palettes=read<unsigned>(state+0x28);
 const unsigned header=read<unsigned>(state+0x18);
 if(!Memory::valid(defs,8)||!Memory::valid(refs,4)||!Memory::valid(header,sizeof old_header_))return false;
 unsigned frames=0;bool terminated=false;
 for(unsigned i=0;i<1024;++i){
  const unsigned entry=defs+i*8;if(!Memory::valid(entry,8))return false;
  if(read<std::uint16_t>(entry)==0xffff){terminated=true;break;}
  frames=std::max(frames,unsigned(read<std::uint16_t>(entry+4))+read<std::uint16_t>(entry+2));
 }
 if(!terminated||!frames||frames>16000||first>=frames||count>frames-first||!Memory::valid(refs,frames*4))return false;
 unsigned image_count=0,palette_count=0;
 for(unsigned i=0;i<frames;++i){
  const int palette=read<std::int16_t>(refs+i*4),image=read<std::int16_t>(refs+i*4+2);
  if(palette<0||image<0)return false;
  image_count=std::max(image_count,unsigned(image)+1);palette_count=std::max(palette_count,unsigned(palette)+1);
 }
 image_count=std::max(image_count,read<unsigned>(header+0x44));
 palette_count=std::max(palette_count,read<unsigned>(header+0x24));
 if(image_count>8190||palette_count>8191||!Memory::valid(images,image_count*8)||!Memory::valid(palettes,palette_count*8))return false;
 // The Gold image normally shares the normal glyph's texels and only adds a
 // palette. Share every identical source page, keeping the blank page separate.
 const std::array<const HdIconData*,4> inputs{{&data,ammunition,vendor_ammo,golden}};
 std::array<const HdIconData*,5> image_data{{&data,nullptr,nullptr,nullptr,nullptr}};
 std::array<unsigned,4> image_slots{},palette_slots{},input_refs{};
 unsigned added_images=2,added_palettes=0,pixel_bytes=unsigned(data.pixel_bytes)+logical_size*logical_size;
 for(unsigned i=0;i<inputs.size();++i){
  const auto* input=inputs[i];if(!input)continue;
  palette_slots[i]=added_palettes++;
  if(!i)continue;
  unsigned page=0;
  for(;page<added_images;++page)if(image_data[page]&&image_data[page]->pixels==input->pixels&&
      image_data[page]->pixel_bytes==input->pixel_bytes)break;
  if(page==added_images){image_data[added_images++]=input;pixel_bytes+=unsigned(input->pixel_bytes);}
  image_slots[i]=page;
 }
 // One allocation: complete table clones, aligned texels and every new CLUT.
 // The session arena reserves this storage through the level loader.
 const unsigned image_bytes=(image_count+added_images)*8,palette_bytes=(palette_count+added_palettes)*8;
 const unsigned header_offset=(image_bytes+palette_bytes+3)&~3u;
 const unsigned tables=(header_offset+unsigned(sizeof old_header_)+63)&~63u;
 const unsigned size=tables+pixel_bytes+added_palettes*1024;
 unsigned allocation=allocate_guest(size);
 if(!Memory::valid(allocation,size)&&compact_hud_fallback_&&data.pixel_bytes==logical_size*logical_size){
  const auto cursor=read<unsigned>(state+0x10),end=read<unsigned>(state+0x14);
  if(Memory::valid(cursor,size)&&end>=cursor&&end<=0x2000000&&end-cursor>=size+16)
   allocation=function<unsigned(*)(unsigned)>("FUN_001ff288")(size);
 }
 if(!Memory::valid(allocation,size))return false;
 const unsigned palette_copy=allocation+image_bytes,pixels=allocation+tables;
 const unsigned cluts=pixels+pixel_bytes;
 old_images_.resize(image_count);old_palettes_.resize(palette_count);
 std::memcpy(old_images_.data(),api_->memory+images,image_count*8);
 std::memcpy(old_palettes_.data(),api_->memory+palettes,palette_count*8);
 std::memcpy(api_->memory+allocation,api_->memory+images,image_count*8);
 std::memcpy(api_->memory+palette_copy,api_->memory+palettes,palette_count*8);
 const auto header_copy=allocation+header_offset;
 std::memcpy(old_header_.data(),api_->memory+header,sizeof old_header_);
 std::memcpy(api_->memory+header_copy,api_->memory+header,sizeof old_header_);
 write<unsigned>(header_copy+0x44,image_count+added_images);write<unsigned>(header_copy+0x24,palette_count+added_palettes);
 const unsigned hd_page=allocation+image_count*8,blank_page=hd_page+8;
 const unsigned palette_page=palette_copy+palette_count*8;
 unsigned cursor=pixels;
 for(unsigned i=0;i<added_images;++i){
  const auto* input=image_data[i];const unsigned bytes=input?unsigned(input->pixel_bytes):logical_size*logical_size;
  if(input)std::memcpy(api_->memory+cursor,input->pixels,bytes);else std::memset(api_->memory+cursor,0,bytes);
  const unsigned page=hd_page+i*8;const auto dimension=std::uint8_t(bytes==texture_size*texture_size?7:5);
  write(page,cursor);write<std::uint16_t>(page+4,0);write<std::uint8_t>(page+6,dimension);write<std::uint8_t>(page+7,dimension);
  cursor+=bytes;
 }
 for(unsigned i=0;i<inputs.size();++i)if(const auto* input=inputs[i]){
  const unsigned clut=cluts+palette_slots[i]*1024,palette=palette_page+palette_slots[i]*8;
  std::memcpy(api_->memory+clut,input->palette,1024);write(palette,clut);write<std::uint32_t>(palette+4,0);
  input_refs[i]=((image_count+image_slots[i])<<16)|(palette_count+palette_slots[i]);
 }
 state_=state;refs_=refs;first_=first;count_=count;level_=current;
 images_=images;palettes_=palettes;image_copy_=allocation;palette_copy_=palette_copy;
 header_=header;header_copy_=header_copy;
 image_count_=image_count;palette_count_=palette_count;hd_page_=hd_page;blank_page_=blank_page;palette_page_=palette_page;
 appended_images_=added_images;appended_palettes_=added_palettes;
 hd_ref_=input_refs[0];blank_ref_=((image_count+1)<<16)|(palette_count+(ammunition?palette_slots[1]:0));
 ammunition_ref_=input_refs[1];ammunition_enabled_=ammunition!=nullptr;
 vendor_ammunition_ref_=input_refs[2];vendor_ammunition_enabled_=vendor_ammo!=nullptr;
 golden_ref_=input_refs[3];golden_enabled_=golden!=nullptr;
 for(unsigned i=0;i<count;++i){old_refs_[i]=read<unsigned>(refs+(first+i)*4);write(refs+(first+i)*4,visible_ref(i));}
 write(state+0x18,header_copy_);write(state+0x24,image_copy_);write(state+0x28,palette_copy_);
 return true;
}

void HdIcon::frame(){
 if(!ready()){if(image_copy_)reset();return;}
 // Counts in the cloned header also cover every appended page. Each native
 // paging pass can therefore invalidate them, including multiple UI panels in
 // the same frame. Clear once here as well before the next native tick.
 for(unsigned i=0;i<appended_images_;++i)write<std::uint16_t>(hd_page_+i*8+4,0);
 for(unsigned i=0;i<appended_palettes_;++i)write<std::uint16_t>(palette_page_+i*8+4,0);
}
bool HdIcon::before_base(){
 if(hidden_)return ready();
 if(!ready()){if(image_copy_)reset();return false;}
 for(unsigned i=0;i<count_;++i)if(!ammunition_enabled_||i!=3)write(refs_+(first_+i)*4,blank_ref_);
 hidden_=true;return true;
}
void HdIcon::after_base(){
 if(!hidden_)return;
 if(!owns_tables()){reset();return;}
 if(owns_tables())for(unsigned i=0;i<count_;++i){
  const unsigned at=refs_+(first_+i)*4;
  if(read<unsigned>(at)==blank_ref_)write(at,visible_ref(i));
 }
 hidden_=false;
}
bool HdIcon::draw(int x,int y,int alpha){
 after_base();if(!ready()||alpha<=0)return false;
 draw_(int(first_),x,y,logical_size,logical_size,std::min(alpha,128));return true;
}
void HdIcon::merge_pages(unsigned original,unsigned copy,const std::vector<std::uint64_t>& baseline){
 for(unsigned i=0;i<baseline.size();++i){
  const unsigned at=original+i*8;
  // Preserve another owner's edit made directly in the original table. Edits
  // made through our active table survive teardown; GS cache state is cleared.
  if(read<std::uint64_t>(at)!=baseline[i])continue;
  write(at,read<std::uint64_t>(copy+i*8));write<std::uint16_t>(at+4,0);
 }
}
void HdIcon::reset(){
 if(api_->loaded_level()==level_&&image_copy_){
  const bool own_refs=read<unsigned>(state_+0x20)==refs_;
  if(own_refs)for(unsigned i=0;i<count_;++i){
   const unsigned at=refs_+(first_+i)*4,value=read<unsigned>(at);
   if(value==hd_ref_||value==blank_ref_||(ammunition_enabled_&&value==ammunition_ref_)||
      (vendor_ammunition_enabled_&&value==vendor_ammunition_ref_)||(golden_enabled_&&value==golden_ref_))write(at,old_refs_[i]);
  }
  // A later owner may have copied references containing our appended indices.
  // In that case leave the level-owned pages alive instead of dangling its refs.
  if(own_refs&&owns_tables()){
   merge_pages(images_,image_copy_,old_images_);merge_pages(palettes_,palette_copy_,old_palettes_);
   for(unsigned i=0;i<old_header_.size();++i){
    if(i==0x24/4||i==0x44/4)continue;
    if(read<unsigned>(header_+i*4)==old_header_[i])write(header_+i*4,read<unsigned>(header_copy_+i*4));
   }
   write(state_+0x18,header_);write(state_+0x24,images_);write(state_+0x28,palettes_);
  }
 }
 level_=-1;state_=refs_=first_=count_=images_=palettes_=image_copy_=palette_copy_=0;
 image_count_=palette_count_=hd_page_=blank_page_=palette_page_=hd_ref_=blank_ref_=0;
 appended_images_=appended_palettes_=0;
 header_=header_copy_=0;old_header_.fill(0);
 ammunition_ref_=0;ammunition_enabled_=false;
 vendor_ammunition_ref_=golden_ref_=0;vendor_ammunition_enabled_=golden_enabled_=false;
 hidden_=false;old_refs_.fill(0);old_images_.clear();old_palettes_.clear();
}
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "hd_icon.hpp"
#include <algorithm>
#include <cstring>

namespace revolverator {
HdIcon::HdIcon(const openrac_api* api,GuestArena* arena):Memory(api,arena),
 draw_(function<Draw>("FUN_001ffc30")){}
HdIcon::~HdIcon(){reset();}

bool HdIcon::owns_tables()const {
 return image_copy_&&api_->loaded_level()==level_&&
  read<unsigned>(state_+0x20)==refs_&&
  read<unsigned>(state_+0x24)==image_copy_&&
  read<unsigned>(state_+0x28)==palette_copy_;
}
bool HdIcon::owns_refs()const {
 if(!owns_tables())return false;
 const unsigned expected=hidden_?blank_ref_:hd_ref_;
 for(unsigned i=0;i<count_;++i)if(read<unsigned>(refs_+(first_+i)*4)!=expected)return false;
 return true;
}
bool HdIcon::ready()const {return owns_refs();}

bool HdIcon::install(unsigned first,unsigned count,const HdIconData& data){
 if(image_copy_)return first_==first&&count_==count&&ready();
 const int current=api_->loaded_level();
 if(current<0||current>=19||!count||count>old_refs_.size()||
    !data.pixels||data.pixel_bytes!=texture_size*texture_size||
    !data.palette||data.palette_bytes!=1024||data.palette[3]!=0)return false;
 const unsigned state=global(0x19a3e8),defs=read<unsigned>(state+0x1c),refs=read<unsigned>(state+0x20);
 const unsigned images=read<unsigned>(state+0x24),palettes=read<unsigned>(state+0x28);
 if(!Memory::valid(defs,8)||!Memory::valid(refs,4))return false;
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
 if(image_count>8190||palette_count>8191||!Memory::valid(images,image_count*8)||!Memory::valid(palettes,palette_count*8))return false;
 // One allocation: table clones, aligned PSMT8 texels, transparent 32px tile,
 // and CLUT. The session arena reserves this storage through the level loader.
 const unsigned image_bytes=(image_count+2)*8,palette_bytes=(palette_count+1)*8;
 const unsigned tables=(image_bytes+palette_bytes+63)&~63u;
 const unsigned size=tables+texture_size*texture_size+logical_size*logical_size+1024;
 const unsigned allocation=allocate_guest(size);if(!Memory::valid(allocation,size))return false;
 const unsigned palette_copy=allocation+image_bytes,pixels=allocation+tables;
 const unsigned blank=pixels+texture_size*texture_size,clut=blank+logical_size*logical_size;
 old_images_.resize(image_count);old_palettes_.resize(palette_count);
 std::memcpy(old_images_.data(),api_->memory+images,image_count*8);
 std::memcpy(old_palettes_.data(),api_->memory+palettes,palette_count*8);
 std::memcpy(api_->memory+allocation,api_->memory+images,image_count*8);
 std::memcpy(api_->memory+palette_copy,api_->memory+palettes,palette_count*8);
 std::memcpy(api_->memory+pixels,data.pixels,data.pixel_bytes);
 std::memset(api_->memory+blank,0,logical_size*logical_size);
 std::memcpy(api_->memory+clut,data.palette,data.palette_bytes);
 const unsigned hd_page=allocation+image_count*8,blank_page=hd_page+8;
 const unsigned palette_page=palette_copy+palette_count*8;
 write(hd_page,pixels);write<std::uint16_t>(hd_page+4,0);write<std::uint8_t>(hd_page+6,7);write<std::uint8_t>(hd_page+7,7);
 write(blank_page,blank);write<std::uint16_t>(blank_page+4,0);write<std::uint8_t>(blank_page+6,5);write<std::uint8_t>(blank_page+7,5);
 write(palette_page,clut);write<std::uint16_t>(palette_page+4,0);write<std::uint16_t>(palette_page+6,0);
 state_=state;refs_=refs;first_=first;count_=count;level_=current;
 images_=images;palettes_=palettes;image_copy_=allocation;palette_copy_=palette_copy;
 image_count_=image_count;palette_count_=palette_count;hd_page_=hd_page;blank_page_=blank_page;palette_page_=palette_page;
 hd_ref_=(image_count<<16)|palette_count;blank_ref_=((image_count+1)<<16)|palette_count;
 for(unsigned i=0;i<count;++i){old_refs_[i]=read<unsigned>(refs+(first+i)*4);write(refs+(first+i)*4,hd_ref_);}
 write(state+0x24,image_copy_);write(state+0x28,palette_copy_);
 return true;
}

void HdIcon::frame(){
 if(!ready()){if(image_copy_)reset();return;}
 // Native bank counts cover only the original pages. Invalidate our appended
 // pages once per tick so transient GS allocation cannot reuse a stale texture.
 write<std::uint16_t>(hd_page_+4,0);write<std::uint16_t>(blank_page_+4,0);write<std::uint16_t>(palette_page_+4,0);
}
bool HdIcon::before_base(){
 if(hidden_)return ready();
 if(!ready()){if(image_copy_)reset();return false;}
 for(unsigned i=0;i<count_;++i)write(refs_+(first_+i)*4,blank_ref_);
 hidden_=true;return true;
}
void HdIcon::after_base(){
 if(!hidden_)return;
 if(!owns_tables()){reset();return;}
 if(owns_tables())for(unsigned i=0;i<count_;++i){
  const unsigned at=refs_+(first_+i)*4;
  if(read<unsigned>(at)==blank_ref_)write(at,hd_ref_);
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
   if(value==hd_ref_||value==blank_ref_)write(at,old_refs_[i]);
  }
  // A later owner may have copied references containing our appended indices.
  // In that case leave the level-owned pages alive instead of dangling its refs.
  if(own_refs&&read<unsigned>(state_+0x24)==image_copy_){merge_pages(images_,image_copy_,old_images_);write(state_+0x24,images_);}
  if(own_refs&&read<unsigned>(state_+0x28)==palette_copy_){merge_pages(palettes_,palette_copy_,old_palettes_);write(state_+0x28,palettes_);}
 }
 level_=-1;state_=refs_=first_=count_=images_=palettes_=image_copy_=palette_copy_=0;
 image_count_=palette_count_=hd_page_=blank_page_=palette_page_=hd_ref_=blank_ref_=0;
 hidden_=false;old_refs_.fill(0);old_images_.clear();old_palettes_.clear();
}
}

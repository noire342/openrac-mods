// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_class_alias.hpp"
#include <cstring>

namespace revolverator {
namespace {
// Executable NTSC-U symbols. api.relocate_data resolves the loaded program's
// copies; offsets into each table are added AFTER resolving its base.
constexpr unsigned class_count=0x15ff00;
constexpr unsigned class_resources=0x1b3200;
constexpr unsigned class_updates=0x1b3580;
constexpr unsigned class_ids=0x1b3900;
constexpr unsigned class_map=0x1b3ac0;
static_assert(NativeAmmoAlias::native_class<NativeAmmoAlias::id_capacity);
}
bool NativeAmmoAlias::supported() const {
    return api_&&api_->memory&&api_->loaded_level&&api_->relocate_data&&
        api_->game&&api_->version&&std::strcmp(api_->game,"rac1")==0&&
        std::strcmp(api_->version,"ntsc")==0&&api_->loaded_level()>=0&&api_->loaded_level()<19;
}
bool NativeAmmoAlias::source_valid(unsigned map,std::uint8_t slot) const {
    const auto count_at=global(class_count),resources=global(class_resources),ids=global(class_ids),updates=global(class_updates);
    if(!valid(map,id_capacity)||!valid(count_at,4)||!valid(resources,slot_capacity*4)||
        !valid(updates,slot_capacity*4)||!valid(ids,slot_capacity*2))return false;
    const int count=read<int>(count_at);
    if(count<=0||count>int(slot_capacity)||slot>=slot_capacity||slot>=count||
        read<std::uint8_t>(map+source_class)!=slot||
        read<std::int16_t>(ids+unsigned(slot)*2)!=int(source_class))return false;
    return valid(read<unsigned>(resources+unsigned(slot)*4),0x4c);
}
bool NativeAmmoAlias::ready() const {
    return owned_&&supported()&&api_->loaded_level()==level_&&global(class_map)==map_&&
        read<std::uint8_t>(map_+native_class)==slot_&&source_valid(map_,slot_);
}
bool NativeAmmoAlias::visual_valid(const Visual& visual) const {
    return visual.address&&supported()&&api_->loaded_level()==level_&&valid(visual.address,256)&&
        read<unsigned>(visual.address+0x24)==visual.pclass&&
        read<unsigned>(visual.address+0x78)==visual.pvars&&
        read<unsigned>(visual.address+0x38)==visual.stamp&&
        read<std::uint8_t>(visual.address+0x20)<0x80;
}
void NativeAmmoAlias::restore_visual(Visual& visual){
    if(visual_valid(visual)&&read<std::uint16_t>(visual.address+0xa6)==native_class)
        write<std::uint16_t>(visual.address+0xa6,source_class);
    visual={};
}
void NativeAmmoAlias::restore_previews(){
    for(auto& visual:previews_)restore_visual(visual);
    previews_.clear();
}
void NativeAmmoAlias::canonicalize_preview(unsigned subject){
    for(std::size_t i=0;i<previews_.size();++i)if(previews_[i].address==subject){
        restore_visual(previews_[i]);previews_.erase(previews_.begin()+i);return;
    }
}
void NativeAmmoAlias::frame() {
    restore_previews();ensure_mapping();
}
bool NativeAmmoAlias::ensure_mapping(){
    if(!supported()){reset();return false;}
    const int current=api_->loaded_level();
    if(current!=level_){reset();level_=current;}
    if(owned_){
        if(ready())return true;
        // Restore only our unchanged byte; another mod's later mapping wins.
        reset();level_=current;
    }
    const auto map=global(class_map);
    if(!valid(map,id_capacity)||read<std::uint8_t>(map+native_class)!=0xff)return false;
    const auto source_slot=read<std::uint8_t>(map+source_class);
    if(!source_valid(map,source_slot))return false;
    // register_moby_class normally appends all tables and mutates its class
    // header. This is an alias, not a new resource: deliberately share its
    // existing slot and leave all those native bindings byte-for-byte intact.
    map_=map;slot_=source_slot;owned_=true;
    write<std::uint8_t>(map_+native_class,slot_);return true;
}
bool NativeAmmoAlias::apply_preview(unsigned subject,bool custom){
    if(!custom){canonicalize_preview(subject);return false;}
    if(!ensure_mapping())return false;
    const auto pclass=read<unsigned>(global(class_resources)+unsigned(slot_)*4);
    for(std::size_t i=0;i<previews_.size();){
        auto& visual=previews_[i];
        if(!visual_valid(visual)){
            // A native pool slot may have been recycled since the last draw.
            // Its new occupant owns every byte, including its class ID.
            previews_.erase(previews_.begin()+i);continue;
        }
        if(visual.address==subject){
            if(visual.pclass==pclass&&read<std::uint16_t>(subject+0xa6)==native_class)return true;
            restore_visual(visual);previews_.erase(previews_.begin()+i);break;
        }
        ++i;
    }
    if(previews_.size()>=MaximumPreviews||!valid(subject,256)||
        read<std::uint8_t>(subject+0x20)>=0x80||
        read<std::uint16_t>(subject+0xa6)!=source_class||
        read<unsigned>(subject+0x24)!=pclass)return false;
    previews_.push_back({subject,pclass,read<unsigned>(subject+0x78),read<unsigned>(subject+0x38)});
    write<std::uint16_t>(subject+0xa6,native_class);return true;
}
void NativeAmmoAlias::reset() {
    restore_previews();
    if(owned_&&supported()&&api_->loaded_level()==level_&&global(class_map)==map_&&
        read<std::uint8_t>(map_+native_class)==slot_)
        write<std::uint8_t>(map_+native_class,0xff);
    owned_=false;map_=0;slot_=0xff;level_=-1;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_alias.hpp"
#include <cstring>
#include <cmath>

namespace revolverator {
namespace {
constexpr unsigned class_count=0x15ff00,class_resources=0x1b3200;
constexpr unsigned class_updates=0x1b3580,class_ids=0x1b3900,class_map=0x1b3ac0;
}
bool NativeWeaponAlias::supported()const {
    return api_&&api_->memory&&api_->loaded_level&&api_->relocate_data&&api_->game&&api_->version&&
        std::strcmp(api_->game,"rac1")==0&&RegionBindings::for_version(api_->version)==&bindings()&&
        api_->loaded_level()>=0&&api_->loaded_level()<19;
}
bool NativeWeaponAlias::source_valid(unsigned map,std::uint8_t slot)const {
    const unsigned resources=global(class_resources),ids=global(class_ids),updates=global(class_updates);
    if(!valid(map,id_capacity)||!valid(global(class_count),4)||!valid(resources,slot_capacity*4)||
       !valid(ids,slot_capacity*2)||!valid(updates,slot_capacity*4))return false;
    const int count=read<int>(global(class_count));
    const unsigned part=slot<slot_capacity?read<unsigned>(resources+unsigned(slot)*4):0;
    return count>0&&count<=int(slot_capacity)&&slot<slot_capacity&&slot<count&&
        read<std::uint8_t>(map+source_class)==slot&&
        read<std::int16_t>(ids+unsigned(slot)*2)==int(source_class)&&
        valid(part,0x4c)&&read<std::uint8_t>(part+8)==5;
}
bool NativeWeaponAlias::ready()const {
    return mapping_owned_&&supported()&&api_->loaded_level()==level_&&global(class_map)==map_&&
        read<std::uint8_t>(map_+native_class)==slot_&&read<std::uint8_t>(map_+golden_class)==slot_&&source_valid(map_,slot_);
}
bool NativeWeaponAlias::visual_valid(const Visual& visual)const {
    return visual.address&&supported()&&api_->loaded_level()==level_&&valid(visual.address,256)&&
        read<unsigned>(visual.address+0x24)==visual.pclass&&
        read<unsigned>(visual.address+0x78)==visual.pvars&&
        read<unsigned>(visual.address+0x38)==visual.stamp&&
        read<std::uint8_t>(visual.address+0x20)<0x80;
}
bool NativeWeaponAlias::owns(unsigned moby)const {
    const auto render=read<std::uint16_t>(moby+0xa6);
    if(moby==visual_.address&&render==visual_.render_class&&visual_valid(visual_))return true;
    for(const auto& preview:previews_)if(preview.address==moby&&render==preview.render_class&&visual_valid(preview))return true;
    return false;
}
void NativeWeaponAlias::restore_visual(Visual& visual) {
    if(visual_valid(visual)&&read<std::uint16_t>(visual.address+0xa6)==visual.render_class){
        if(visual.scaled&&read<float>(visual.address+0x2c)==visual.applied_scale)write(visual.address+0x2c,visual.original_scale);
        write<std::uint16_t>(visual.address+0xa6,source_class);
    }
    visual={};
}
void NativeWeaponAlias::restore_previews(){
    for(auto& preview:previews_)restore_visual(preview);
    previews_.clear();
}
void NativeWeaponAlias::frame() {
    restore_visual(visual_);restore_previews();
    ensure_mapping();
}
bool NativeWeaponAlias::ensure_mapping(){
    if(!supported()){reset();return false;}
    const int current=api_->loaded_level();
    if(current!=level_){reset();level_=current;}
    if(mapping_owned_){if(ready())return true;reset();level_=current;}
    const unsigned map=global(class_map);
    if(!valid(map,id_capacity)||read<std::uint8_t>(map+native_class)!=0xff||read<std::uint8_t>(map+golden_class)!=0xff)return false;
    const auto slot=read<std::uint8_t>(map+source_class);
    if(!source_valid(map,slot))return false;
    map_=map;slot_=slot;mapping_owned_=true;
    write<std::uint8_t>(map_+native_class,slot_);write<std::uint8_t>(map_+golden_class,slot_);return true;
}
bool NativeWeaponAlias::apply_visual(Visual& lease,unsigned subject,unsigned render_class){
    if(!ready()||!valid(subject,256)||read<std::uint8_t>(subject+0x20)>=0x80)return false;
    if(render_class!=native_class&&render_class!=golden_class)return false;
    if(lease.address==subject&&visual_valid(lease)&&read<std::uint16_t>(subject+0xa6)==lease.render_class){
        write<std::uint16_t>(subject+0xa6,static_cast<std::uint16_t>(render_class));lease.render_class=render_class;return true;
    }
    if(read<std::uint16_t>(subject+0xa6)!=source_class)return false;
    const unsigned pclass=read<unsigned>(subject+0x24);
    if(pclass!=read<unsigned>(global(class_resources)+unsigned(slot_)*4))return false;
    lease={subject,pclass,read<unsigned>(subject+0x78),read<unsigned>(subject+0x38)};
    lease.render_class=render_class;
    write<std::uint16_t>(subject+0xa6,static_cast<std::uint16_t>(render_class));return true;
}
bool NativeWeaponAlias::apply(unsigned held,bool revolverator,bool golden) {
    if(!revolverator||held!=visual_.address)restore_visual(visual_);
    if(!revolverator||!ensure_mapping())return false;
    for(const auto& preview:previews_)if(preview.address==held)return false;
    return apply_visual(visual_,held,golden?golden_class:native_class);
}
bool NativeWeaponAlias::apply_preview(unsigned subject,bool revolverator,bool golden){
    for(std::size_t i=0;i<previews_.size();++i)if(previews_[i].address==subject){
        if(revolverator&&ready()&&visual_valid(previews_[i]))return apply_visual(previews_[i],subject,golden?golden_class:native_class);
        restore_visual(previews_[i]);previews_.erase(previews_.begin()+i);break;
    }
    if(!revolverator||!ensure_mapping()||subject==visual_.address||previews_.size()>=32)return false;
    Visual preview;
    if(!valid(subject,256)||read<std::uint8_t>(subject+0x20)>=0x80||
       read<std::uint16_t>(subject+0xa6)!=source_class||
       read<unsigned>(subject+0x24)!=read<unsigned>(global(class_resources)+unsigned(slot_)*4))return false;
    // Reserve the host ownership record before changing the guest actor.
    previews_.push_back(preview);
    if(apply_visual(previews_.back(),subject,golden?golden_class:native_class))return true;
    previews_.pop_back();return false;
}
bool NativeWeaponAlias::fit_preview(unsigned subject,float factor){
    if(!std::isfinite(factor)||factor<=0||factor>1)return false;
    for(auto& preview:previews_)if(preview.address==subject&&visual_valid(preview)&&
        read<std::uint16_t>(subject+0xa6)==preview.render_class){
        const float current=read<float>(subject+0x2c);
        if(!std::isfinite(current)||current<=0||current>1000)return false;
        if(preview.scaled&&current!=preview.applied_scale)return false;
        if(!preview.scaled)preview.original_scale=current;
        preview.applied_scale=preview.original_scale*factor;preview.scaled=true;
        write(subject+0x2c,preview.applied_scale);return true;
    }
    return false;
}
void NativeWeaponAlias::reset() {
    restore_visual(visual_);restore_previews();
    if(mapping_owned_&&supported()&&api_->loaded_level()==level_&&global(class_map)==map_){
        for(unsigned render:{native_class,golden_class})if(read<std::uint8_t>(map_+render)==slot_)
            write<std::uint8_t>(map_+render,0xff);
    }
    level_=-1;map_=0;slot_=0xff;mapping_owned_=false;
}
}

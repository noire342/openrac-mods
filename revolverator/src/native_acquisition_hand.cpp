// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_acquisition_hand.hpp"
#include <cstring>

namespace revolverator {
namespace {
struct KeyHeader {
    float rate;
    short time;
    unsigned short quadwords, quaternion_bytes, scale_count, translation_offset, translation_count;
};
struct ScaleKey {
    std::array<unsigned short,3> axes;
    unsigned char joint, flags;
};
static_assert(sizeof(KeyHeader)==0x10&&sizeof(ScaleKey)==8);
constexpr auto key_header_bytes=static_cast<unsigned>(sizeof(KeyHeader));
constexpr auto scale_key_bytes=static_cast<unsigned>(sizeof(ScaleKey));
constexpr unsigned translation_key_bytes=8;
constexpr unsigned key_a=0x68;
constexpr unsigned forearm_joint=54;
constexpr unsigned maximum_joints=112;
constexpr unsigned short unit_scale=4096; // 4.12 fixed point.
}

bool NativeAcquisitionHand::same_actor()const noexcept{
    return actor_&&api_->loaded_level()==level_&&valid(actor_,256)&&
        read<unsigned short>(actor_+0xa6)==0&&read<unsigned>(actor_+0x24)==pclass_&&
        read<unsigned>(actor_+0x78)==variables_&&read<unsigned>(actor_+0x38)==stamp_;
}

bool NativeAcquisitionHand::copy_pose_key(unsigned original,unsigned destination,unsigned joint_count){
    if(!valid(original,sizeof(KeyHeader)))return false;
    const auto header=read<KeyHeader>(original);
    const unsigned bytes=key_header_bytes+16u*header.quadwords;
    const unsigned translation_end=header.translation_offset+translation_key_bytes*header.translation_count;
    if(header.quaternion_bytes!=joint_count*8u||
       header.translation_offset!=header.quaternion_bytes+scale_key_bytes*header.scale_count||
       header.translation_count>maximum_joints||header.scale_count>maximum_joints||
       header.quadwords!=(translation_end+15u)/16u||bytes>frame_capacity||!valid(original,bytes))return false;

    const unsigned scales=key_header_bytes+header.quaternion_bytes;
    for(unsigned i=0;i<header.scale_count;++i){
        const auto offset=scales+i*scale_key_bytes;
        auto scale=read<ScaleKey>(original+offset);
        // Both axes collapse and the inherited Z scale is one fixed-point step.
        // Other joints and authored end-of-limb corrections keep their keys.
        if(scale.joint!=forearm_joint||scale.flags!=0x80||
           scale.axes!=std::array<unsigned short,3>{0,0,1})continue;
        std::memcpy(api_->memory+destination,api_->memory+original,bytes);
        scale.axes.fill(unit_scale);
        write(destination+offset,scale);
        return true;
    }
    return false;
}

void NativeAcquisitionHand::publish(unsigned actor){
    restore();
    if(!valid(actor,256)||read<unsigned short>(actor+0xa6)!=0||read<unsigned char>(actor+0x20)>=0x80)return;
    const auto pclass=read<unsigned>(actor+0x24);
    if(!valid(pclass,0x48))return;
    const unsigned joints=read<unsigned char>(pclass+8);
    if(joints<=forearm_joint||joints>maximum_joints)return;
    if(!arena_.owns(scratch_,2*frame_capacity))scratch_=arena_.allocate(2*frame_capacity);
    if(!arena_.owns(scratch_,2*frame_capacity))return;
    level_=api_->loaded_level();actor_=actor;pclass_=pclass;
    variables_=read<unsigned>(actor+0x78);stamp_=read<unsigned>(actor+0x38);
    for(unsigned i=0;i<keys_.size();++i){
        const auto field=actor+key_a+i*4u;
        const auto original=read<unsigned>(field),copy=scratch_+i*frame_capacity;
        if(!copy_pose_key(original,copy,joints))continue;
        keys_[i]={original,copy};
        write(field,copy);
    }
}

void NativeAcquisitionHand::restore()noexcept{
    if(same_actor())for(unsigned i=0;i<keys_.size();++i){
        const auto field=actor_+key_a+i*4u;
        if(keys_[i].copy&&read<unsigned>(field)==keys_[i].copy)write(field,keys_[i].original);
    }
    keys_={};actor_=pclass_=variables_=stamp_=0;level_=-1;
}

void NativeAcquisitionHand::reset()noexcept{
    restore();
    scratch_=0;
}
}

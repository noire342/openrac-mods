// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_equip_audio.hpp"

namespace revolverator {
unsigned NativeEquipAudio::validated_class(unsigned actor)const{
    if(!api_||!api_->memory||!api_->loaded_level||!api_->relocate_data||
       api_->loaded_level()<0||api_->loaded_level()>18||!valid(actor,256)||
       read<unsigned char>(actor+0x20)>=0x80)return 0;
    const auto hero=global(0x13f350);
    const auto render=read<unsigned short>(actor+0xa6);
    if(!valid(hero,0x22ac)||read<unsigned>(hero+0x1090)!=actor||
       read<int>(hero+0x10b8)!=18||
       (render!=180&&render!=2017&&render!=2018))return 0;
    const auto klass=read<unsigned>(actor+0x24);
    if(!valid(klass,0x30)||read<unsigned char>(klass+0xd)<=mechanical_equip_entry)return 0;
    const auto definitions=read<unsigned>(klass+0x28);
    constexpr unsigned definition_bytes=32;
    if(!valid(definitions,definition_bytes*(mechanical_equip_entry+1))||
       read<int>(definitions+mechanical_equip_entry*definition_bytes+0x1c)<=0)return 0;
    return klass;
}
bool NativeEquipAudio::suppress_native_appearance(unsigned actor)const{
    const auto klass=validated_class(actor);
    if(!klass||(read<unsigned char>(actor+0x52)!=0&&read<unsigned char>(actor+0x53)!=0))return false;
    const auto sequence=read<unsigned>(klass+0x48);
    if(!valid(sequence,0x1c)||read<unsigned char>(sequence+0x12)!=1)return false;
    const auto trigger=sequence+0x1c+unsigned(read<unsigned char>(sequence+0x10))*4;
    if(!valid(trigger,4)||(read<unsigned>(trigger)&0xffffu)!=mechanical_equip_entry)return false;
    write<unsigned char>(actor+0x7e,0);
    return true;
}
NativeEquipAudio::Result NativeEquipAudio::play(unsigned actor)const{
    if(!validated_class(actor))return Result::Unavailable;
    using Play=int(*)(int,int,unsigned);
    const auto native=reinterpret_cast<Play>(find_function("FUN_0022da68"));
    if(!native)return Result::Unavailable;
    if(read<unsigned char>(actor+0x52)==0||read<unsigned char>(actor+0x53)==0){
        // The original glove's appear sequence contains precisely one draw
        // trigger. Its animation no longer owns our custom assembly. Transfer
        // that one pending trigger to the logical equip event, touching only
        // this actor's event count (never the shared class/sequence/bank).
        if(!suppress_native_appearance(actor))return Result::NativeAppearance;
    }
    return native(mechanical_equip_entry,0,actor)>=0?Result::Played:Result::Rejected;
}
}

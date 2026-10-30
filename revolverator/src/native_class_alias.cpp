// SPDX-License-Identifier: GPL-3.0-or-later
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
void NativeAmmoAlias::frame() {
    if(!supported()){reset();return;}
    const int current=api_->loaded_level();
    if(current!=level_){reset();level_=current;}
    if(owned_){
        if(ready())return;
        // Restore only our unchanged byte; another mod's later mapping wins.
        reset();level_=current;
    }
    const auto map=global(class_map);
    if(!valid(map,id_capacity)||read<std::uint8_t>(map+native_class)!=0xff)return;
    const auto source_slot=read<std::uint8_t>(map+source_class);
    if(!source_valid(map,source_slot))return;
    // register_moby_class normally appends all tables and mutates its class
    // header. This is an alias, not a new resource: deliberately share its
    // existing slot and leave all those native bindings byte-for-byte intact.
    map_=map;slot_=source_slot;owned_=true;
    write<std::uint8_t>(map_+native_class,slot_);
}
void NativeAmmoAlias::reset() {
    if(owned_&&supported()&&api_->loaded_level()==level_&&global(class_map)==map_&&
        read<std::uint8_t>(map_+native_class)==slot_)
        write<std::uint8_t>(map_+native_class,0xff);
    owned_=false;map_=0;slot_=0xff;level_=-1;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal.hpp"
#include <algorithm>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned item=18;
constexpr unsigned owned=0x13d4c0,unlocked=0x13d408,acquired=0x13d4e8;
constexpr unsigned ammo=0x13d428,variant=0x13e520,favourites=0x141ea0;
std::size_t field_width(std::size_t size,std::size_t at){
    // Keep native scalar ownership indivisible. Restoring separate changed
    // bytes of a price or class ID could splice two owners' values together.
    if(size==76&&at>=0x34&&at<0x44)return 2;
    if(size==24&&at>=8&&at<20)return 2;
    return std::min<std::size_t>(4,size-at);
}
bool field_owned(const std::vector<unsigned char>& mask,std::size_t at,std::size_t width){
    return std::any_of(mask.begin()+at,mask.begin()+at+width,[](unsigned char byte){return byte!=0;});
}
}
NativeArsenal::NativeArsenal(const openrac_api* api,arsenal::Registry& registry,ArsenalIdentityView& presentation)
    :Memory(api),registry_(registry),presentation_(presentation){}

arsenal::WalloperNativeState NativeArsenal::native_walloper()const {
    return {read<unsigned char>(global(owned)+item)!=0,
        read<unsigned char>(global(unlocked)+item),read<unsigned char>(global(acquired)+item),
        read<int>(global(ammo)+item*4),read<unsigned char>(global(variant)+item)};
}
void NativeArsenal::write_walloper(const arsenal::WalloperNativeState& state){
    write<unsigned char>(global(owned)+item,state.owned?1:0);
    write(global(unlocked)+item,state.unlocked);write(global(acquired)+item,state.acquired);
    write(global(ammo)+item*4,state.ammo);write(global(variant)+item,state.text_variant);
}
void NativeArsenal::capture_table(unsigned address,unsigned size){
    if(!valid(address,size))return;
    for(const auto& table:tables_)if(table.address==address)return;
    Table table;table.address=address;table.original.resize(size);
    std::memcpy(table.original.data(),api_->memory+address,size);
    table.custom=table.last=table.original;table.mask.assign(size,0);
    tables_.push_back(std::move(table));
}
void NativeArsenal::import_native_inventory(){
    registry_.set_walloper(native_walloper());
    if(initialized_)return;
    for(unsigned slot=0;slot<8;++slot){
        const int key=read<int>(global(favourites)+slot*4);
        if(key>=0&&key<=int(arsenal::Registry::MaximumNativeKey))registry_.assign_quick(slot,unsigned(key));
    }
    const int held=read<int>(global(0x13f350)+0x10b8);
    if(held>=0&&held<=int(arsenal::Registry::MaximumNativeKey))registry_.equip(unsigned(held));
    initialized_=true;
}
void NativeArsenal::begin_level(){
    reset();level_=api_->loaded_level();
    if(level_<0||level_>=19)return;
    import_native_inventory();
    capture_table(global(0x1863d0)+item*76,76);
    capture_table(level(0x179ac0)+item*76,76);
    capture_table(level(0x1c40b0)+item*24,24);
    capture_table(level(0x1c8ff0)+item*64,64);
}
void NativeArsenal::finish_level(){
    if(level_!=api_->loaded_level()||tables_.empty())return;
    for(auto& table:tables_){
        std::memcpy(table.custom.data(),api_->memory+table.address,table.custom.size());
        if(table.custom.size()==76){
            const auto icon=static_cast<unsigned short>(60037);
            std::memcpy(table.custom.data()+0x38,&icon,2);
        }
        for(std::size_t i=0;i<table.original.size();++i)table.mask[i]=table.custom[i]!=table.original[i];
        table.last=table.custom;
    }
    ready_=true;projected_=arsenal::RevolveratorKey;
    // Initialization produced custom metadata, but never granted custom stock.
    apply_tables(true);apply_state(projected_);restore_gameplay();
}
void NativeArsenal::commit(){
    if(!ready_||level_!=api_->loaded_level())return;
    if(const auto* state=registry_.custom_state(projected_)){
        auto next=*state;
        next.owned=read<unsigned char>(global(owned)+item)!=0;
        const int count=read<int>(global(ammo)+item*4);
        const auto* descriptor=registry_.descriptor(projected_);
        next.ammo=unsigned(std::clamp(count,0,int(descriptor->capacity)));
        registry_.set_state(projected_,next);
    }else registry_.set_walloper(native_walloper());
}
void NativeArsenal::apply_tables(bool custom){
    for(auto& table:tables_){
        const auto& target=custom?table.custom:table.original;
        for(std::size_t i=0;i<target.size();){
            const auto width=field_width(target.size(),i);
            // Only fields owned by the adapter are projected. Unrelated damage,
            // animation and material edits remain under their original owner.
            if(field_owned(table.mask,i,width)){
                std::memcpy(api_->memory+table.address+i,target.data()+i,width);
                std::memcpy(table.last.data()+i,target.data()+i,width);
            }
            i+=width;
        }
    }
}
void NativeArsenal::apply_state(arsenal::WeaponKey key){
    if(const auto* state=registry_.custom_state(key)){
        arsenal::WalloperNativeState transport;
        transport.owned=state->owned;transport.unlocked=transport.acquired=state->owned?1:0;
        transport.ammo=int(state->ammo);write_walloper(transport);
    }else write_walloper(registry_.walloper());
}
void NativeArsenal::project(arsenal::WeaponKey key){
    if(!ready_||level_!=api_->loaded_level())return;
    const bool custom=registry_.descriptor(key)!=nullptr;
    const auto next=custom?key:arsenal::NativeWalloperKey;
    if(next!=projected_){commit();projected_=next;apply_tables(custom);apply_state(next);}
    presentation_.select_identity(custom);
}
void NativeArsenal::restore_gameplay(){project(registry_.equipped());}
bool NativeArsenal::equipped_revolverator()const{return registry_.equipped()==arsenal::RevolveratorKey;}
bool NativeArsenal::revolverator_active()const{
    return ready_&&equipped_revolverator()&&read<int>(global(0x13f350)+0x10b8)==int(item);
}
void NativeArsenal::observe_equipped(){
    if(read<int>(level(0x15f5c4))!=0)return;
    const int held=read<int>(global(0x13f350)+0x10b8);
    const int pending=read<int>(global(0x13f350)+0x20b8),equipped=read<int>(global(0x141660));
    // The native switch consumes pending before the old held actor is retired.
    // Keep a logical custom selection until the held and persisted native IDs
    // agree; otherwise that transient old weapon erases the logical identity.
    if(registry_.descriptor(registry_.equipped())&&(pending==int(item)||equipped==int(item)))return;
    if(held!=int(item)&&held>0&&held<=int(arsenal::Registry::MaximumNativeKey)&&held==equipped)
        registry_.equip(unsigned(held));
}
void NativeArsenal::project_quick_slots(bool native_save){
    auto slots=registry_.native_slots();
    if(native_save)for(unsigned i=0;i<slots.size();++i)if(registry_.quick()[i]>=arsenal::Registry::FirstCustomKey)slots[i]=0;
    write(global(favourites),slots);
}
void NativeArsenal::reset(){
    if(ready_&&level_==api_->loaded_level()){
        commit();
        for(const auto& table:tables_)for(std::size_t i=0;i<table.original.size();){
            const auto width=field_width(table.original.size(),i);
            if(field_owned(table.mask,i,width)&&std::memcmp(api_->memory+table.address+i,table.last.data()+i,width)==0)
                std::memcpy(api_->memory+table.address+i,table.original.data()+i,width);
            i+=width;
        }
        write_walloper(registry_.walloper());presentation_.select_identity(false);
    }
    tables_.clear();ready_=false;level_=-1;projected_=arsenal::NativeWalloperKey;
}
}

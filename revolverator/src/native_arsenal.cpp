// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal.hpp"
#include "native_arsenal_layout.hpp"
#include "eqs_probe.hpp"
#include <algorithm>
#include <cstring>

namespace revolverator {
namespace {
using namespace native_arsenal_layout;
constexpr unsigned item = arsenal::NativeWalloperKey;
constexpr unsigned required_moby_class_bytes = 0x4c;

std::size_t field_width(std::size_t size,std::size_t at){
    // Keep native scalar ownership indivisible. Restoring separate changed
    // bytes of a price or class ID could splice two owners' values together.
    if(size==item_definition_size&&at>=pickup_plural_text&&at<item_definition_halfwords_end)
        return sizeof(std::uint16_t);
    if(size==vendor_item_info_size&&at>=ammo_unit_price&&at<initial_ammo+sizeof(std::uint16_t))
        return sizeof(std::uint16_t);
    return std::min(sizeof(std::uint32_t),size-at);
}
bool field_owned(const std::vector<unsigned char>& mask,std::size_t at,std::size_t width){
    return std::any_of(mask.begin()+at,mask.begin()+at+width,[](unsigned char byte){return byte!=0;});
}
}
NativeArsenal::NativeArsenal(const openrac_api* api,arsenal::Registry& registry,ArsenalIdentityView& presentation)
    :Memory(api),registry_(registry),presentation_(presentation){}

arsenal::WalloperNativeState NativeArsenal::native_walloper()const {
    return {read<unsigned char>(global(items_owned)+item)!=0,
        0,read<unsigned char>(global(items_acquired)+item),
        read<int>(global(weapon_ammo_counts)+item*sizeof(int)),read<unsigned char>(global(item_text_variant)+item)};
}
void NativeArsenal::write_walloper(const arsenal::WalloperNativeState& state){
    write<unsigned char>(global(items_owned)+item,state.owned?1:0);
    // The legacy codec's "unlocked" byte never represented weapon state:
    // canonical 0x13d408 is the skill-point table. Do not read, project or
    // restore it, including when loading a sidecar from an earlier mod build.
    write(global(items_acquired)+item,state.acquired);
    write(global(weapon_ammo_counts)+item*sizeof(int),state.ammo);
    write(global(item_text_variant)+item,state.text_variant);
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
    for(unsigned slot=0;slot<quick_slot_count;++slot){
        const int key=read<int>(global(quick_slots)+slot*sizeof(int));
        if(key>=0&&key<=int(arsenal::Registry::MaximumNativeKey))registry_.assign_quick(slot,unsigned(key));
    }
    const int held=read<int>(global(hero)+held_item);
    if(held>=0&&held<=int(arsenal::Registry::MaximumNativeKey))registry_.equip(unsigned(held));
    initialized_=true;
}
void NativeArsenal::begin_level(){
    reset();level_=api_->loaded_level();
    if(level_<0||level_>last_campaign_level)return;
    import_native_inventory();
    capture_table(global(resident_item_definitions)+item*item_definition_size,item_definition_size);
    capture_table(level(level_item_definitions)+item*item_definition_size,item_definition_size);
    capture_table(level(vendor_item_info)+item*vendor_item_info_size,vendor_item_info_size);
    capture_table(level(vendor_item_slots)+item*vendor_item_slot_size,vendor_item_slot_size);
}
void NativeArsenal::finish_level(){
    if(level_!=api_->loaded_level()||tables_.empty())return;
    for(auto& table:tables_){
        std::memcpy(table.custom.data(),api_->memory+table.address,table.custom.size());
        if(table.custom.size()==item_definition_size)
            std::memcpy(table.custom.data()+item_icon_bank,&revolverator_icon_bank,sizeof(revolverator_icon_bank));
        for(std::size_t i=0;i<table.original.size();++i)table.mask[i]=table.custom[i]!=table.original[i];
        table.last=table.custom;
    }
    ready_=true;projected_=arsenal::RevolveratorKey;
    // Initialization produced custom metadata, but never granted custom stock.
    apply_tables(true);apply_state(projected_);restore_gameplay();
}
void NativeArsenal::commit(){
    if(!ready_||level_!=api_->loaded_level())return;
    if(foreign_&&foreign_->contains(projected_)){foreign_->commit(projected_);return;}
    if(const auto* descriptor=registry_.descriptor(projected_);descriptor&&descriptor->carrier==item){
        const auto* state=registry_.custom_state(projected_);if(!state)return;
        auto next=*state;
        next.owned=read<unsigned char>(global(items_owned)+item)!=0;
        const int count=read<int>(global(weapon_ammo_counts)+item*sizeof(int));
        next.ammo=unsigned(std::clamp(count,0,int(descriptor->capacity)));
        registry_.set_state(projected_,next);
    }else registry_.set_walloper(native_walloper());
}
void NativeArsenal::apply_tables(bool custom){
    const bool native_wheel=eqs_probe::installed();
    for(auto& table:tables_){
        const auto& target=custom?table.custom:table.original;
        for(std::size_t i=0;i<target.size();){
            const auto width=field_width(target.size(),i);
            // Only fields owned by the adapter are projected. Unrelated damage,
            // animation and material edits remain under their original owner.
            // EQS uses native IDs on all pages. Keep their shared Walloper
            // icon canonical while the custom weapon is equipped.
            if(field_owned(table.mask,i,width)){
                const bool shared_wheel_icon=native_wheel &&
                    table.original.size()==item_definition_size && i==item_icon_bank;
                const auto& view=shared_wheel_icon?table.original:target;
                std::memcpy(api_->memory+table.address+i,view.data()+i,width);
                std::memcpy(table.last.data()+i,view.data()+i,width);
            }
            i+=width;
        }
    }
}
void NativeArsenal::apply_state(arsenal::WeaponKey key){
    if(const auto* state=registry_.custom_state(key)){
        arsenal::WalloperNativeState transport;
        transport.owned=state->owned;transport.acquired=state->owned?1:0;
        transport.ammo=int(state->ammo);write_walloper(transport);
    }else write_walloper(registry_.walloper());
}
void NativeArsenal::project(arsenal::WeaponKey key){
    if(!ready_||level_!=api_->loaded_level())return;
    const auto* descriptor=registry_.descriptor(key);
    const bool external=descriptor&&descriptor->carrier!=item&&foreign_&&foreign_->contains(key);
    if(external&&key==projected_)return;
    if(foreign_&&foreign_->contains(projected_)){
        commit();
        // A failed withdrawal keeps the existing view and its owner alive.
        if(!foreign_->project(projected_,false))return;
        projected_=arsenal::NativeWalloperKey;
    }
    if(external){
        commit();
        if(projected_!=arsenal::NativeWalloperKey){
            projected_=arsenal::NativeWalloperKey;apply_tables(false);apply_state(projected_);
        }
        presentation_.select_identity(false);
        if(foreign_->project(key,true))projected_=key;
        else request_equipped();
        return;
    }
    const bool custom=descriptor&&descriptor->carrier==item;
    const auto next=custom?key:arsenal::NativeWalloperKey;
    if(next!=projected_){commit();projected_=next;apply_tables(custom);apply_state(next);}
    presentation_.select_identity(custom);
}
void NativeArsenal::restore_gameplay(){
    project(registry_.equipped());
    if(foreign_)foreign_->selection_changed(registry_.equipped());
}
void NativeArsenal::set_foreign_view(ForeignArsenalView* view){
    if(view==foreign_)return;
    if(foreign_&&foreign_->contains(projected_)){
        commit();
        if(!foreign_->project(projected_,false))return;
        projected_=arsenal::NativeWalloperKey;
    }
    foreign_=view;
}
void NativeArsenal::detach_foreign_view(ForeignArsenalView* view)noexcept{
    if(foreign_!=view)return;
    foreign_=nullptr;
    if(const auto* descriptor=registry_.descriptor(projected_);descriptor&&descriptor->carrier!=item){
        projected_=arsenal::NativeWalloperKey;
        if(ready_&&level_==api_->loaded_level()){
            apply_tables(false);write_walloper(registry_.walloper());presentation_.select_identity(false);
        }
    }
}
bool NativeArsenal::request_equipped(){
    if(!ready_||level_!=api_->loaded_level())return false;
    const auto key=registry_.equipped();const auto* definition=registry_.descriptor(key);
    const auto native=definition?definition->carrier:key;
    const auto hero_address=global(hero),selected=global(hand_selection);
    if(!native||native>arsenal::Registry::MaximumNativeKey||
       !valid(hero_address,pending_hand+sizeof(int))||!valid(selected,sizeof(int)))return false;
    write<int>(selected,int(native));write<int>(hero_address+pending_hand,int(native));return true;
}
void NativeArsenal::give_item(int native_item, int equip, bool custom_grant, GiveItem original) {
    if (!ready_ || level_ != api_->loaded_level()) {
        original(native_item, equip);
        return;
    }

    const auto quick_address = global(quick_slots);
    const int grant_level = level_;
    using Quick = std::array<int, quick_slot_count>;
    const auto quick_before = read<Quick>(quick_address);
    const bool carrier = native_item == int(arsenal::NativeWalloperKey);
    const bool revolverator = carrier && custom_grant;
    using Stock = std::array<unsigned char, vendor_stock_size>;
    unsigned stock_address = 0;
    Stock stock_before{};
    bool first_native_walloper = false;
    if (carrier) {
        project(revolverator ? arsenal::RevolveratorKey : arsenal::NativeWalloperKey);
        if (revolverator) {
            stock_address = global(vendor_stock);
            stock_before = read<Stock>(stock_address);
        } else {
            first_native_walloper = !native_walloper().owned;
        }
    }

    original(native_item, equip);
    if (!ready_ || level_ != grant_level || api_->loaded_level() != grant_level) return;

    if (carrier) commit();
    const bool native_wheel=eqs_probe::installed();
    const auto quick_after = read<Quick>(quick_address);
    if (native_wheel) {
        if (revolverator) {
            // Undo only the native grant's first-empty-slot insertion.
            const auto empty=std::find(quick_before.begin(),quick_before.end(),0);
            if(empty!=quick_before.end()){
                const auto slot=std::size_t(empty-quick_before.begin());
                if(quick_after[slot]==native_item)write<int>(quick_address+unsigned(slot)*sizeof(int),0);
            }
        }
    } else {
        for (std::size_t slot = 0; slot < quick_after.size(); ++slot) {
            const int value = quick_after[slot];
            if (value == quick_before[slot] || value < 0 ||
                value > int(arsenal::Registry::MaximumNativeKey)) continue;
            const auto key = revolverator && value == native_item
                ? arsenal::RevolveratorKey : arsenal::WeaponKey(value);
            registry_.assign_quick(slot, key);
        }
    }

    if (!carrier) return;
    const auto hero_address = global(hero);
    if (!revolverator) {
        // Observe the native first-acquisition hand request. The engine owns
        // every native equipment slot and decides whether a grant equips it.
        if (first_native_walloper && equip && valid(hero_address, pending_hand + sizeof(int)) &&
            read<int>(hero_address + pending_hand) == native_item) {
            registry_.equip(arsenal::NativeWalloperKey);
        }
        return;
    }

    const auto stock_after = read<Stock>(stock_address);
    for (std::size_t slot = 0; slot < stock_after.size(); ++slot) {
        if (stock_after[slot] != stock_before[slot] && (stock_after[slot] & vendor_stock_item_mask) == item) {
            write<unsigned char>(stock_address + unsigned(slot), stock_before[slot]);
        }
    }
    const auto& quick = registry_.quick();
    // EQS owns the visible and saved native wheel assignments. Auto-picking
    // an empty slot in Revolverator's private logical registry can otherwise
    // draw a Revolverator icon on top of an unrelated EQS gadget.
    if (!native_wheel &&
        registry_.owned(arsenal::RevolveratorKey) &&
        std::find(quick.begin(), quick.end(), arsenal::RevolveratorKey) == quick.end()) {
        const auto empty = std::find(quick.begin(), quick.end(), 0u);
        if (empty != quick.end()) registry_.assign_quick(std::size_t(empty - quick.begin()), arsenal::RevolveratorKey);
    }
    // Explicit custom re-equip may reuse an already-owned native carrier.
    if (equip && valid(hero_address, pending_hand + sizeof(int)) && registry_.equip(arsenal::RevolveratorKey)) {
        write<int>(hero_address + pending_hand, native_item);
    }
    project_quick_slots();
}
bool NativeArsenal::native_inventory(arsenal::WeaponKey key,arsenal_inventory_state_v1& state)const{
    return foreign_&&key!=item&&foreign_->native_inventory(key,state);
}
bool NativeArsenal::native_owned(arsenal::WeaponKey key,bool& owned)const{
    arsenal_inventory_state_v1 state{};state.size=sizeof(state);
    if(!native_inventory(key,state))return false;
    owned=state.owned!=0;return true;
}
bool NativeArsenal::equipped_revolverator()const{return registry_.equipped()==arsenal::RevolveratorKey;}
bool NativeArsenal::revolverator_active()const{
    return ready_&&equipped_revolverator()&&read<int>(global(hero)+held_item)==int(item);
}
void NativeArsenal::mark_snapshot_restored(){
    initialized_=true;
    const auto key=registry_.equipped();
    restored_selection_=registry_.descriptor(key)&&registry_.owned(key)?key:0;
}
void NativeArsenal::observe_equipped(){
    if(!ready_||level_!=api_->loaded_level())return;
    if(restored_selection_&&registry_.equipped()!=restored_selection_)restored_selection_=0;
    if(read<int>(level(game_mode))!=gameplay_mode)return;
    const auto hero_address=global(hero);
    if(restored_selection_){
        const auto* descriptor=registry_.descriptor(restored_selection_);
        if(!descriptor||!registry_.owned(restored_selection_))restored_selection_=0;
        else {
            // A native-compatible save deliberately stores the wrench instead
            // of a custom transport selection. Restore it once after the actual
            // player is ready, before observing that fallback as a user switch.
            const auto body=read<unsigned>(hero_address+hero_moby);
            const int state=read<int>(hero_address+hero_state);
            if(!valid(hero_address,hero_health+sizeof(int))||read<int>(hero_address+hero_health)<=0||
               state==hero_dying||state==hero_dying_zero_health||state==hero_fatal_fall||
               !valid(body,moby_size)||read<std::uint16_t>(body+moby_class)!=ratchet_class||
               read<std::uint8_t>(body+moby_state)>=moby_unavailable_state||
               !valid(read<unsigned>(body+moby_class_pointer),required_moby_class_bytes))return;
            const int carrier=int(descriptor->carrier);
            if(restored_selection_==arsenal::RevolveratorKey){
                const int held=read<int>(hero_address+held_item);
                const auto selected=hero_address+hero_selected_item;
                // The native switch discards a request matching the cached hand
                // selection, even if a checkpoint restored a different held item.
                if((held==int(arsenal::NativeWrenchKey)||held==empty_hand_item)&&read<int>(selected)==carrier)
                    write<int>(selected,held);
            }
            write<int>(global(hand_selection),carrier);
            write<int>(hero_address+pending_hand,carrier);
            project_quick_slots();restored_selection_=0;return;
        }
    }
    const int held=read<int>(hero_address+held_item);
    const int pending=read<int>(hero_address+pending_hand),equipped=read<int>(global(hand_selection));
    // The native switch consumes pending before the old held actor is retired.
    // Keep a logical custom selection until the held and persisted native IDs
    // agree; otherwise that transient old weapon erases the logical identity.
    if(const auto* descriptor=registry_.descriptor(registry_.equipped());descriptor&&
       (pending==int(descriptor->carrier)||equipped==int(descriptor->carrier)))return;
    if(held!=int(item)&&held>0&&held<=int(arsenal::Registry::MaximumNativeKey)&&held==equipped)
        registry_.equip(unsigned(held));
}
void NativeArsenal::project_quick_slots(bool native_save){
    // EQS owns the game's native favourites array for all its pages.
    // Revolverator has only one logical eight-slot registry; projecting it
    // (on grants, level start, or equipment restoration) silently replaces
    // EQS's page-one assignments, then poisons subsequent editor captures.
    // Keep logical Revolverator state but do not write native favourites.
    // Without EQS, retain the exact original projection and save behaviour.
    if(eqs_probe::installed())return;
    auto slots=registry_.native_slots();
    if(native_save)for(unsigned i=0;i<slots.size();++i)if(registry_.quick()[i]>=arsenal::Registry::FirstCustomKey)slots[i]=0;
    write(global(quick_slots),slots);
}
void NativeArsenal::reset(){
    restored_selection_=0;
    if(ready_&&level_==api_->loaded_level()){
        commit();
        if(foreign_&&foreign_->contains(projected_))foreign_->project(projected_,false);
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

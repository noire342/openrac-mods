// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arsenal_state.hpp"
#include <algorithm>
#include <climits>

namespace arsenal {
namespace {
bool stable_id_valid(const std::string& id){
    if(id.empty()||id.size()>128)return false;
    for(unsigned char c:id)if(c<=32||c>=127)return false;
    return true;
}
bool state_equal(const State& a,const State& b){return a.owned==b.owned&&a.ammo==b.ammo&&a.xp==b.xp&&a.golden==b.golden;}
bool walloper_equal(const WalloperNativeState& a,const WalloperNativeState& b){
    return a.owned==b.owned&&a.unlocked==b.unlocked&&a.acquired==b.acquired&&
           a.ammo==b.ammo&&a.text_variant==b.text_variant;
}
bool records_equal(const std::vector<Record>& a,const std::vector<Record>& b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].key!=b[i].key||a[i].stable_id!=b[i].stable_id||!state_equal(a[i].state,b[i].state))return false;
    return true;
}
bool basic_state_valid(const State& state){return state.ammo<=Registry::MaximumCapacity&&state.xp<=Registry::MaximumXp&&(!state.golden||state.owned);}
auto find_record(std::vector<Record>& records,WeaponKey key){
    return std::find_if(records.begin(),records.end(),[=](const Record& r){return r.key==key;});
}
auto find_record(const std::vector<Record>& records,WeaponKey key){
    return std::find_if(records.begin(),records.end(),[=](const Record& r){return r.key==key;});
}
void sort_records(std::vector<Record>& records){
    std::sort(records.begin(),records.end(),[](const Record& a,const Record& b){return a.key<b.key;});
}
}

const Descriptor* Registry::descriptor(WeaponKey key)const {
    const auto at=std::find_if(descriptors_.begin(),descriptors_.end(),[=](const Descriptor& d){return d.key==key;});
    return at==descriptors_.end()?nullptr:&*at;
}
const State* Registry::custom_state(WeaponKey key)const {
    const auto at=find_record(records_,key);
    return at==records_.end()?nullptr:&at->state;
}
bool Registry::register_weapon(const Descriptor& definition){
    if(sealed_||descriptors_.size()>=MaxDescriptors||definition.key<FirstCustomKey||
       !stable_id_valid(definition.stable_id)||definition.carrier!=NativeWalloperKey||
       definition.icon_bank==0||definition.icon_bank>65535||definition.capacity>MaximumCapacity||
       definition.price>INT_MAX||definition.first_level>18||descriptor(definition.key))return false;
    for(const auto& d:descriptors_)if(d.stable_id==definition.stable_id)return false;
    for(const auto& r:records_)if(r.stable_id==definition.stable_id&&r.key!=definition.key)return false;
    auto next_records=records_;
    auto at=find_record(next_records,definition.key);
    if(at!=next_records.end()){
        if(at->stable_id!=definition.stable_id||at->state.ammo>definition.capacity||!basic_state_valid(at->state))return false;
    }else{
        if(next_records.size()>=MaxSnapshotRecords)return false;
        next_records.push_back({definition.key,definition.stable_id,{}});
        sort_records(next_records);
    }
    auto next_descriptors=descriptors_;
    next_descriptors.push_back(definition);
    records_.swap(next_records);descriptors_.swap(next_descriptors);++revision_;return true;
}
void Registry::seal(){if(!sealed_){sealed_=true;++revision_;}}
void Registry::unbind(WeaponKey key){
    for(auto& binding:quick_)if(binding==key)binding=0;
    if(selected_==key)selected_=NativeWrenchKey;
}
bool Registry::set_walloper(const WalloperNativeState& state){
    if(state.ammo<0)return false;
    if(walloper_equal(walloper_,state))return true;
    walloper_=state;if(!state.owned)unbind(NativeWalloperKey);++revision_;return true;
}
bool Registry::set_state(WeaponKey key,const State& state){
    const auto* d=descriptor(key);
    if(!d||!basic_state_valid(state)||state.ammo>d->capacity)return false;
    const auto at=find_record(records_,key);
    if(at==records_.end())return false;
    if(state_equal(at->state,state))return true;
    at->state=state;if(!state.owned)unbind(key);++revision_;return true;
}
bool Registry::owned(WeaponKey key)const {
    if(key==NativeWalloperKey)return walloper_.owned;
    if(key==NativeWrenchKey)return true;
    const auto* d=descriptor(key);const auto* s=d?custom_state(key):nullptr;
    return s&&s->owned;
}
bool Registry::golden(WeaponKey key)const {
    const auto* state=descriptor(key)?custom_state(key):nullptr;
    return state&&state->owned&&state->golden;
}
GoldenPurchaseResult Registry::purchase_golden(WeaponKey key,int& bolts,std::uint32_t price){
    const auto* definition=descriptor(key);const auto* current=definition?custom_state(key):nullptr;
    if(!current)return GoldenPurchaseResult::UnknownWeapon;
    if(!current->owned)return GoldenPurchaseResult::BaseMissing;
    if(current->golden)return GoldenPurchaseResult::AlreadyOwned;
    if(!price||price>std::uint32_t(INT_MAX))return GoldenPurchaseResult::InvalidPrice;
    if(bolts<0||std::uint32_t(bolts)<price)return GoldenPurchaseResult::InsufficientBolts;
    auto upgraded=*current;upgraded.golden=true;
    if(!set_state(key,upgraded))return GoldenPurchaseResult::UnknownWeapon;
    bolts-=int(price);return GoldenPurchaseResult::Purchased;
}
bool Registry::can_equip(WeaponKey key)const {
    if(key<=MaximumNativeKey)return key!=NativeWalloperKey||walloper_.owned;
    return owned(key);
}
bool Registry::equip(WeaponKey key){
    if(!can_equip(key))return false;
    if(selected_!=key){selected_=key;++revision_;}return true;
}
WeaponKey Registry::equipped()const{return can_equip(selected_)?selected_:NativeWrenchKey;}
bool Registry::assign_quick(std::size_t slot,WeaponKey key){
    if(slot>=quick_.size()||!can_equip(key))return false;
    auto next=quick_;
    if(key)for(auto& binding:next)if(binding==key)binding=0;
    next[slot]=key;
    if(next!=quick_){quick_=next;++revision_;}return true;
}
std::array<std::uint32_t,8> Registry::native_slots()const {
    std::array<std::uint32_t,8> native{};
    for(std::size_t i=0;i<quick_.size();++i){
        const WeaponKey key=quick_[i];
        if(key<=MaximumNativeKey){if(can_equip(key))native[i]=key;}
        else if(const auto* d=descriptor(key);d&&owned(key))native[i]=d->carrier;
    }
    return native;
}
Snapshot Registry::snapshot()const {return {1,walloper_,records_,quick_,selected_};}
bool Registry::restore(const Snapshot& snapshot){
    if(snapshot.format!=1||snapshot.walloper.ammo<0||snapshot.records.size()>MaxSnapshotRecords)return false;
    auto records=snapshot.records;
    for(std::size_t i=0;i<records.size();++i){
        const auto& r=records[i];
        if(r.key<FirstCustomKey||!stable_id_valid(r.stable_id)||!basic_state_valid(r.state))return false;
        for(std::size_t j=0;j<i;++j)if(records[j].key==r.key||records[j].stable_id==r.stable_id)return false;
        if(const auto* d=descriptor(r.key);d){
            if(d->stable_id!=r.stable_id||r.state.ammo>d->capacity)return false;
        }
        for(const auto& d:descriptors_)if(d.stable_id==r.stable_id&&d.key!=r.key)return false;
    }
    for(const auto& d:descriptors_){
        if(find_record(records,d.key)==records.end())records.push_back({d.key,d.stable_id,{}});
    }
    if(records.size()>MaxSnapshotRecords)return false;
    const auto valid_reference=[&](WeaponKey key){
        if(key<=MaximumNativeKey)return key!=NativeWalloperKey||snapshot.walloper.owned;
        if(key<FirstCustomKey)return false;
        const auto at=find_record(records,key);
        return at!=records.end()&&at->state.owned;
    };
    if(!valid_reference(snapshot.equipped))return false;
    for(std::size_t i=0;i<snapshot.quick.size();++i){
        const WeaponKey key=snapshot.quick[i];
        if(!valid_reference(key))return false;
        if(key)for(std::size_t j=0;j<i;++j)if(snapshot.quick[j]==key)return false;
    }
    sort_records(records);
    if(walloper_equal(walloper_,snapshot.walloper)&&records_equal(records_,records)&&
       quick_==snapshot.quick&&selected_==snapshot.equipped)return true;
    records_.swap(records);walloper_=snapshot.walloper;quick_=snapshot.quick;selected_=snapshot.equipped;++revision_;return true;
}
}

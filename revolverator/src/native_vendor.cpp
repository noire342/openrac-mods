// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "native_vendor.hpp"
#include "native_ammunition.hpp"
#include <algorithm>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned item_prices=0x1c40b0;
constexpr unsigned item_owned=0x13d4c0;
constexpr unsigned level_visits=0x13dd58;
constexpr unsigned vendor_menu=0x1ca4c0;
constexpr unsigned vendor_sounds=0x1c2108;
constexpr unsigned rows_offset=0xd0;
constexpr unsigned count_offset=0x210;
constexpr unsigned selection_offset=0x58;
constexpr unsigned vendor_item_slots=0x1c8ff0;
constexpr unsigned game_mode=0x15f5c4;
}

NativeVendor::NativeVendor(const openrac_api* api):Memory(api){}
NativeVendor::~NativeVendor(){reset();}
bool NativeVendor::supported() const {
    return api_&&api_->memory&&api_->game&&api_->version&&
        std::strcmp(api_->game,"rac1")==0&&std::strcmp(api_->version,"ntsc")==0&&
        api_->loaded_level()>=0&&api_->loaded_level()<19;
}
bool NativeVendor::unlocked() const {
    if(!supported())return false;
    // level_available at 0x13DD40 is only the map unlock. The distinct saved
    // visit byte is written on actual entry (0=new,1=entered,2=done).
    // Source: rnc/gameplay/state/level_state.h; rendering/00245c28.c.
    return api_->loaded_level()==first_level||
        read<std::uint8_t>(global(level_visits)+first_level)!=0;
}
void NativeVendor::frame() {
    const int current=api_->loaded_level();
    if(current!=level_){reset();level_=current;}
    if(!supported())return;
    // vsync consumes the native deferred draw before mods::frame (see
    // port/game/common/program/vsync.cpp). Return to the native class now so
    // the following menu update/reinitialization never sees a render-only ID.
    before_vendor_update();
    const unsigned address=level(item_prices)+item_id*24;
    if(!valid(address,24))return;
    if(!price_owned_){
        price_address_=address;original_price_=read<std::int32_t>(address);
        write<std::int32_t>(address,normal_price);price_owned_=true;
    }
    const unsigned preview_model=level(vendor_item_slots)+item_id*0x40+0x30;
    if(!preview_model_owned_&&valid(preview_model,4)&&read<std::uint16_t>(address+8)!=0){
        preview_model_address_=preview_model;original_preview_model_=read<int>(preview_model);
        applied_preview_class_=preview_class_;write<std::int32_t>(preview_model,int(preview_class_));
        preview_model_owned_=true;
    }
    if(preview_model_owned_&&preview_class_!=applied_preview_class_&&read<int>(preview_model_address_)==int(applied_preview_class_)){
        write<int>(preview_model_address_,int(preview_class_));applied_preview_class_=preview_class_;
    }
}
void NativeVendor::before_build(){frame();}
void NativeVendor::after_build(int skip_unflagged) {
    frame();
    if(!supported()||!price_owned_)return;
    const unsigned base=level(vendor_menu),rows=base+rows_offset;
    if(!valid(base,count_offset+4))return;
    const int count=read<std::int32_t>(base+count_offset);
    if(count<0||count>16)return;

    MenuPatch next;
    next.rows=rows;next.selection=base+selection_offset;
    next.original=read<std::array<Row,16>>(rows);next.applied=next.original;
    next.original_count=count;
    next.original_selection=read<std::int32_t>(next.selection);
    const bool owned=read<std::uint8_t>(global(item_owned)+item_id)!=0;
    const bool ammunition=read<std::uint16_t>(price_address_+8)!=0;
    const bool can_offer=owned?ammunition:(!skip_unflagged&&unlocked());
    bool listed=false;
    int out=0,selected=-1;
    for(int i=0;i<count;++i){
        Row row=next.original[unsigned(i)];
        if(row.item==item_id){
            if(!can_offer||listed)continue;
            // Repair only this row's weapon/ammo view if a pre-existing save's
            // stock bit and owned flag differ. Never grant or clear the item.
            row.is_ammo=owned?1:0;listed=true;
        }
        if(i==next.original_selection)selected=out;
        next.applied[unsigned(out++)]=row;
    }
    if(can_offer&&!listed&&out<16){
        const int sound=read<std::int32_t>(level(vendor_sounds)+item_id*20);
        next.applied[unsigned(out++)]={item_id,owned?1:0,0,0,sound};
    }
    next.applied_count=out;
    if(selected<0)selected=std::clamp(next.original_selection,0,std::max(0,out-1));
    next.applied_selection=selected;
    next.active=next.applied_count!=next.original_count||
        next.applied_selection!=next.original_selection||
        std::memcmp(next.original.data(),next.applied.data(),sizeof(next.original))!=0;
    if(next.active){
        write(rows,next.applied);write(base+count_offset,next.applied_count);
        write(next.selection,next.applied_selection);
    }
    menu_=next;
}
void NativeVendor::restore_menu() {
    if(!menu_.active||api_->loaded_level()!=level_)return;
    const auto current=read<std::array<Row,16>>(menu_.rows);
    if(read<int>(menu_.rows+sizeof(current))!=menu_.applied_count||
       read<int>(menu_.selection)!=menu_.applied_selection||
       std::memcmp(current.data(),menu_.applied.data(),sizeof(current))!=0)return;
    write(menu_.rows,menu_.original);write(menu_.rows+sizeof(current),menu_.original_count);
    write(menu_.selection,menu_.original_selection);
}
bool NativeVendor::preview_valid() const {
    return preview_.address&&api_->loaded_level()==level_&&valid(preview_.address,0x100)&&
        read<unsigned>(preview_.address+0x24)==preview_.pclass&&
        read<unsigned>(preview_.address+0x78)==preview_.pvars&&
        read<unsigned>(preview_.address+0x38)==preview_.stamp;
}
void NativeVendor::before_vendor_update() {
    if(preview_valid()&&read<std::uint16_t>(preview_.address+0xa6)==NativeAmmunition::visual_class)
        write(preview_.address+0xa6,preview_.original_class);
    preview_={};
}
void NativeVendor::after_vendor_update() {
    if(!supported())return;
    if(api_->loaded_level()!=level_||!price_owned_)frame();
    before_vendor_update();
    const unsigned base=level(vendor_menu);
    if(read<int>(level(game_mode))!=5||!valid(base,count_offset+4)||read<int>(base+8)!=1)return;
    const int count=read<int>(base+count_offset),selected=read<int>(base+selection_offset);
    if(count<=0||count>16||selected<0||selected>=count)return;
    const auto row=read<Row>(base+rows_offset+unsigned(selected)*sizeof(Row));
    if(row.item!=item_id||row.is_ammo!=1||read<std::uint16_t>(price_address_+8)==0)return;
    const unsigned list=read<unsigned>(base+0x20);
    if(!valid(list,0x600))return;
    // initialize_vendor_selected_item_mobys creates SIX 0x100-byte menu
    // records. Slot2 is the ammunition hologram, initialized from the +0x30
    // field of the 0x40-byte vendor-slot row. Slot3 is the WEAPON hologram
    // (drawn for an unowned row), and slot1 is the weapon monitor preview.
    const unsigned moby=list+0x200,pclass=read<unsigned>(moby+0x24);
    if(!valid(pclass,0x30)||read<std::uint8_t>(moby+0x20)>=0x80||
       read<std::uint16_t>(moby+0xa6)!=NativeAmmunition::native_pickup_class)return;
    preview_={moby,pclass,read<unsigned>(moby+0x78),read<unsigned>(moby+0x38),
        read<std::uint16_t>(moby+0xa6)};
    write<std::uint16_t>(moby+0xa6,NativeAmmunition::visual_class);
}
void NativeVendor::reset() {
    if(api_&&api_->loaded_level()==level_){
        before_vendor_update();
        restore_menu();
        if(price_owned_&&read<std::int32_t>(price_address_)==normal_price)
            write(price_address_,original_price_);
        if(preview_model_owned_&&read<std::int32_t>(preview_model_address_)==int(applied_preview_class_))
            write(preview_model_address_,original_preview_model_);
    }
    menu_={};level_=-1;price_address_=0;original_price_=0;price_owned_=false;
    preview_={};preview_model_address_=0;original_preview_model_=0;preview_model_owned_=false;
}
}

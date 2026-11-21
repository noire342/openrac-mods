// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_vendor_icons.hpp"
#include "native_vendor.hpp"
#include "hud_table_scope.hpp"
#include "native_arsenal_layout.hpp"
#include <array>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned vendor_menu=0x1ca4c0;
constexpr unsigned row_count=0x210;
constexpr unsigned selected_row=0x58;
constexpr unsigned scroll_offset=0x4c;
constexpr unsigned display_rows=0xd0;
constexpr unsigned display_row_bytes=20;
constexpr unsigned display_row_is_ammo=sizeof(std::int32_t);
constexpr int max_vendor_rows=16;
}

NativeVendorIcons::NativeVendorIcons(const openrac_api* api,NativeVendor& vendor)
    :Memory(api),vendor_(vendor){
    if(!api||!api->function)return;
    rectangle_=reinterpret_cast<Rectangle>(find_function("FUN_001fb8f0"));
    frame_=reinterpret_cast<Frame>(find_function("FUN_001ff960"));
    sprite_=reinterpret_cast<Sprite>(find_function("FUN_001ffc30"));
}
bool NativeVendorIcons::draw(){
    if(!api_||!api_->memory||!api_->game||!api_->version||!api_->loaded_level||
       !api_->relocate_data||!api_->relocate_level_data||!rectangle_||!frame_||!sprite_||
       std::strcmp(api_->game,"rac1")||RegionBindings::for_version(api_->version)!=&bindings())return false;
    const int loaded=api_->loaded_level();
    if(loaded<0||loaded>native_arsenal_layout::last_campaign_level)return false;
    const auto mode=level(native_arsenal_layout::game_mode),menu=level(vendor_menu);
    const auto overlay_hud=level(native_hud::level_state),resident_hud=global(native_hud::resident_state);
    if(!valid(mode,sizeof(std::int32_t))||read<int>(mode)!=native_arsenal_layout::gadgetron_mode||!valid(menu,row_count+sizeof(std::int32_t))||
       !valid(overlay_hud,native_hud::table_end)||!valid(resident_hud,native_hud::table_end))return false;
    const int count=read<int>(menu+row_count),selected=read<int>(menu+selected_row);
    if(count<=0||count>max_vendor_rows||selected<0||selected>=count)return false;
    if (!vendor_.has_custom_rows()) return false;
    const int scroll=read<int>(menu+scroll_offset);
    // Retail moves by at most one slot (56px). Reject corrupt coordinates
    // before a signed subtraction or a native fixed-point draw can overflow.
    if(count>=8&&(scroll<-4096||scroll>4096))return false;
    std::array<std::uint16_t,max_vendor_rows> banks{};
    std::array<int,max_vendor_rows> frames{};
    for(int i=0;i<count;++i){
        const auto bank=vendor_.icon_bank_for_row(unsigned(i));
        if(!bank||bank>native_hud::animation_terminator)return false;
        banks[unsigned(i)]=static_cast<std::uint16_t>(bank);
        const auto row=menu+display_rows+unsigned(i)*display_row_bytes;
        frames[unsigned(i)]=read<int>(row+display_row_is_ammo)==1?native_hud::vendor_ammunition_frame:0;
    }
    // Resolve every logical bank before emitting packets, so a refused pass
    // cannot leave a partially drawn strip for the caller's native fallback.
    HudTableScope tables(*this);
    rectangle_(0,0,512,128,512,128,0);
    const int pulse_phase=int((read<unsigned>(menu+4)&15u)*4u)-32;
    const unsigned pulse=unsigned(pulse_phase<0?-pulse_phase:pulse_phase)+64u;
    const unsigned color=(pulse*0x10202u)|0x80000000u;
    const auto highlight=[&](int x){
        rectangle_(x,2,x+56,58,512,128,color);
        rectangle_(x+2,4,x+54,56,512,128,0x80000000u);
    };
    if(count<8){
        highlight(selected*56+8);
        for(int i=0;i<count;++i)
            sprite_(frame_(banks[unsigned(i)],frames[unsigned(i)]),12+i*56,6,48,48,native_hud::max_alpha);
    }else{
        int offset=scroll;
        if(offset<0)offset+=4;
        else if(offset>0)offset-=4;
        else highlight(176);
        if(offset!=scroll)write(menu+scroll_offset,offset);
        std::int32_t alpha{};
        const std::uint32_t alpha_bits=0x80808080u;
        std::memcpy(&alpha,&alpha_bits,sizeof alpha);
        for(int i=0;i<11;++i){
            const unsigned row=unsigned((2*count+selected-5+i)%count);
            sprite_(frame_(banks[row],frames[row]),offset-100+i*56,6,48,48,alpha);
        }
    }
    return true;
}
}

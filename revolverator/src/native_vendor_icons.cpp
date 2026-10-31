// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_vendor_icons.hpp"
#include "native_vendor.hpp"
#include "hud_table_scope.hpp"
#include <array>
#include <cstring>

namespace revolverator {
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
    if(loaded<0||loaded>18)return false;
    const auto mode=level(0x15f5c4),menu=level(0x1ca4c0);
    const auto overlay_hud=level(0x17e4d8),resident_hud=global(0x19a3e8);
    if(!valid(mode,4)||read<int>(mode)!=5||!valid(menu,0x214)||
       !valid(overlay_hud,0x2c)||!valid(resident_hud,0x2c))return false;
    const int count=read<int>(menu+0x210),selected=read<int>(menu+0x58);
    if(count<=0||count>16||selected<0||selected>=count)return false;
    const int scroll=read<int>(menu+0x4c);
    // Retail moves by at most one slot (56px). Reject corrupt coordinates
    // before a signed subtraction or a native fixed-point draw can overflow.
    if(count>=8&&(scroll<-4096||scroll>4096))return false;
    std::array<std::uint16_t,16> banks{};
    std::array<int,16> frames{};
    for(int i=0;i<count;++i){
        const auto bank=vendor_.icon_bank_for_row(unsigned(i));
        if(!bank||bank>0xffffu)return false;
        banks[unsigned(i)]=static_cast<std::uint16_t>(bank);
        frames[unsigned(i)]=read<int>(menu+0xd0+unsigned(i)*20+4)==1?2:0;
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
            sprite_(frame_(banks[unsigned(i)],frames[unsigned(i)]),12+i*56,6,48,48,128);
    }else{
        int offset=scroll;
        if(offset<0)offset+=4;
        else if(offset>0)offset-=4;
        else highlight(176);
        if(offset!=scroll)write(menu+0x4c,offset);
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

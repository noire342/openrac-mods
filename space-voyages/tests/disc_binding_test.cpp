// SPDX-License-Identifier: GPL-3.0-or-later
#include "disc_binding.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spacevoyages;
namespace {
unsigned checks{};
void require(bool pass,const char* message){++checks;if(!pass){std::fprintf(stderr,"disc_binding: %s\n",message);std::exit(1);}}
}
int main(){
    require(disc::select("rac1","pal")==&disc::pal,"canonical PAL is selected explicitly");
    require(disc::select("rac1","ntsc")==&disc::ntsc_u,"NTSC-U remains selected explicitly");
    for(const auto* version:{"pal_v1","jp","PAL","","ntsc-j"})require(!disc::select("rac1",version),"unknown revisions never inherit another disc's addresses");
    require(!disc::select(nullptr,"pal")&&!disc::select("rac1",nullptr)&&!disc::select("rac2","pal"),"missing identity and other games fail closed");
    const std::array<std::uint32_t,10> pal_expected{0x13e130,0x13de48,0x13de60,0x1d5f70,0x15f6a8,0x15f6e4,0x15eeb4,0x15ee60,0x15ee64,0x15ee6c};
    const std::array<std::uint32_t,10> ntsc_expected{0x13e030,0x13dd40,0x13dd58,0x1d5bf0,0x15f5c4,0x15f600,0x15edb4,0x15ed60,0x15ed64,0x15ed6c};
    for(const auto* selected:{&disc::pal,&disc::ntsc_u}){
        const auto& expected=selected==&disc::pal?pal_expected:ntsc_expected;
        const std::array<disc::NativeAddress,10> fields{selected->shared_ship,selected->coordinates,selected->visits,selected->menu,selected->cinematic_mode,selected->pending_destination,selected->mirror,selected->frame_scale,selected->frame_scale_squared,selected->frame_time};
        for(unsigned field=0;field<fields.size();++field){
            require(fields[field].value==expected[field],"regional field keeps its independently audited source address");
            for(unsigned epoch:{0x100000u,0x200000u}){
                unsigned executable_calls=0,level_calls=0,level_seen=999,address_seen=0;
                const auto executable=[&](std::uint32_t at){++executable_calls;address_seen=at;return at+epoch;};
                const auto level=[&](int which,std::uint32_t at){++level_calls;level_seen=unsigned(which);address_seen=at;return at+epoch+0x400000;};
                const auto result=disc::resolve(fields[field],executable,level);
                const bool overlay=field==4;
                require(address_seen==expected[field]&&executable_calls==unsigned(!overlay)&&level_calls==unsigned(overlay),"each field uses precisely its executable or level-zero relocation service");
                require(result==expected[field]+epoch+(overlay?0x400000:0)&&(!overlay||level_seen==0),"relocation is refreshed after a new overlay rather than caching an old address");
            }
        }
        unsigned executable_calls=0;
        const auto relocated_ship=disc::resolve(selected->shared_ship,[&](std::uint32_t at){++executable_calls;return at+0x500000;},[](int,std::uint32_t){return 0u;});
        require(executable_calls==1&&relocated_ship+disc::ship_variant_offset==selected->shared_ship.value+0x500026,"variant writes derive from the relocated shared record plus the native offset");
    }
    require(std::strcmp(disc::pal.prepare_target,"func_L00_00290030")==0&&std::strcmp(disc::ntsc_u.prepare_target,"FUN_L00_0028ed58")==0,"departure initializer resolves the correct disc's function name");
    require(disc::pal.boarding_sequence.value==0x18cc20&&disc::ntsc_u.boarding_sequence.value==0x18cb20&&disc::pal.boarding_sequence.space==disc::AddressSpace::Executable&&disc::ntsc_u.boarding_sequence.space==disc::AddressSpace::Executable,"boarding playback uses the resident render sequence rather than overlay cutscene scratch data");
    require(std::strcmp(disc::pal.parse_boarding_bank,"func_00205220")==0&&std::strcmp(disc::ntsc_u.parse_boarding_bank,"FUN_002049f0")==0,"boarding seeks every actor and camera through the native regional bank parser");
    require(std::strcmp(disc::pal.fade_to_black,"func_001F4E08")==0&&std::strcmp(disc::ntsc_u.fade_to_black,"FUN_001f4a58")==0,"departure completes the native regional fade before acquiring the temporary scene");
    require(disc::pal.coordinates.value-disc::ntsc_u.coordinates.value==0x108&&disc::pal.menu.value-disc::ntsc_u.menu.value==0x380&&disc::pal.pending_destination.value-disc::ntsc_u.pending_destination.value==0xe4,"independently audited fields cannot be replaced by a guessed uniform region shift");
    require(disc::native_sixty_hz(1.f,1.f,1.f/60.f),"PAL in OpenRAC's native60Hz mode is supported without a region multiplier");
    require(!disc::native_sixty_hz(1.2f,1.44f,.02f),"retail PAL50 timing retains original travel instead of entering the60Hz scene");
    require(!disc::native_sixty_hz(1.f,1.f,.02f)&&!disc::native_sixty_hz(1.2f,1.f,1.f/60.f)&&!disc::native_sixty_hz(1.f,1.44f,1.f/60.f),"inconsistent timing globals fail closed");
    for(float invalid:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})require(!disc::native_sixty_hz(invalid,1.f,1.f/60.f)&&!disc::native_sixty_hz(1.f,invalid,1.f/60.f)&&!disc::native_sixty_hz(1.f,1.f,invalid),"uninitialized or nonfinite timing cannot arm a transient takeover");
    std::printf("Disc binding: %u regional selection, relocation and timing checks passed\n",checks);
}

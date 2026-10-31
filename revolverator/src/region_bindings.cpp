// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
// PALv2 SCES_509.16 / NTSC-U SCUS_971.99. Source audit:
// OpenRAC0.10.0 d015ea1c6925de90b4be91ddb349af83361d9d4b;
// rac1-ntsc/host/rac1_version.h and games/rac1/config/versions/*/functions.tsv.
// Tables are exact canonical keys, not arithmetic region deltas.
#include "region_bindings.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace revolverator {
namespace {
[[noreturn]] void missing(const char* space,unsigned overlay,unsigned address) {
    char text[160];
    std::snprintf(text,sizeof text,"Revolverator PAL has no %s binding for overlay %u address 0x%08x",space,overlay,address);
    throw std::runtime_error(text);
}
struct FunctionName { const char* ntsc; const char* pal; };
constexpr FunctionName functions[] = {
    {"FUN_001efa68","func_001EFE10"},
    {"FUN_001f4280","func_001F4630"},
    {"FUN_001f4398","func_001F4748"},
    {"FUN_001f44b8","func_001F4868"},
    {"FUN_001f5450","func_001F5800"},
    {"FUN_001f9dc8","func_001F9F90"},
    {"FUN_001f9de0","func_001F9FA8"},
    {"FUN_001fa030","func_001FA1F8"},
    {"FUN_001fa580","func_001FA748"},
    {"FUN_001fa610","func_001FA7D8"},
    {"FUN_001fb8f0","func_001FBAB8"},
    {"FUN_001ff288","func_001FFAB8"},
    {"FUN_001ff308","func_001FFB38"},
    {"FUN_001ff960","func_00200198"},
    {"FUN_001ffc30","func_00200468"},
    {"FUN_00200e08","func_00201640"},
    {"FUN_00209298","func_00209CE8"},
    {"FUN_00209370","func_00209DC0"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_002093d8","func_00209E68"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0020abb0","func_0020BA00"},
    {"FUN_0020af20","func_0020BD70"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0020b178","func_0020BFC8"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0020c4f8","func_0020D348"},
    {"FUN_0020c828","func_0020D678"},
    {"FUN_0020cca8","func_0020DAF8"},
    {"FUN_0021a328","func_0021B298"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0021b858","func_0021C840"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0021c7a0","func_0021D7A0"},
    {"FUN_0021c9c8","func_0021D9C8"},
    {"FUN_0021ca60","func_0021DA60"},
    {"FUN_0021d948","func_0021E950"},
    {"FUN_0021ddf8","func_0021EE00"},
    {"FUN_0021df98","func_0021EFA0"},
    {"FUN_0021e110","func_0021F118"},
    {"FUN_0021e230","func_0021F238"},
    {"FUN_0021e608","func_0021F610"},
    {"FUN_0021f158","func_00220160"},
    {"FUN_00220b20","func_00221B58"},
    {"FUN_00224368","func_002255F8"},
    {"FUN_00224b70","func_00225E00"},
    {"FUN_002269c0","func_00227C78"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_00226b08","func_00227DB0"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_0022da68","func_0022ED80"},
    {"FUN_002386e8","func_00239A00"},
    {"FUN_L00_00216f90","func_L00_00217718"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_L00_002223f8","func_L00_00222B80"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_L00_002323b8","func_L00_00232C10"},
    {"FUN_L00_00233db8","func_L00_00234638"},
    {"FUN_L00_00235a70","func_L00_00236400"},
    {"FUN_L00_00235e18","func_L00_002367A8"},
    {"FUN_L00_00237190","func_L00_00237B20"},
    {"FUN_L00_002371e0","func_L00_00237B70"},
    {"FUN_L00_00237200","func_L00_00237B90"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_L00_00237ae8","func_L00_00238478"},
    {"FUN_L00_00237ba0","func_L00_00238530"},
    {"FUN_L00_002382a0","func_L00_00238C30"},
    {"FUN_L00_0023a7a8","func_L00_0023B140"},
    {"FUN_L00_00257218","func_L00_00258250"},
    {"FUN_L00_00259a88","func_L00_0025AAC0"},
    {"FUN_L00_0025f090","func_L00_00260108"},
    {"FUN_L00_00260860","func_L00_002618D8"}, // native grant used by isolated QA fixtures
    {"FUN_L00_0029b680","func_L00_0029C9E8"},
    {"FUN_L00_0029ba78","func_L00_0029CDE0"}, // counterpart changes between discs; preserve the typed call boundary
    {"FUN_L00_002a5dd8","func_L00_002A7078"},
    {"FUN_L00_002c52a8","func_L00_002C6608"},
    {"FUN_L01_002aea70","func_L01_002AFD00"},
};
} // namespace

const RegionBindings& RegionBindings::ntsc() noexcept {
    static constexpr RegionBindings profile(Region::Ntsc);return profile;
}
const RegionBindings& RegionBindings::pal() noexcept {
    static constexpr RegionBindings profile(Region::Pal);return profile;
}
const RegionBindings* RegionBindings::for_version(const char* name) noexcept {
    if(!name)return nullptr;
    if(std::strcmp(name,"ntsc")==0)return &ntsc();
    if(std::strcmp(name,"pal")==0)return &pal();
    return nullptr;
}
const char* RegionBindings::version() const noexcept {return region_==Region::Pal?"pal":"ntsc";}
const char* RegionBindings::card_serial() const noexcept {return region_==Region::Pal?"SCES_509.16":"SCUS_971.99";}
const char* RegionBindings::card_directory() const noexcept {return region_==Region::Pal?"BESCES-50916RATCHET":"BASCUS-97199RATCHET";}
unsigned RegionBindings::animation_transfer_offset() const noexcept {return region_==Region::Pal?0x46u:0x48u;}
float RegionBindings::nominal_hz() const noexcept {return region_==Region::Pal?50.f:60.f;}
const MemcardLayout& RegionBindings::memcard() const noexcept {
    // NTSC: rnc/storage/memory_card/memory_card_state.h. PAL: the independent
    // McCard_209E68/McState_209E68 definitions in src/game/memcard.c. PAL adds
    // used_blocks/request inside the card record; driver fields are not NTSC
    // offsets at a different global base. Keep both complete tables explicit.
    static constexpr MemcardLayout ntsc{
        0xb8,0x00,0x04,0x08,0x0c,0x10,0x14,0x18,0x1c,
        0xac,0xb0,0xb4,
        0xb8,0xbc,0xc0,0xc4,0xc8,0xcc,0xd0,
        0xd4,0xd8,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,0xf4
    };
    static constexpr MemcardLayout pal{
        0xc0,0x00,0x04,0x08,0x0c,0x10,0x14,0x18,0x1c,
        0xb4,0xb8,0xbc,
        0xc0,0xc4,0xc8,0xcc,0xd0,0xd4,0xd8,
        0xdc,0xe0,0xe4,0xe8,0xec,0xf0,0xf4,0xf8,0xfc
    };
    return region_==Region::Pal?pal:ntsc;
}

unsigned RegionBindings::data(unsigned canonical) const {
    if(region_==Region::Ntsc)return canonical;
    switch(canonical) {
    case 0x0013c940u:return 0x0013ca40u;
    case 0x0013d290u:return 0x0013d390u;
    case 0x0013d388u:return 0x0013d490u;
    case 0x0013d408u:return 0x0013d510u;
    case 0x0013d428u:return 0x0013d530u;
    case 0x0013d4c0u:return 0x0013d5c8u;
    case 0x0013d4e8u:return 0x0013d5f0u;
    case 0x0013dd58u:return 0x0013de60u;
    case 0x0013e520u:return 0x0013e620u;
    case 0x0013f350u:return 0x0013f450u;
    case 0x00140408u:return 0x00140508u;
    case 0x00141660u:return 0x00141760u;
    case 0x00141ea0u:return 0x00141fa0u;
    case 0x0014d590u:return 0x0014d690u;
    case 0x0015ed7cu:return 0x0015ee7cu;
    case 0x0015ed80u:return 0x0015ee80u;
    case 0x0015ed84u:return 0x0015ee84u;
    case 0x0015ed88u:return 0x0015ee88u;
    case 0x0015ed98u:return 0x0015ee98u;
    case 0x0015edd0u:return 0x0015eed0u;
    case 0x0015ee20u:return 0x0015ef20u;
    case 0x0015f6a0u:return 0x0015f780u;
    case 0x0015ff00u:return 0x00160000u;
    case 0x0015ff18u:return 0x00160018u;
    case 0x0015ff20u:return 0x00160020u;
    case 0x00160290u:return 0x00160390u;
    case 0x00160294u:return 0x00160394u;
    case 0x001863d0u:return 0x001864d0u;
    case 0x001996fcu:return 0x001997fcu;
    case 0x00199b60u:return 0x00199c60u;
    case 0x0019a3e8u:return 0x0019a4e8u;
    case 0x001a04c0u:return 0x001a05c0u;
    case 0x001b3200u:return 0x001b3580u;
    case 0x001b3580u:return 0x001b3900u;
    case 0x001b3900u:return 0x001b3c80u;
    case 0x001b3ac0u:return 0x001b3e40u;
    case 0x001cf120u:return 0x001cf4a0u;
    case 0x001d5bf0u:return 0x001d5f70u;
    case 0x001df3f0u:return 0x001df770u;
    default:missing("executable data",0,canonical);
    }
}

unsigned RegionBindings::level(unsigned overlay,unsigned canonical) const {
    if(region_==Region::Ntsc)return canonical;
    switch(overlay) {
    case 0:switch(canonical) {
        case 0x0015f5c4u:return 0x0015f6a8u;
        case 0x0015f720u:return 0x0015f800u;
        case 0x0015f724u:return 0x0015f804u;
        case 0x0015f780u:return 0x0015f860u;
        case 0x0015f78cu:return 0x0015f86cu;
        case 0x0015f7c0u:return 0x0015f8a0u;
        case 0x0015f7c4u:return 0x0015f8a4u;
        case 0x0015f7c8u:return 0x0015f8a8u;
        case 0x0015f91cu:return 0x0015f9f4u;
        case 0x0015f92cu:return 0x0015fa04u;
        case 0x0015fa90u:return 0x0015fb48u;
        case 0x00166c80u:return 0x00166d80u;
        case 0x00173e58u:return 0x00173f58u;
        case 0x00173e60u:return 0x00173f60u;
        case 0x00173e80u:return 0x00173f80u;
        case 0x00179ac0u:return 0x00179bc0u;
        case 0x0017dac0u:return 0x0017dbc0u;
        case 0x0017e4d8u:return 0x0017e5d8u;
        case 0x0017e530u:return 0x0017e630u;
        case 0x001b0340u:return 0x001b06c0u;
        case 0x001c2108u:return 0x001c2488u;
        case 0x001c40b0u:return 0x001c43b0u;
        case 0x001c8ff0u:return 0x001c92f0u;
        case 0x001ca4c0u:return 0x001ca7c0u;
        default:break;
        }break;
    case 1:switch(canonical) {
        case 0x0016cce0u:return 0x0016cd60u;
        case 0x001ca4a0u:return 0x001ca720u;
        case 0x001ca940u:return 0x001cabc0u;
        default:break;
        }break;
    default:break;
    }
    missing("level data",overlay,canonical);
}

unsigned RegionBindings::callback(unsigned overlay,unsigned canonical) const {
    if(region_==Region::Ntsc)return canonical;
    switch(overlay) {
    case 0:switch(canonical) {
        case 0x002a5dd8u:return 0x002a7078u;
        default:break;
        }break;
    case 1:switch(canonical) {
        case 0x0024c828u:return 0x0024d130u;
        // HUD channel12 update: neighbouring init/draw entries are24d130/24d1a0.
        // Other identical copies of this tiny function are not this guest entry.
        case 0x0024c878u:return 0x0024d180u;
        case 0x0024c898u:return 0x0024d1a0u;
        case 0x002bb758u:return 0x002bc920u;
        default:break;
        }break;
    case 2:switch(canonical) {
        case 0x002a5d10u:return 0x002a6f00u;
        default:break;
        }break;
    case 3:switch(canonical) {
        case 0x00296298u:return 0x00297450u;
        default:break;
        }break;
    case 4:switch(canonical) {
        case 0x00297fe0u:return 0x002991b0u;
        default:break;
        }break;
    case 5:switch(canonical) {
        case 0x002cfce0u:return 0x002d0f28u;
        default:break;
        }break;
    case 6:switch(canonical) {
        case 0x002b2dc8u:return 0x002b3fe8u;
        default:break;
        }break;
    case 7:switch(canonical) {
        case 0x002ceee8u:return 0x002d00b8u;
        default:break;
        }break;
    case 8:switch(canonical) {
        case 0x002ae8b8u:return 0x002afa80u;
        default:break;
        }break;
    case 9:switch(canonical) {
        case 0x002c3fe8u:return 0x002c5188u;
        default:break;
        }break;
    case 10:switch(canonical) {
        case 0x00296cc0u:return 0x00297e70u;
        default:break;
        }break;
    case 11:switch(canonical) {
        case 0x002c9cc0u:return 0x002cae80u;
        default:break;
        }break;
    case 12:switch(canonical) {
        case 0x002bd798u:return 0x002be968u;
        default:break;
        }break;
    case 13:switch(canonical) {
        case 0x002b2038u:return 0x002b31e8u;
        default:break;
        }break;
    case 14:switch(canonical) {
        case 0x002afe30u:return 0x002b1080u;
        default:break;
        }break;
    case 15:switch(canonical) {
        case 0x00293c18u:return 0x00294df8u;
        default:break;
        }break;
    case 16:switch(canonical) {
        case 0x0029ec40u:return 0x0029fdf8u;
        default:break;
        }break;
    case 17:switch(canonical) {
        case 0x0029df30u:return 0x0029f1f8u;
        default:break;
        }break;
    case 18:switch(canonical) {
        case 0x002a5878u:return 0x002a6a58u;
        default:break;
        }break;
    default:break;
    }
    missing("guest callback",overlay,canonical);
}

const char* RegionBindings::function(const char* name) const noexcept {
    if(!name||region_==Region::Ntsc)return name;
    if(std::strncmp(name,"FUN_",4)!=0)return name;
    for(const auto& entry:functions)if(std::strcmp(name,entry.ntsc)==0)return entry.pal;
    return nullptr;
}

} // namespace revolverator

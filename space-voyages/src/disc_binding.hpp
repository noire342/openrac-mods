// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spacevoyages::disc {
// These are addresses in a named disc program, not deltas between regions.
// SDK relocation maps each program's copy into the currently loaded overlay.
enum class AddressSpace {Executable,LevelZero};
struct NativeAddress {std::uint32_t value;AddressSpace space;};
struct Binding {
    const char* version;
    const char* name;
    NativeAddress shared_ship,coordinates,visits,menu,cinematic_mode,pending_destination,mirror;
    NativeAddress frame_scale,frame_scale_squared,frame_time;
    const char* prepare_target;
    NativeAddress boarding_sequence;
    const char* parse_boarding_bank;
    const char* fade_to_black;
};
inline constexpr NativeAddress executable(std::uint32_t value){return {value,AddressSpace::Executable};}
inline constexpr NativeAddress level_zero(std::uint32_t value){return {value,AddressSpace::LevelZero};}

// Source provenance and the deliberately nonuniform differences are recorded
// in docs/PAL_AUDIT.md. "pal" is OpenRAC's canonical PAL v2 program, not pal_v1.
inline constexpr Binding pal{
    "pal","PAL (SCES-50916, canonical v2)",
    executable(0x0013e130),executable(0x0013de48),executable(0x0013de60),executable(0x001d5f70),
    level_zero(0x0015f6a8),executable(0x0015f6e4),executable(0x0015eeb4),
    executable(0x0015ee60),executable(0x0015ee64),executable(0x0015ee6c),
    "func_L00_00290030",executable(0x0018cc20),"func_00205220","func_001F4E08"
};
inline constexpr Binding ntsc_u{
    "ntsc","NTSC-U (SCUS-97199)",
    executable(0x0013e030),executable(0x0013dd40),executable(0x0013dd58),executable(0x001d5bf0),
    level_zero(0x0015f5c4),executable(0x0015f600),executable(0x0015edb4),
    executable(0x0015ed60),executable(0x0015ed64),executable(0x0015ed6c),
    "FUN_L00_0028ed58",executable(0x0018cb20),"FUN_002049f0","FUN_001f4a58"
};
inline const Binding* select(const char* game,const char* version)noexcept {
    if(!game||!version||std::strcmp(game,"rac1"))return nullptr;
    if(!std::strcmp(version,pal.version))return &pal;
    if(!std::strcmp(version,ntsc_u.version))return &ntsc_u;
    return nullptr;
}
template<class ExecutableRelocation,class LevelRelocation>
std::uint32_t resolve(NativeAddress at,ExecutableRelocation executable_relocation,LevelRelocation level_relocation){
    return at.space==AddressSpace::Executable?executable_relocation(at.value):level_relocation(0,at.value);
}
inline constexpr std::uint32_t ship_variant_offset=0x26;
// OpenRAC runs both supported discs in their own60Hz mode. Never derive the
// active timing from the disc's region or write to the suspended world's clock.
inline bool native_sixty_hz(float scale,float squared,float seconds)noexcept {
    return std::isfinite(scale)&&std::isfinite(squared)&&std::isfinite(seconds)&&
        std::abs(scale-1.f)<.00001f&&std::abs(squared-1.f)<.00001f&&std::abs(seconds-1.f/60.f)<.000001f;
}
}

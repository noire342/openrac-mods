// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstdint>

// Verified source: OpenRAC 2a457b30426ebf01caf40b2831f27ae647d7ad4a.
// Each address is checked through api.function_at before any HUD data is changed.
namespace weaponxp {
struct HudAddresses {
    std::uint32_t relay;
    std::uint32_t weapon;
};

inline constexpr std::array<HudAddresses, 19> kHudPal{{
    {0x002A7078u, 0x0023B140u},  // level 0
    {0x002BC920u, 0x0024FCC0u},  // level 1
    {0x002A6F00u, 0x0023D310u},  // level 2
    {0x00297450u, 0x00229DE8u},  // level 3
    {0x002991B0u, 0x0022E6A0u},  // level 4
    {0x002D0F28u, 0x00264538u},  // level 5
    {0x002B3FE8u, 0x0024A9C0u},  // level 6
    {0x002D00B8u, 0x002632B0u},  // level 7
    {0x002AFA80u, 0x00245898u},  // level 8
    {0x002C5188u, 0x0025AD20u},  // level 9
    {0x00297E70u, 0x0022E728u},  // level 10
    {0x002CAE80u, 0x00261050u},  // level 11
    {0x002BE968u, 0x00253EA0u},  // level 12
    {0x002B31E8u, 0x00246F30u},  // level 13
    {0x002B1080u, 0x00243118u},  // level 14
    {0x00294DF8u, 0x0022B238u},  // level 15
    {0x0029FDF8u, 0x00236BB8u},  // level 16
    {0x0029F1F8u, 0x002364A0u},  // level 17
    {0x002A6A58u, 0x0023DFA8u},  // level 18
}};
inline constexpr std::array<HudAddresses, 19> kHudNtsc{{
    {0x002A5DD8u, 0x0023A7A8u},  // level 0
    {0x002BB758u, 0x0024F3B0u},  // level 1
    {0x002A5D10u, 0x0023C9D8u},  // level 2
    {0x00296298u, 0x002294E8u},  // level 3
    {0x00297FE0u, 0x0022DD88u},  // level 4
    {0x002CFCE0u, 0x00263B48u},  // level 5
    {0x002B2DC8u, 0x0024A058u},  // level 6
    {0x002CEEE8u, 0x00262998u},  // level 7
    {0x002AE8B8u, 0x00244F88u},  // level 8
    {0x002C3FE8u, 0x0025A488u},  // level 9
    {0x00296CC0u, 0x0022DE10u},  // level 10
    {0x002C9CC0u, 0x00260748u},  // level 11
    {0x002BD798u, 0x00253588u},  // level 12
    {0x002B2038u, 0x00246618u},  // level 13
    {0x002AFE30u, 0x00242780u},  // level 14
    {0x00293C18u, 0x0022A910u},  // level 15
    {0x0029EC40u, 0x00236258u},  // level 16
    {0x0029DF30u, 0x00235A90u},  // level 17
    {0x002A5878u, 0x0023D680u},  // level 18
}};
}  // namespace weaponxp

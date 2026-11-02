// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstring>
#include "discs.hpp"
#include "openrac/openrac_mod.h"

namespace weaponxp {
// A cached level number does not retain ownership while the native loader
// reuses level data. This check only reads the SDK's regional mode binding.
inline bool native_level_loading(const openrac_api* api, const Disc& disc) {
    if (!api || !api->memory || !api->relocate_level_data) return true;
    if (api->loaded_level) {
        const int current = api->loaded_level();
        if (current < 0 || current > 18) return true;
    }
    const auto address = api->relocate_level_data(0, disc.mode);
    if (address < 0x10000 || address > 0x2000000 - sizeof(int)) return true;
    int mode{};
    std::memcpy(&mode, api->memory + address, sizeof mode);
    return mode == 6;
}
}

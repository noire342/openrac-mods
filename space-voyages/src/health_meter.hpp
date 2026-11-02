// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace spacevoyages::health_meter {
// Gemlik DrawGemlikTurretHud: palette entries2..255 are progressively
// replaced by opaque black. The initial hp_bar255 leaves254/255 black even
// when the threshold returns to its maximum253. Entries0/1 remain original.
// The shader performs this lookup BEFORE bilinear filtering, like a native
// palette update, so filling the arc does not crop or stretch its artwork.
inline int palette_threshold(float shown,float maximum)noexcept {
    if(!std::isfinite(shown)||!std::isfinite(maximum)||maximum<=0)return 2;
    return std::clamp(int(std::clamp(shown/maximum,0.f,1.f)*251.f)+2,2,253);
}
inline float destruction_black(float progress)noexcept {
    const float t=std::clamp((progress-.45f)/.55f,0.f,1.f);
    return t*t*(3.f-2.f*t);
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <array>

namespace revolverator {
// Draw helpers read both the resident HUD tables and the loaded overlay's
// cached pointers. Appended icon references must address the same pages in both.
class HudTableScope final {
    const Memory& memory_;
    unsigned overlay_{};
    std::array<unsigned,5> before_{},applied_{};
public:
    explicit HudTableScope(const Memory& memory):memory_(memory),overlay_(memory.level(0x17e4d8)) {
        const auto resident=memory.global(0x19a3e8);
        if(!Memory::valid(overlay_+0x18,20)||!Memory::valid(resident+0x18,20)){overlay_=0;return;}
        before_=memory.read<decltype(before_)>(overlay_+0x18);
        applied_=memory.read<decltype(applied_)>(resident+0x18);
        memory.write(overlay_+0x18,applied_);
    }
    ~HudTableScope(){
        if(overlay_)for(unsigned i=0;i<before_.size();++i)
            if(memory_.read<unsigned>(overlay_+0x18+i*4)==applied_[i])
                memory_.write(overlay_+0x18+i*4,before_[i]);
    }
    HudTableScope(const HudTableScope&)=delete;
    HudTableScope& operator=(const HudTableScope&)=delete;
};
}

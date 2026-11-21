// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include "native_hud_layout.hpp"
#include <array>

namespace revolverator {
inline void restore_cached_hud_pointer(const Memory& memory,unsigned offset,unsigned copy,unsigned original) {
    const auto overlay=memory.level(native_hud::level_state);
    if(overlay&&memory.read<unsigned>(overlay+offset)==copy)memory.write(overlay+offset,original);
}

// Draw helpers read both the resident HUD tables and the loaded overlay's
// cached pointers. Appended icon references must address the same pages in both.
class HudTableScope final {
    const Memory& memory_;
    unsigned overlay_{};
    static constexpr unsigned table_bytes=native_hud::table_end-native_hud::bank_header;
    std::array<unsigned,table_bytes/sizeof(unsigned)> before_{},applied_{};
public:
    explicit HudTableScope(const Memory& memory):memory_(memory),overlay_(memory.level(native_hud::level_state)) {
        const auto resident=memory.global(native_hud::resident_state);
        if(!Memory::valid(overlay_,native_hud::table_end)||!Memory::valid(resident,native_hud::table_end)){
            overlay_=0;return;
        }
        before_=memory.read<decltype(before_)>(overlay_+native_hud::bank_header);
        applied_=memory.read<decltype(applied_)>(resident+native_hud::bank_header);
        memory.write(overlay_+native_hud::bank_header,applied_);
    }
    ~HudTableScope(){
        if(overlay_)for(unsigned i=0;i<before_.size();++i){
            const unsigned field=overlay_+native_hud::bank_header+i*unsigned(sizeof(std::uint32_t));
            if(memory_.read<unsigned>(field)==applied_[i])memory_.write(field,before_[i]);
        }
    }
    HudTableScope(const HudTableScope&)=delete;
    HudTableScope& operator=(const HudTableScope&)=delete;
};
}

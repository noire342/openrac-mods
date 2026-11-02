// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"

namespace revolverator {
// The visual owner supplies one event per logical equip. A newly spawned
// native appearance trigger is transferred to that event, including when a
// carrier is reused. This adapter never owns input, clocks or sound banks.
class NativeEquipAudio final:public Memory {
public:
    enum class Result { Unavailable, NativeAppearance, Played, Rejected };
    static constexpr int mechanical_equip_entry=2;
    explicit NativeEquipAudio(const openrac_api* api):Memory(api){}
    Result play(unsigned actor)const;
    // Native pause previews can recreate the same logical weapon. That new
    // actor must not replay a legacy appearance cue without a new equip event.
    bool suppress_native_appearance(unsigned actor)const;
private:
    unsigned validated_class(unsigned actor)const;
};
}

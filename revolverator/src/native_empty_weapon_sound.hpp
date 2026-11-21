// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include "memory.hpp"

namespace revolverator {

class NativeEmptyWeaponSound final : private Memory {
public:
    NativeEmptyWeaponSound(const openrac_api*, openrac_mod*);
    void play(unsigned emitter) const;

private:
    using PlayClassSound = int (*)(int, int, unsigned, int);
    using PlayBankSound = void (*)(int, int, int, int, int, int, int, void*);

    bool play_for_resident_class(unsigned emitter) const;
    void play_from_level_bank() const;

    openrac_mod* mod_;
    PlayClassSound play_class_sound_;
    PlayBankSound play_bank_sound_;
    bool trace_;
};

}

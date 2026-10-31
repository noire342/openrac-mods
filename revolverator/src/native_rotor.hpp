// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include "native_arena.hpp"
#include "rotor_controller.hpp"
#include <array>

namespace revolverator {
class NativeRotor final:public Memory {
public:
    NativeRotor(const openrac_api* api,GuestArena& arena,bool hud_fallback=false,bool preserve_root=false);
    ~NativeRotor();
    void update(unsigned weapon,int ammunition,Phase phase,float charge,float enemy_angle,
                float seconds,bool playing,bool paused);
    void dry_fire(){pending_dry_=true;}
    // Apply again inside the native held-item update, before its render snapshot.
    // It never advances time: idle, capture and enemy synchronization tick once.
    void apply();
    void reset(bool discard=false);
    float angle()const{return controller_.angle();}
    const RotorController& controller()const{return controller_;}
private:
    GuestArena& guest_;
    RotorController controller_;
    unsigned arena_{},owner_{},klass_{},stamp_{},vars_{};
    int level_=-1,arena_level_=-1;
    bool pending_dry_{};
    bool hud_fallback_{};
    bool preserve_root_{};
    Vec3 pivot_{},inverse_bind_offset_{};
    bool attached()const;
    bool attach(unsigned weapon);
    void unlink(unsigned node);
};
}

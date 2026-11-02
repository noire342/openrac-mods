// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "native_rotor.hpp"
#include "native_weapon_alias.hpp"
#include <optional>

namespace revolverator {

// Rendering contexts own separate pose state. A native carrier update is only
// one way of reaching a draw: paused worlds, inventory and acquisition scenes
// must publish the logical weapon without requiring that combat update.
class NativeWeaponVisual final:public Memory {
public:
    enum class Preview { Standalone, Equipped, Acquisition };
    struct HeldState {
        unsigned actor{};
        int ammunition{};
        Phase phase=Phase::Idle;
        float charge{},enemy_angle{};
        Frame frame{};
        bool golden{};
        // Logical inventory selection survives native pause menus temporarily
        // retiring/replacing the held actor. It is not Frame::equipped.
        bool logical_equipped{};
    };
    struct EquipEvent {unsigned actor{};};

    NativeWeaponVisual(const openrac_api*,GuestArena& level_arena,GuestArena& held_arena);
    NativeWeaponVisual(const NativeWeaponVisual&)=delete;
    NativeWeaponVisual& operator=(const NativeWeaponVisual&)=delete;
    void begin_native_frame();
    void restore_native();
    void set_mode(int native_mode);
    void update_held(const HeldState&);
    // Native animation adapters call this after their canonical update. It
    // publishes the current pose; no time or controller state advances here.
    bool publish_held(unsigned actor,bool custom,bool golden);
    bool publish_preview(Preview,unsigned actor,bool custom,bool golden,float fit=1.f);
    void clear_preview(Preview);
    void prepare_native(unsigned actor);
    void reset();
    // One event per validated held-actor/logical-equip transition. A paused
    // transition waits for live gameplay; previews never produce this event.
    std::optional<EquipEvent> poll_equip();
    bool owns(unsigned actor)const{return alias_.owns(actor);}
    bool ready()const{return alias_.ready();}
    const RotorController& controller()const{return held_.controller();}
    EquipAssemblyPose assembly_pose()const{return assembly_.pose();}

    // These are the narrow native adapters consumed by existing arm and Gold
    // shop code. Logical ownership, ammunition, damage, XP and saves never live
    // in this presentation component.
    NativeRotor& held_rotor(){return held_;}
    NativeWeaponAlias& native_alias(){return alias_;}
private:
    NativeWeaponAlias alias_;
    NativeRotor held_,standalone_,equipped_,acquisition_;
    EquipAssembly assembly_;
    struct Identity {
        unsigned actor{},pclass{},pvars{},stamp{};
        bool operator==(const Identity& other)const{
            return actor==other.actor&&pclass==other.pclass&&pvars==other.pvars&&stamp==other.stamp;
        }
    } held_identity_;
    bool equip_pending_{},live_{},logical_equipped_{},pause_continuation_{};
    NativeRotor& preview(Preview);
    Identity identity(unsigned actor)const;
    void clear_held();
};
}

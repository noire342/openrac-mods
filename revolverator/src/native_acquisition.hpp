// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <array>

namespace revolverator {
// Presentation only: uses the original Walloper acquisition scene, without
// granting an item, ammunition, XP or modifying the ordinary/Gold balances.
class NativeAcquisition final:public Memory {
public:
    struct Context {
        void* user{};
        void (*project)(void*){};
        void (*restore)(void*){};
        void (*pose_item)(void*,unsigned){};
    };
    static constexpr unsigned maximum_pending=64;
    NativeAcquisition(const openrac_api*,openrac_mod*,GuestArena&,Context);
    ~NativeAcquisition();
    NativeAcquisition(const NativeAcquisition&)=delete;
    NativeAcquisition& operator=(const NativeAcquisition&)=delete;
    bool request()noexcept;
    // safe: gameplay is ready and no capture/throw owns Ratchet. external_active:
    // an XP/native presentation already owns the scene. Pending requests wait.
    void frame(bool safe,bool external_active);
    bool active()const noexcept{return preparing_||presenting_;}
    bool presenting()const noexcept{return presenting_;}
    bool pending()const noexcept{return requests_!=0;}
    // Filter the raw native pad report BEFORE the original ProcessReport call.
    void filter_report(unsigned char* report,int size)const noexcept;
    // Call before level/arena release. Never touches a different level's RAM.
    void reset()noexcept;
private:
    struct Identity {unsigned address{},pclass{},pvars{},stamp{};};
    using Present=void(*)(unsigned,int);
    using Place=void(*)(unsigned,unsigned,int,int);
    openrac_mod* mod_{};
    GuestArena& arena_;
    Context context_;
    Present present_{};
    Place place_{};
    int level_=-1;
    unsigned buffer_{},hero_{},vendor_{},scene_{},mode_{};
    unsigned requests_{},quiet_{},settled_{};
    float cooldown_seconds_{};
    bool preparing_=false,presenting_=false,projected_=false,hidden_=false,reported_=false;
    Identity body_{};
    unsigned short body_flags_{};
    unsigned char lock_a_{},lock_b_{};
    int previous_item_{};
    std::array<float,4> position_{},rotation_{},velocity_{};
    std::array<int,7> equipment_{},pending_items_{};
    std::array<unsigned char,0x220> vendor_snapshot_{};
    bool supported()const;
    bool bindings_ready();
    bool same_level()const;
    bool body_valid()const;
    bool snapshot_ready()const;
    bool quiet_player()const;
    void begin();
    void hide_player();
    void pose_items();
    void restore_body()noexcept;
    void finish(bool place_player)noexcept;
    void align_scene(const std::array<float,3>& camera);
    void restore_context()noexcept;
};
}

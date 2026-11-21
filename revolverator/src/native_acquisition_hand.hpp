// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <array>

namespace revolverator {

// The acquisition pose has keys that collapse Ratchet's forearm underneath
// the retail glove. Present the full arm on the owned scene actor.
class NativeAcquisitionHand final : private Memory {
public:
    NativeAcquisitionHand(const openrac_api* api, GuestArena& arena) : Memory(api,&arena), arena_(arena) {}
    ~NativeAcquisitionHand(){reset();}
    NativeAcquisitionHand(const NativeAcquisitionHand&)=delete;
    NativeAcquisitionHand& operator=(const NativeAcquisitionHand&)=delete;
    void publish(unsigned actor);
    void restore() noexcept;
    void reset() noexcept;
private:
    static constexpr unsigned frame_capacity=0xc00;
    struct Key {unsigned original{},copy{};};
    GuestArena& arena_;
    std::array<Key,2> keys_{};
    unsigned actor_{}, pclass_{}, variables_{}, stamp_{}, scratch_{};
    int level_=-1;
    bool same_actor() const noexcept;
    bool copy_pose_key(unsigned original,unsigned destination,unsigned joint_count);
};

}

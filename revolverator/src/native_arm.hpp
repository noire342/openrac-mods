// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "arm_math.hpp"
#include "memory.hpp"
#include "native_arena.hpp"

namespace revolverator {
class NativeArm final : public Memory {
public:
    NativeArm(const openrac_api* api,GuestArena& arena);
    ~NativeArm();
    std::optional<Vec3> hold(unsigned owner, unsigned weapon, float yaw, float seconds);
    std::optional<Vec3> align_weapon(unsigned weapon);
    void release();
    void settle(float seconds);
    void reset(bool discard=false);
private:
    using Bone=void(*)(unsigned,int,unsigned);
    using Basis=void(*)(unsigned,unsigned);
    GuestArena& guest_; Bone bone_; Basis basis_;
    unsigned arena_{},owner_{},klass_{},stamp_{};
    unsigned weapon_{},weapon_class_{},weapon_stamp_{};
    float original_radius_{},owned_radius_{};
    int level_=-1,arena_level_=-1;
    float raised_time_{},release_time_{},yaw_{};
    std::array<Quaternion,3> rotations_{{{0,0,0,1},{0,0,0,1},{0,0,0,1}}};
    std::array<Vec3,3> grip_axes_{};bool grip_ready_{};
    using Matrix=std::array<float,16>;
    bool attached() const;
    bool attach(unsigned owner);
    std::optional<Matrix> sample(unsigned joint);
    std::optional<Matrix> parent_frame(unsigned joint);
    bool pose();
    bool aim(unsigned node, unsigned joint, unsigned endpoint, Vec3 target, float weight);
    bool turn_wrist(float weight);
    bool track_weapon(unsigned weapon);
    void unlink(unsigned owner,unsigned node);
};
}

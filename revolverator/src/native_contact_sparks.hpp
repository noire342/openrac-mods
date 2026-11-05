// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "contact_sparks.hpp"
#include "memory.hpp"
namespace revolverator {
class NativeContactSparks final : public Memory {
public:
    explicit NativeContactSparks(const openrac_api*);
    unsigned update(const Frame&,Target,Vec3 contact,Vec3 shaft,float angle,float angular_speed);
    void reset(){rate_.reset();}
    bool available()const{return spawn_!=nullptr;}
    std::uint64_t emitted()const{return emitted_;}
private:
    using Spawn=void(*)(unsigned,unsigned);
    Spawn spawn_{};ContactSparks rate_;std::uint32_t sequence_{};std::uint64_t emitted_{};
};
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>
#include <unordered_map>

#include "progression.hpp"

namespace weaponxp {

struct Award {
    bool changed = false;
    int item = -1;
    int old_level = 1;
    int new_level = 1;
    bool secondary = false;
};

// The combat rules know no addresses, filesystem, renderer or SDK. Game adapters supply events.
class Combat {
public:
    Combat(Progression& state, const Settings& settings) : m_state(state), m_settings(settings) {}

    float hit(
        std::uint32_t target, int item, float damage, std::uint64_t tick, bool secondary = false
    );
    Award death(std::uint32_t target, int reward, std::uint64_t tick);

    void forget(std::uint32_t target) { m_targets.erase(target); }

    void reset() { m_targets.clear(); }

    void expire(std::uint64_t tick);

private:
    struct Target {
        int item;
        std::uint64_t tick;
        bool credited = false;
        bool secondary = false;
    };

    Progression& m_state;
    const Settings& m_settings;
    std::unordered_map<std::uint32_t, Target> m_targets;
};

}  // namespace weaponxp

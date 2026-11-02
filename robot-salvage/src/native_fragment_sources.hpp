// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include <cstdint>
#include <functional>
#include <map>

namespace salvage {

struct FragmentOrigin { std::uint32_t source_class{}, fragment_class{}; };
struct FragmentMotion { Vec3 velocity{}, angular_velocity{}; }; // world units/radians per second

enum class FragmentFactory { FlyingDebris81, PlasmabotDebris325, PathDebris403, BomberJetDebris1510 };
using FlyingDebris81Factory = unsigned (*)(unsigned owner, float angle, unsigned position);
using PlasmabotDebris325Factory = unsigned (*)(unsigned owner, unsigned position);
using PathDebris403Factory = unsigned (*)(unsigned owner, unsigned scale, int mode);
using BomberJetDebris1510Factory = unsigned (*)(unsigned owner, unsigned direction, float scale);
struct FragmentFactoryBinding {
    FragmentFactory factory;
    const char* hook_id;
    const char* symbol;
    const char* signature;
    std::uint32_t fragment_class;
};
const FragmentFactoryBinding* fragment_factory_bindings(std::size_t& count) noexcept;

enum class FragmentObservation { Rejected, Adopted, Pending };

// Boundary for the four source-backed factories, not a blanket CreateMoby hook.
// Call observe AFTER the original factory returns. The source's class is copied
// at that moment; pending pieces never borrow metadata from a recycled owner.
// The adapter binds each observed native allocation to a new local generation.
// reset() MUST run before every native gameplay reload, including same-planet.
// All calls and the adopter run on the game's serialized frame thread.
class NativeFragmentSources final : private NativeMemory {
public:
    using Adopter = std::function<bool(FragmentOrigin, unsigned actor, const FragmentMotion&)>;
    NativeFragmentSources(const openrac_api*, Adopter);

    FragmentObservation observe(FragmentFactory, unsigned owner, unsigned actor, int mode = 1);
    void frame();
    void reset() noexcept;
    std::size_t pending() const noexcept { return pending_.size(); }

private:
    struct Token {
        unsigned address{}, pclass{}, pvars{}, update{}, owner{};
        std::uint64_t generation{}, visit{};
        unsigned short matrix_counter{};
    };
    struct Pending {
        FragmentFactory factory{};
        FragmentOrigin origin{};
        Token token{};
        Vec3 last_position{};
    };
    Adopter adopter_;
    std::map<unsigned, Pending> pending_;
    std::uint64_t generation_{}, visit_ = 1;
    int level_ = -1;

    bool level_ready();
    bool valid_pending(Pending&) const;
    bool motion(const Pending&, FragmentMotion&) const;
    bool due(const Pending&) const;
    bool try_adopt(unsigned actor);
};

} // namespace salvage

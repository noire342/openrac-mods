// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace clank {
struct Vec3 {
    float x{}, y{}, z{};
};
enum class Phase { Attached, Detached };
enum class ReturnReason { Nearby, Death, Scene, LevelChange, Shutdown };
struct Observation {
    std::uint32_t epoch{};
    bool playing{}, menu{}, alive{}, ratchet{}, carrying_clank{}, grounded{};
    Vec3 position{};
};
struct Commands {
    bool detach{}, join{};
};

// Native mutations and ownership belong to the adapter; policy never sees
// addresses, SDK types, platform calls, assets or persistent story flags.
class IWorld {
  public:
    virtual ~IWorld() = default;
    virtual Observation observe() const = 0;
    virtual bool detach() = 0;
    virtual void maintain() = 0;
    virtual bool restore(ReturnReason) = 0;
    virtual void expire() noexcept = 0;
};

class Controller final {
  public:
    explicit Controller(IWorld &world, float join_radius = 2.0f);
    void tick(Commands);
    void before_level_change();
    void stop();
    void out_of_air();
    bool near_ratchet() const;
    Phase phase() const noexcept { return m_phase; }
    Vec3 anchor() const noexcept { return m_anchor; }

  private:
    void restore(ReturnReason);
    IWorld &m_world;
    float m_radius;
    Phase m_phase = Phase::Attached;
    Vec3 m_anchor{};
    std::uint32_t m_epoch{};
};
} // namespace clank

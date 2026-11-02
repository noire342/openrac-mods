// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native_world.hpp"
#include "native_hud.hpp"
#include <array>
namespace clank {
// A complete native controller is present in the executable, but its original
// per-planet variants omit Clank branches. Route an owned Clank through the
// complete implementation; every other actor keeps the original path.
class NativeControl final {
  public:
    NativeControl(const openrac_api &, openrac_mod *, NativeWorld &, NativeHud &);
    ~NativeControl();
    NativeControl(const NativeControl &) = delete;
    NativeControl &operator=(const NativeControl &) = delete;
    bool prepare(int level) noexcept;
    bool clear() noexcept;
    bool active() const { return m_world.controls_clank(); }
    bool active_memory() const noexcept { return m_world.controls_clank_memory(); }
    std::uint32_t relocate(int from, std::uint32_t address) const;
    void *resolve(std::uint32_t address) const;
    bool drawing_air() const noexcept { return m_hud.drawing_air(); }
    int air_frame() const noexcept { return m_hud.air_frame(); }
    bool pending_hud() const noexcept { return m_hud.pending_memory(); }
    bool remap_air_icon(int &id, int &frame) const noexcept {
        return m_hud.remap_air_icon(id, frame);
    }
    void warning_played() noexcept { m_hud.warning_played(); }
    int play_warning(int variant) { return m_hud.play_warning(variant); }
    int state(int requested, int force, bool full);
    void motion();
    void transitions();
    int hit(int argument);
    void capsule();
    void vehicle();
    void terrain();

  private:
    struct Hook {
        void *target{};
        bool created{}, enabled{};
    };
    const openrac_api &m_api;
    openrac_mod *m_mod;
    NativeWorld &m_world;
    NativeHud &m_hud;
    std::array<void *, 5> m_full{};
    std::array<Hook, 12> m_hooks{};
    void *m_vehicle_update{};
    void *m_full_terrain{};
    void *m_level_terrain{};
    void *m_resolver{};
    void *m_function_resolver{};
    void *m_icon_lookup{}, *m_warning{};
    int m_level = -1;
    std::array<bool, 256> m_reported{};
};
} // namespace clank

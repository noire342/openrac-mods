// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "game_profile.hpp"
#include "tether.hpp"
#include "hud_assets.hpp"
#include "alarm_sound.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <openrac/openrac_mod.h>

namespace clank {
// Owns the native radial selector and an electrical variant of the original
// meter. Both keep the native drawing, update and animation services.
class NativeHud final {
  public:
    NativeHud(const openrac_api &, openrac_mod *, const GadgebotIcon &, const AlarmSource &);
    ~NativeHud();
    NativeHud(const NativeHud &) = delete;
    NativeHud &operator=(const NativeHud &) = delete;
    void prepare(int level) noexcept { m_level = level; }
    void *callback(std::uint32_t address) const noexcept;
    void wheel_init(std::uint32_t record);
    int wheel_draw(std::uint32_t record);
    int air_draw(std::uint32_t record);
    bool drawing_air() const noexcept { return m_drawing_air; }
    int air_frame() const noexcept { return m_air_frame; }
    bool remap_air_icon(int &id, int &frame) const noexcept;
    bool pending_memory() const noexcept;
    void warning_played() noexcept;
    int play_warning(int variant);
    void air_update(std::uint32_t record);
    void air(const AirStatus &);
    void clear() noexcept;
    // At process unload the renderer and audio may already be destroyed. The
    // engine owns terminal cleanup; no live-game API can be re-entered here.
    void abandon() noexcept {
        m_air_id = m_wheel_id = -1;
        m_air_record = 0;
        m_alarm.abandon();
        m_assets.abandon();
    }
    void before_level_change() noexcept {
        clear();
        m_alarm.clear();
        m_air_record = 0;
        m_assets.remove();
    }
    std::uint32_t reserve_assets(std::uint32_t cursor) {
        const auto end = m_assets.reserve(cursor);
        m_alarm.bind(m_assets.charge_address() + 16);
        return end;
    }
    float game_seconds() const noexcept;
    float native_air_loss() const noexcept;

  private:
    using Show = int (*)(int, int, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, int);
    using Remove = void (*)(int);
    template <class T> T read(std::uint32_t at) const noexcept {
        T out{};
        if (at >= 0x10000 && at <= 0x02000000 - sizeof(T))
            std::memcpy(&out, m_api.memory + at, sizeof(T));
        return out;
    }
    bool same_epoch(std::uint32_t epoch) const noexcept;
    std::uint32_t epoch() const noexcept;
    void hide_air() noexcept;
    const openrac_api &m_api;
    openrac_mod *m_mod;
    HudAssets m_assets;
    AlarmSound m_alarm;
    Show m_show{};
    Remove m_remove{};
    int (*m_scale_ticks)(int){};
    int (*m_warning)(int, int){};
    std::array<void *, 6> m_functions{};
    int m_level = -1;
    int m_air_id = -1, m_wheel_id = -1;
    // Removal changes the native serial before the slide-out has finished.
    // The private texture id, not that serial, owns the retiring presentation.
    std::uint32_t m_air_record{};
    std::uint32_t m_air_epoch{}, m_wheel_epoch{}, m_wheel_record{};
    int m_current_air = 10000;
    bool m_drawing_air{};
    bool m_warning_played{};
    bool m_was_critical{};
    int m_air_frame{}, m_last_air_frame = -1, m_last_warning_tick = -1;
};
} // namespace clank

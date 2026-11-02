// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller.hpp"
#include "game_profile.hpp"
#include <cstring>
#include <openrac/openrac_mod.h>
#include <vector>
namespace clank {
class NativeWorld final : public IWorld {
  public:
    NativeWorld(const openrac_api &api, openrac_mod *mod, const std::vector<std::uint8_t> &payload)
        : m_api(api), m_mod(mod), m_payload(payload), m_hero(api.relocate_data(hero)) {}
    Observation observe() const override;
    bool detach() override;
    void maintain() override;
    bool restore(ReturnReason) override;
    void expire() noexcept override;
    std::uint32_t reserve_class(std::uint32_t cursor);
    bool controls_clank() const {
        return owned() && read<std::uint8_t>(global(hero) + 0x20A4) == 1;
    }
    bool controls_clank_memory() const noexcept {
        return m_active && read<std::uint8_t>(m_hero + 0x20A4) == 1 &&
               read<std::uint32_t>(m_hero + 0x2080) == m_clank &&
               read<std::uint32_t>(m_hero + 0xA88) == m_ratchet &&
               read<std::uint32_t>(m_clank + 0x24) == m_pclass &&
               read<std::uint16_t>(m_clank + 0xA6) == 87;
    }
    void control_ready(bool ready) noexcept { m_control_ready = ready; }

  private:
    static constexpr std::uint32_t hero = profile::hero;
    template <class T> T read(std::uint32_t at) const {
        T v{};
        if (at >= 0x10000 && at <= 0x02000000 - sizeof(T))
            std::memcpy(&v, m_api.memory + at, sizeof(T));
        return v;
    }
    template <class T> void write(std::uint32_t at, T v) {
        if (at >= 0x10000 && at <= 0x02000000 - sizeof(T))
            std::memcpy(m_api.memory + at, &v, sizeof(T));
    }
    std::uint32_t global(std::uint32_t at) const { return m_api.relocate_data(at); }
    bool owned() const;
    void log(const char *) const;
    void reset_position(const openrac_pose &);
    void *find_exit() const;
    const openrac_api &m_api;
    openrac_mod *m_mod;
    const std::vector<std::uint8_t> &m_payload;
    const std::uint32_t m_hero;
    openrac_pose m_parked{};
    openrac_held m_held{};
    int m_figure = -1, m_clank_slot = -1;
    std::uint32_t m_epoch{}, m_clank{}, m_ratchet{}, m_pclass{};
    std::uint32_t m_blob{}, m_blob_end{};
    int m_blob_level = -1;
    bool m_active{};
    bool m_control_ready{};
    bool m_class_ready{};
};
} // namespace clank

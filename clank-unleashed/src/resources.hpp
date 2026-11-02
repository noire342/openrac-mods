// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hud_assets.hpp"
#include "alarm_sound.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>
namespace clank {
// Owns only immutable source bytes and per-session manifest aliases. Never writes
// the source extraction, disc image, save files, or another mod's cache.
class Resources final {
  public:
    Resources();
    ~Resources();
    Resources(const Resources &) = delete;
    Resources &operator=(const Resources &) = delete;
    const std::vector<std::uint8_t> &payload() const noexcept { return m_payload; }
    const GadgebotIcon &gadgebot_icon() const noexcept { return m_gadgebot_icon; }
    const AlarmSource &alarm_source() const noexcept { return m_alarm_source; }

  private:
    std::vector<std::uint8_t> m_payload;
    GadgebotIcon m_gadgebot_icon;
    AlarmSource m_alarm_source;
    std::filesystem::path m_session;
    std::vector<std::filesystem::path> m_files;
    bool m_installed{};
    bool m_owns_session{};
};
std::vector<std::uint8_t> read_clank_class(const std::filesystem::path &iso);
} // namespace clank

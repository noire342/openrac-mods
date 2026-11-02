// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <openrac/openrac_mod.h>
namespace clank {
struct AlarmSource {
    std::uint32_t sector{}, offset{};
    std::int16_t sound{};
    std::array<std::uint8_t, 32> definition{};
};
AlarmSource read_alarm_source(const std::filesystem::path &iso);
// Owns a lazily loaded native bank and its own definition. No actor, existing
// bank, system output setting or game's sound mapping is modified.
class AlarmSound final {
  public:
    AlarmSound(const openrac_api &, const AlarmSource &);
    ~AlarmSound() { clear(); }
    void bind(std::uint32_t definition) noexcept { m_definition = definition; }
    int play(int flags);
    void clear() noexcept;
    void abandon() noexcept { m_bank = m_definition = 0; }

  private:
    const openrac_api &m_api;
    AlarmSource m_source;
    std::uint32_t m_bank{}, m_definition{};
    std::uint32_t (*m_load)(int, int){};
    void (*m_unload)(int){};
    void (*m_kill)(int){};
    int (*m_allocate)(std::uint32_t, int, std::uint32_t, int, int){};
};
} // namespace clank

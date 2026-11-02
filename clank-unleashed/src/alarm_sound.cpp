// SPDX-License-Identifier: GPL-3.0-or-later
#include "alarm_sound.hpp"
#include "game_profile.hpp"
#include <cstring>
#include <stdexcept>
namespace clank {
AlarmSound::AlarmSound(const openrac_api &api, const AlarmSource &source)
    : m_api(api), m_source(source) {
    m_load = reinterpret_cast<decltype(m_load)>(api.function(profile::alarm_bank_load));
    m_unload = reinterpret_cast<decltype(m_unload)>(api.function(profile::alarm_bank_unload));
    m_kill = reinterpret_cast<decltype(m_kill)>(api.function(profile::alarm_voice_kill));
    m_allocate = reinterpret_cast<decltype(m_allocate)>(api.function(profile::allocate_voice));
    if (!m_load || !m_unload || !m_kill || !m_allocate || !source.sector || source.offset >= 2048 ||
        source.sound < 0)
        throw std::runtime_error("Native energy alarm services or source are unavailable");
}
int AlarmSound::play(int flags) {
    if (m_definition < 0x10000 || m_definition > 0x2000000 - 32)
        return -1;
    if (!m_bank) {
        m_bank = m_load(static_cast<int>(m_source.sector), static_cast<int>(m_source.offset));
        if (!m_bank)
            return -1;
        std::memcpy(m_api.memory + m_definition, m_source.definition.data(), 32);
        std::memcpy(m_api.memory + m_definition + 0x1A, &m_source.sound, sizeof(m_source.sound));
        std::memcpy(m_api.memory + m_definition + 0x1C, &m_bank, sizeof(m_bank));
    }
    return m_allocate(m_definition, flags | 0x11, 0, 0, 0x400);
}
void AlarmSound::clear() noexcept {
    if (m_bank) {
        const auto pool = m_api.relocate_data(profile::voice_pool);
        for (unsigned slot = 0; slot < 30; ++slot) {
            const auto at = pool + (slot + 1) * 0x70 + 8;
            std::uint32_t definition{};
            if (at >= 0x10000 && at <= 0x2000000 - 4)
                std::memcpy(&definition, m_api.memory + at, 4);
            if (definition == m_definition)
                m_kill(static_cast<int>(slot));
        }
        m_unload(static_cast<int>(m_bank));
    }
    m_bank = m_definition = 0;
}
} // namespace clank

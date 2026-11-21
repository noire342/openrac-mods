// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "name_providers.hpp"

#include <algorithm>
#include <cstring>

namespace weaponxp {

int NameProviders::subscribe(weapon_xp_name_provider_v5 provider, void* user,
                             std::uint32_t* token) {
    if (!provider || !token) return WEAPON_XP_INVALID_ARGUMENT;
    if (m_dispatching) return WEAPON_XP_BUSY;
    if (!m_next) return WEAPON_XP_FULL;
    for (const auto& entry : m_entries) {
        if (entry.provider == provider && entry.user == user) return WEAPON_XP_INVALID_ARGUMENT;
    }
    // Append behind surviving registrations, including after a removal.
    const auto slot = std::remove_if(m_entries.begin(), m_entries.end(),
                                     [](const Entry& entry) { return !entry.provider; });
    if (slot == m_entries.end()) return WEAPON_XP_FULL;
    std::fill(slot, m_entries.end(), Entry{});
    *slot = {provider, user, m_next++};
    *token = slot->token;
    ++m_revision;
    return WEAPON_XP_OK;
}

int NameProviders::unsubscribe(std::uint32_t token) {
    if (!token) return WEAPON_XP_INVALID_ARGUMENT;
    for (auto& entry : m_entries) {
        if (entry.token != token) continue;
        entry = {};
        ++m_revision;
        return WEAPON_XP_OK;
    }
    return WEAPON_XP_NOT_FOUND;
}

bool NameProviders::resolve(const weapon_xp_name_context_v5& context, char* out,
                            std::uint32_t capacity) noexcept {
    if (context.size < sizeof(context) || !out || capacity < 2 || m_dispatching) return false;
    struct Dispatch {
        bool& flag;
        explicit Dispatch(bool& busy) : flag(busy) { flag = true; }
        ~Dispatch() { flag = false; }
    } dispatch(m_dispatching);

    std::array<char, WEAPON_XP_NAME_MAX_BYTES> candidate;
    const auto limit = std::min(capacity, WEAPON_XP_NAME_MAX_BYTES);
    for (auto& entry : m_entries) {
        if (!entry.provider) continue;
        const auto token = entry.token;
        candidate.fill('\x7f');
        try {
            if (entry.provider(entry.user, &context, candidate.data(), limit) != 1 ||
                entry.token != token) continue;
        } catch (...) {
            if (entry.token == token) {
                entry = {};
                ++m_revision;
            }
            continue;
        }
        const auto* end = static_cast<const char*>(std::memchr(candidate.data(), 0, limit));
        if (!end || end == candidate.data() ||
            !std::all_of(static_cast<const char*>(candidate.data()), end, [](unsigned char byte) {
                return byte >= ' ' && byte <= '~';
            })) continue;
        std::memcpy(out, candidate.data(), std::size_t(end - candidate.data()) + 1);
        return true;
    }
    return false;
}

}  // namespace weaponxp

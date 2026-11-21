// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "runtime.hpp"

namespace weaponxp {
int Runtime::format_custom_name(const weapon_xp_name_context_v5* context,
                                char* out, std::uint32_t capacity) {
    if (!context || context->size < sizeof(*context) || context->item_id != -1 ||
        !context->default_name || !context->custom_id || context->level < 1 ||
        context->level > kMaxLevel || !out || !capacity) {
        return WEAPON_XP_INVALID_ARGUMENT;
    }
    // Keep custom identities independent of display text and native item IDs.
    constexpr unsigned max_id_bytes = 128;
    unsigned length = 0;
    for (; length < max_id_bytes && context->custom_id[length]; ++length) {
        const auto c = context->custom_id[length];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_')) return WEAPON_XP_INVALID_ARGUMENT;
    }
    if (!length || length == max_id_bytes) return WEAPON_XP_INVALID_ARGUMENT;
    if (!m_settings.versioned_names) return WEAPON_XP_DISABLED;
    if (callback_busy()) return WEAPON_XP_BUSY;
    return m_name_providers.resolve(*context, out, capacity) ? WEAPON_XP_OK : WEAPON_XP_NOT_FOUND;
}
}  // namespace weaponxp

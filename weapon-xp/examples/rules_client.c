/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Copy into an independent native mod after resolving query(WEAPON_XP_ABI_V3).
 * Requires no C++ types, SDK headers, link library or access to game memory.
 * The override lasts for this session. Save the original and restore on unload
 * only if your mod still owns the override; never overwrite a newer consumer's rules.
 */
#include "weapon_xp_api.h"

int weapon_xp_example_set_blaster_xp(const weapon_xp_api_v3* api, uint32_t multiplier) {
    weapon_xp_configuration_v3 config = {0};
    weapon_xp_rules_v3 rule = {0};
    int status;
    if (!api || api->v2.base.abi_version != WEAPON_XP_ABI_V3
        || api->v2.base.size < sizeof(*api)) return WEAPON_XP_INVALID_ARGUMENT;
    config.size = sizeof(config);
    rule.size = sizeof(rule);
    status = api->get_configuration(&config);
    if (status != WEAPON_XP_OK) return status;
    status = api->get_rules(15, &rule);
    if (status != WEAPON_XP_OK) return status;
    rule.xp_multiplier = multiplier;
    return api->set_rules(&rule, 1, config.revision);
}

/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Compile as C11. Resolve query(WEAPON_XP_ABI_V4) in the consuming mod and keep
 * the provider loaded until this consumer has unregistered its callback.
 * Classification only: the consumer owns accepted-hit receipts, awards and UI.
 */
#include "weapon_xp_api.h"
#include <stddef.h>

_Static_assert(sizeof(weapon_xp_hit_context_v4) == 64, "v4 context layout");
_Static_assert(offsetof(weapon_xp_hit_context_v4, tick) == 40, "v4 timestamp offset");
_Static_assert(offsetof(weapon_xp_api_v4, v3) == 0, "v3 prefix must remain unchanged");

typedef struct weapon_xp_example_hit_owner {
    const weapon_xp_api_v4* api;
    uint32_t token;
    int32_t origin_class;
} weapon_xp_example_hit_owner;

static int32_t classify_custom_hit(void* user, const weapon_xp_hit_context_v4* hit) {
    const weapon_xp_example_hit_owner* owner = user;
    if (hit && hit->size >= sizeof(*hit) && hit->origin_class == owner->origin_class)
        return WEAPON_XP_HIT_EXTERNAL;
    return WEAPON_XP_HIT_NATIVE;
}

int weapon_xp_example_register_owner(weapon_xp_example_hit_owner* owner,
                                   const weapon_xp_api_v4* api, int32_t origin_class) {
    int status;
    if (!owner || !api || origin_class < 0 || owner->token
        || api->v3.v2.base.abi_version != WEAPON_XP_ABI_V4
        || api->v3.v2.base.size < offsetof(weapon_xp_api_v4, request_acquisition_presentation))
        return WEAPON_XP_INVALID_ARGUMENT;
    owner->origin_class = origin_class;
    status = api->register_hit_filter(classify_custom_hit, owner, &owner->token);
    if (status == WEAPON_XP_OK) owner->api = api;
    return status;
}

int weapon_xp_example_request_scene(const weapon_xp_api_v4* api, int32_t native_item) {
    if (!api || api->v3.v2.base.abi_version != WEAPON_XP_ABI_V4
        || api->v3.v2.base.size < offsetof(weapon_xp_api_v4, request_acquisition_presentation)
                                     + sizeof(api->request_acquisition_presentation)
        || !api->request_acquisition_presentation) return WEAPON_XP_NOT_READY;
    return api->request_acquisition_presentation(native_item);
}

int weapon_xp_example_unregister_owner(weapon_xp_example_hit_owner* owner) {
    int status;
    if (!owner || !owner->api || !owner->token) return WEAPON_XP_INVALID_ARGUMENT;
    status = owner->api->unregister_hit_filter(owner->token);
    if (status == WEAPON_XP_OK || status == WEAPON_XP_NOT_FOUND) {
        owner->token = 0;
        owner->api = NULL;
    }
    return status;
}

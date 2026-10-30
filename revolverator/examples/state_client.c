/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 noire342 */
#include <stddef.h>
#include <stdio.h>
#include "revolverator_api.h"

#define REV_HAS(service, member) \
    ((service)->size >= offsetof(revolverator_api_v1, member) + sizeof((service)->member))

/* Call only on the game thread. The caller owns discovery and provider lifetime. */
void revolverator_inspect(revolverator_query_fn query, int request_cancel)
{
    const revolverator_api_v1* service = query ? query(1u) : NULL;
    if (!service || service->version != 1u || !REV_HAS(service, cancel) ||
        !service->get_state || !service->cancel)
        return;

    revolverator_state_v1 state = {0};
    state.size = sizeof state;
    if (service->get_state(&state)) {
        printf("item=%u phase=%u charge=%.2f captures=%llu\n",
            state.item_id, state.phase, (double)state.charge,
            (unsigned long long)state.captures);
    }

    if (REV_HAS(service, get_mechanics) && service->get_mechanics) {
        revolverator_mechanics_v1 mechanics = {0};
        mechanics.size = sizeof mechanics;
        if (service->get_mechanics(&mechanics)) {
            printf("ammo=%u/%u rotor=%.2f rad, %.2f rad/s dry=%u sync=%u\n",
                mechanics.ammunition, mechanics.capacity,
                (double)mechanics.rotor_angle, (double)mechanics.rotor_speed,
                mechanics.dry_firing, mechanics.synchronized);
        }
    }
    if (request_cancel)
        (void)service->cancel();
}

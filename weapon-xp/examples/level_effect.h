/* SPDX-License-Identifier: MIT */
#ifndef OPENRAC_WEAPON_XP_LEVEL_EFFECT_EXAMPLE_H
#define OPENRAC_WEAPON_XP_LEVEL_EFFECT_EXAMPLE_H
#include "weapon_xp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Zero-initialize one instance and keep it alive until detach. */
typedef struct blarg_effect_example {
    const weapon_xp_api_v5* xp;
    uint32_t names;
    uint32_t events;
    uint32_t effect_pending;
    float effect_position[3];
} blarg_effect_example;

/* Call on a game frame with a query from an existing Weapon Experience module.
 * Retry NOT_READY on a later frame. NOT_FOUND means the module lacks API v5. */
int32_t blarg_effect_attach(blarg_effect_example* state, weapon_xp_query_fn query);

/* Run outside XP callbacks. spawn supplies the consuming mod's own effect. */
void blarg_effect_tick(blarg_effect_example* state, void (*spawn)(const float position[3]));
void blarg_effect_detach(blarg_effect_example* state);

#ifdef __cplusplus
}
#endif
#endif

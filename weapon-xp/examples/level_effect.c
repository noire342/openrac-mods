/* SPDX-License-Identifier: MIT */
#include "weapon_xp_api.h"
#include <stddef.h>
#include <string.h>

enum { pyrocitor = 16, italian = 5 };

/* One consumer instance. This example coalesces repeated hit observations
 * to the latest position before the consumer's next game-thread tick. */
typedef struct blarg_effect_example {
    const weapon_xp_api_v5* xp;
    uint32_t names, events;
    uint32_t effect_pending;
    float effect_position[3];
} blarg_effect_example;

static int32_t blarg_name(void* user, const weapon_xp_name_context_v5* name,
                          char* out, uint32_t capacity) {
    (void)user;
    if (name->item_id != pyrocitor || name->level != WEAPON_XP_LEVEL_COUNT) return 0;
    const char* replacement = name->language == italian ? "Inceneritore Blarg" : "Pyrocitor Blarg";
    const size_t bytes = strlen(replacement) + 1;
    if (bytes > capacity) return 0;
    memcpy(out, replacement, bytes);
    return 1;
}

static void pyrocitor_event(void* user, const weapon_xp_event_v1* event) {
    blarg_effect_example* state = user;
    if (event->kind == WEAPON_XP_STOPPING) {
        state->xp = NULL;
        state->names = state->events = state->effect_pending = 0;
    } else if (event->kind == WEAPON_XP_HIT && event->item_id == pyrocitor &&
               event->new_level == WEAPON_XP_LEVEL_COUNT) {
        memcpy(state->effect_position, event->target_position, sizeof state->effect_position);
        state->effect_pending = 1;
    } else if (event->kind == WEAPON_XP_LEVEL_LOADED) {
        state->effect_pending = 0;
    }
}

/* Call on a normal game frame. The query came from the already loaded DLL.
 * Retry later if its runtime reports NOT_READY. */
int32_t blarg_effect_attach(blarg_effect_example* state, weapon_xp_query_fn query) {
    if (!state || !query) return WEAPON_XP_INVALID_ARGUMENT;
    if (state->xp) return WEAPON_XP_BUSY;
    const weapon_xp_api_v1* base = query(WEAPON_XP_ABI_V5);
    if (!base || base->abi_version != WEAPON_XP_ABI_V5 ||
        base->size < sizeof(weapon_xp_api_v5)) return WEAPON_XP_NOT_READY;
    const weapon_xp_api_v5* api = (const weapon_xp_api_v5*)base;
    int32_t result = api->register_name_provider(blarg_name, state, &state->names);
    if (result != WEAPON_XP_OK) return result;
    const uint32_t mask = WEAPON_XP_EVENT_MASK(WEAPON_XP_HIT)
        | WEAPON_XP_EVENT_MASK(WEAPON_XP_LEVEL_LOADED) | WEAPON_XP_EVENT_MASK(WEAPON_XP_STOPPING);
    result = api->subscribe_filtered(mask, WEAPON_XP_ANY_ITEM, pyrocitor_event, state, &state->events);
    if (result != WEAPON_XP_OK) {
        api->unregister_name_provider(state->names);
        state->names = 0;
        return result;
    }
    state->xp = api;
    return WEAPON_XP_OK;
}

/* Run outside XP callbacks. Supply the consuming mod's own cosmetic effect. */
void blarg_effect_tick(blarg_effect_example* state, void (*spawn)(const float position[3])) {
    if (state->effect_pending && spawn) {
        state->effect_pending = 0;
        spawn(state->effect_position);
    }
}

void blarg_effect_detach(blarg_effect_example* state) {
    if (state->xp) {
        state->xp->v4.v3.v2.base.unsubscribe(state->events);
        state->xp->unregister_name_provider(state->names);
    }
    memset(state, 0, sizeof *state);
}

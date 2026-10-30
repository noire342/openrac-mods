/* SPDX-License-Identifier: MIT
 * Public C ABI. Copy this header into another mod; no link-time dependency on weapon-xp.
 * All calls and callbacks run on OpenRAC's game thread. Query does not load a mod.
 */
#ifndef OPENRAC_WEAPON_XP_API_H
#define OPENRAC_WEAPON_XP_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WEAPON_XP_ABI_V1 1u
#define WEAPON_XP_QUERY_SYMBOL "openrac_weapon_xp_query"

enum weapon_xp_result {
    WEAPON_XP_OK = 0,
    WEAPON_XP_NOT_READY = -1,
    WEAPON_XP_INVALID_ARGUMENT = -2,
    WEAPON_XP_BUSY = -3,
    WEAPON_XP_FULL = -4,
    WEAPON_XP_NOT_FOUND = -5
};

enum weapon_xp_event_kind {
    WEAPON_XP_HIT = 1,
    WEAPON_XP_CHANGED = 2,
    WEAPON_XP_LEVEL_UP = 3,
    WEAPON_XP_LEVEL_LOADED = 4,
    WEAPON_XP_STOPPING = 5
};

typedef struct weapon_xp_stats_v1 {
    uint32_t size; /* Caller initializes to sizeof(weapon_xp_stats_v1). */
    int32_t item_id;
    const char* name; /* Borrowed, valid while the DLL is loaded. */
    uint32_t level;   /* 1..5 */
    uint32_t xp;      /* Total, 0..1500. */
    uint32_t level_start_xp;
    uint32_t next_level_xp; /* 1500 at V5. */
    float damage_multiplier;
    float ammo_multiplier;
} weapon_xp_stats_v1;

typedef struct weapon_xp_event_v1 {
    uint32_t size;
    uint32_t kind;
    int32_t item_id; /* -1 for level loaded/stopping. */
    uint32_t old_level;
    uint32_t new_level;
    uint32_t xp;
    uint32_t target_moby;     /* Guest address; HIT only, valid only during this callback. */
    float target_position[3]; /* Copied world position; HIT only. */
    float damage;             /* HIT: our outgoing damage, before the target's own reaction. */
} weapon_xp_event_v1;

typedef void (*weapon_xp_listener_v1)(void* user, const weapon_xp_event_v1* event);

typedef struct weapon_xp_api_v1 {
    uint32_t size;
    uint32_t abi_version;
    uint32_t (*weapon_count)(void);
    int32_t (*weapon_id_at)(uint32_t index); /* -1 outside the list. */
    int32_t (*get_stats)(int32_t item, weapon_xp_stats_v1* out);
    /* No inventory unlocks. Capped at V5. Returns BUSY during an event callback. */
    int32_t (*award_xp)(int32_t item, uint32_t amount);
    /* At most 16 listeners. A subscription is owned by its consumer; unsubscribe on unload.
     * Callbacks must not throw. Read/unsubscribe from a callback is allowed; award/subscribe is not.
     */
    int32_t (*subscribe)(weapon_xp_listener_v1 listener, void* user, uint32_t* token);
    int32_t (*unsubscribe)(uint32_t token);
} weapon_xp_api_v1;

typedef const weapon_xp_api_v1* (*weapon_xp_query_fn)(uint32_t abi_version);

#ifdef __cplusplus
}
#endif
#endif

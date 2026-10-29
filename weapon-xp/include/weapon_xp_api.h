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
#define WEAPON_XP_ABI_V2 2u
#define WEAPON_XP_ABI_V3 3u
#define WEAPON_XP_LEVEL_COUNT 10u
#define WEAPON_XP_QUERY_SYMBOL "openrac_weapon_xp_query"

enum weapon_xp_result {
    WEAPON_XP_OK = 0,
    WEAPON_XP_NOT_READY = -1,
    WEAPON_XP_INVALID_ARGUMENT = -2,
    WEAPON_XP_BUSY = -3,
    WEAPON_XP_FULL = -4,
    WEAPON_XP_NOT_FOUND = -5,
    WEAPON_XP_DISABLED = -6,
    WEAPON_XP_STALE_REVISION = -7,
    WEAPON_XP_IO_ERROR = -8
};

enum weapon_xp_event_kind {
    WEAPON_XP_HIT = 1,
    WEAPON_XP_CHANGED = 2,
    WEAPON_XP_LEVEL_UP = 3,
    WEAPON_XP_LEVEL_LOADED = 4,
    WEAPON_XP_STOPPING = 5,
    WEAPON_XP_V5_EFFECT = 6,            /* Reserved legacy event; no longer emitted. */
    WEAPON_XP_CONFIGURATION_CHANGED = 7 /* Query v3 revision/rules; XP has not been edited. */
};

typedef struct weapon_xp_stats_v1 {
    uint32_t size; /* Caller initializes to sizeof(weapon_xp_stats_v1). */
    int32_t item_id;
    const char* name; /* Borrowed, valid while the DLL is loaded. */
    uint32_t level;   /* 1..10 */
    uint32_t xp;      /* Total, 0..46000; use reported thresholds. */
    uint32_t level_start_xp;
    uint32_t next_level_xp; /* Current cap at level 10; never hard-code it. */
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
    uint32_t target_moby; /* Guest address; HIT or V5_EFFECT if known, callback lifetime only. */
    float target_position[3]; /* Copied world position for HIT or effect center for V5_EFFECT. */
    float damage;             /* HIT: our outgoing damage, before the target's own reaction. */
    uint32_t flags; /* Optional tail since 0.2.0: bit 0 = secondary V5 damage. Check size. */
} weapon_xp_event_v1;

#define WEAPON_XP_SECONDARY_HIT 1u
#define WEAPON_XP_EVENT_V1_BASE_SIZE 44u

typedef void (*weapon_xp_listener_v1)(void* user, const weapon_xp_event_v1* event);

typedef struct weapon_xp_api_v1 {
    uint32_t size;
    uint32_t abi_version;
    uint32_t (*weapon_count)(void);
    int32_t (*weapon_id_at)(uint32_t index); /* -1 outside the list. */
    int32_t (*get_stats)(int32_t item, weapon_xp_stats_v1* out);
    /* No inventory unlocks. Capped at level 10. Returns BUSY during an event callback. */
    int32_t (*award_xp)(int32_t item, uint32_t amount);
    /* At most 16 listeners. A subscription is owned by its consumer; unsubscribe on unload.
     * Callbacks must not throw. Read/unsubscribe from a callback is allowed; award/subscribe is not.
     */
    int32_t (*subscribe)(weapon_xp_listener_v1 listener, void* user, uint32_t* token);
    int32_t (*unsubscribe)(uint32_t token);
} weapon_xp_api_v1;

/* Optional co-op/combat adapter. All addresses are local to this process and must
 * never be serialized. session_id namespaces every event; event_id must increase
 * for each prepared local hit. target_id/epoch identify its stable actor lifetime.
 * begin_external_scope(0) clears/disables the adapter. */
typedef struct weapon_xp_external_hit_v2 {
    uint32_t size;
    uint32_t epoch;
    uint64_t session_id, event_id, target_id;
    uint32_t target_moby, hit_address;
    int32_t target_class;
} weapon_xp_external_hit_v2;

typedef struct weapon_xp_prepared_hit_v2 {
    uint32_t size;
    int32_t item_id;
    float damage;
    uint32_t flags; /* WEAPON_XP_SECONDARY_HIT */
} weapon_xp_prepared_hit_v2;

typedef struct weapon_xp_api_v2 {
    weapon_xp_api_v1 base; /* query(2): base.size is sizeof(v2), abi_version is 2. */
    int32_t (*begin_external_scope)(uint64_t session_id);
    /* Existing verified ring hit: apply optional damage scaling once. No XP
     * is awarded until a matching confirmed death. Duplicate event -> same result.
     * This is outgoing hit preparation, not proof the enemy accepted the damage. */
    int32_t (*prepare_local_hit)(const weapon_xp_external_hit_v2*, weapon_xp_prepared_hit_v2*);
    /* Call immediately after posting a replicated hit, BEFORE its native update.
     * Exact target/ring payload is marked to skip this PC's scaling and XP.
     * Native reaction metadata is left intact. The marker expires after 8 ticks;
     * an unchanged stale hit is discarded, never reclassified as a local hit. */
    int32_t (*mark_replicated_hit)(const weapon_xp_external_hit_v2*);
    /* Only for an authenticated owner receipt for a previously prepared event.
     * Repeated confirmation cannot award twice. Applies this PC's xp_rate and
     * normal level-up events/animation. Does not replay local death effects. */
    int32_t (*confirm_external_death)(const weapon_xp_external_hit_v2*);
} weapon_xp_api_v2;

enum weapon_xp_configuration_flags_v3 {
    WEAPON_XP_SCALE_DAMAGE = 1u,
    WEAPON_XP_SCALE_CAPACITY = 2u,
    WEAPON_XP_SHOW_HUD = 4u,
    WEAPON_XP_REDUCED_MOTION = 8u,
    WEAPON_XP_LEVEL_UP_ANIMATION = 16u,
    WEAPON_XP_VERSIONED_NAMES = 32u,
    WEAPON_XP_FLIP_MORPH_MODEL = 64u /* Since 0.6.1; optional held-model orientation. */
};

typedef struct weapon_xp_configuration_v3 {
    uint32_t size;
    uint32_t flags;
    uint64_t revision;
    double xp_rate;       /* .25..4; awards retain quarter-XP precision. */
    uint32_t kill_xp_cap; /* 0..100000, before global and per-weapon multipliers. */
    uint32_t level_count; /* 10; never infer it from an old stats header. */
} weapon_xp_configuration_v3;

typedef struct weapon_xp_rules_v3 {
    uint32_t size;
    int32_t item_id;
    uint32_t level_count;   /* Must equal WEAPON_XP_LEVEL_COUNT. */
    uint32_t xp_multiplier; /* Integer 0..16; 0 pauses awards, keeps previous XP. */
    uint32_t thresholds[WEAPON_XP_LEVEL_COUNT]; /* Strictly ascending, first 0, last <=1e9. */
    float damage[WEAPON_XP_LEVEL_COUNT];        /* Nondecreasing 1..16; level 1 = 1. */
    float ammo[WEAPON_XP_LEVEL_COUNT];          /* Nondecreasing 1..16; level 1 = 1. */
} weapon_xp_rules_v3;

typedef struct weapon_xp_api_v3 {
    weapon_xp_api_v2 v2; /* Unchanged v1/v2 prefixes. base.size = sizeof(v3), abi_version = 3. */
    int32_t (*get_configuration)(weapon_xp_configuration_v3*);
    int32_t (*get_rules)(int32_t item, weapon_xp_rules_v3*);
    /* Session-only mutations, game thread, expected revision must match exactly.
     * No mutation from event callbacks, during acquisition or an active co-op scope.
     * Batch rule edits are atomic. Earned XP is never clamped or erased.
     * Consumer owns its override and restores it on unload using a fresh revision. */
    int32_t (*set_configuration)(const weapon_xp_configuration_v3*, uint64_t expected_revision);
    int32_t (*set_rules)(const weapon_xp_rules_v3*, uint32_t count, uint64_t expected_revision);
    int32_t (*reload_rules)(uint64_t expected_revision); /* Replaces rules from rules_file. */
    int32_t (*flush)(void); /* Atomic XP save now; no game-save-slot writes. */
    int32_t (*last_error)(char* buffer, uint32_t capacity); /* Required bytes incl NUL, or <0. */
} weapon_xp_api_v3;

typedef const weapon_xp_api_v1* (*weapon_xp_query_fn)(uint32_t abi_version);

#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: MIT */
#ifndef OPENRAC_SKILL_POINTS_API_H
#define OPENRAC_SKILL_POINTS_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define OPENRAC_SKILL_POINTS_ABI 1u
#define SP_OK 0
#define SP_NOT_FOUND (-1)
#define SP_INVALID (-2)
#define SP_BUSY (-3)
#define SP_UNAVAILABLE (-4)
#define SP_EVENT_UNLOCKED 1u
#define SP_EVENT_STOPPING 2u

typedef struct sp_achievement {
    uint32_t size, id, points;
    int32_t native_index; /* 0..29 for original skill points; -1 otherwise */
    const char *title, *description, *author;
    uint32_t unlocked, measured_value, measured_target;
    uint64_t unlocked_at; /* Unix seconds; zero for original save imports */
} sp_achievement;

typedef struct sp_totals {
    uint32_t size, count, unlocked, points, earned_points;
} sp_totals;

typedef struct sp_event {
    uint32_t size, type, id;
} sp_event;

typedef void (*sp_listener)(const sp_event*, void*);

typedef struct sp_api {
    uint32_t size, abi;
    int (*totals)(sp_totals*);
    int (*at)(uint32_t index, sp_achievement*);
    int (*get)(uint32_t id, sp_achievement*);
    int (*subscribe)(sp_listener, void*, uint32_t* token);
    int (*unsubscribe)(uint32_t token);
    /* Optional tail since 0.1.2: check size with offsetof before using.
     * The catalogue/totals/get/at above always retain all 178 historical entries. */
    int (*is_enabled)(uint32_t id);   /* 1 active, 0 disabled, or a negative SP_* error */
    int (*active_totals)(sp_totals*); /* Counts/points excluding disabled entries. */
} sp_api;

/* GetProcAddress/GetModuleHandle, or dlsym/RTLD_NOLOAD on a loaded producer.
 * All calls are synchronous on OpenRAC's game thread. Text lasts until STOPPING.
 * Getters and unsubscribe are safe in callbacks; subscribe returns BUSY there.
 * No API method can award an achievement or mutate game/save data. */
typedef const sp_api* (*sp_query_fn)(uint32_t abi);
#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Versioned interop ABI; calls run on the OpenRAC game/load thread.
 * Resolve from an already loaded arsenal-compat provider. Query never loads it.
 */
#ifndef OPENRAC_ARSENAL_HOOKS_API_H
#define OPENRAC_ARSENAL_HOOKS_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define OPENRAC_ARSENAL_HOOKS_ABI_V1 1u
#define OPENRAC_ARSENAL_HOOKS_QUERY "openrac_arsenal_hooks_query"
#define OPENRAC_ARSENAL_REQUIRED_HOOKS 30u

enum arsenal_hooks_result {
    ARSENAL_HOOKS_OK = 0,
    ARSENAL_HOOKS_ARGUMENT = -1,
    ARSENAL_HOOKS_BUILD = -2,
    ARSENAL_HOOKS_UNRESOLVED = -3,
    ARSENAL_HOOKS_UNSAFE_TARGET = -4,
    ARSENAL_HOOKS_CONSUMER = -5,
    ARSENAL_HOOKS_NOT_CLAIMED = -6,
    ARSENAL_HOOKS_UNKNOWN = -7,
    ARSENAL_HOOKS_SIGNATURE = -8,
    ARSENAL_HOOKS_DUPLICATE = -9,
    ARSENAL_HOOKS_BACKEND = -10,
    ARSENAL_HOOKS_INCOMPLETE = -11,
    ARSENAL_HOOKS_COMMITTED = -12,
    ARSENAL_HOOKS_BUSY = -13,
    ARSENAL_HOOKS_NOT_READY = -14,
    ARSENAL_HOOKS_THREAD = -15,
    ARSENAL_HOOKS_ROLLBACK = -16
};

enum arsenal_hooks_stage {
    ARSENAL_HOOKS_COLD = 0,
    ARSENAL_HOOKS_READY = 1,
    ARSENAL_HOOKS_COLLECTING = 2,
    ARSENAL_HOOKS_ACTIVE = 3,
    ARSENAL_HOOKS_FAILED = 4
};

typedef struct arsenal_hooks_diagnostics_v1 {
    uint32_t size;                  /* caller sets sizeof(*out) */
    uint32_t version;
    uint32_t stage;
    uint32_t required;
    uint32_t resolved;
    uint32_t bound;
    uint32_t active;
    int32_t last_status;
    int32_t backend_status;
    char consumer[64];
    char hook_id[96];
    char message[256];
    char executable_sha256[65];
} arsenal_hooks_diagnostics_v1;

typedef struct arsenal_hooks_api_v1 {
    uint32_t size;
    uint32_t version;
    /* Exactly one consumer per transaction. Repeated claims are refused. */
    int32_t (*claim)(const char* consumer_id);
    /* Prepares a DISABLED hook. *original is its callable native trampoline.
     * IDs and signatures are exact; duplicate binds are refused.
     * No hook executes before commit. No C++ exception crosses this ABI. */
    int32_t (*bind)(const char* id, const char* signature,
                    void* replacement, void** original);
    /* Requires all 30 bindings. Activation uses one queued batch. */
    int32_t (*commit)(void);
    /* Disable/remove OWN hooks and release claim; call before consumer unload.
     * Failed rollback retains ownership and reports ARSENAL_HOOKS_ROLLBACK. */
    int32_t (*rollback)(void);
    int32_t (*diagnostics)(arsenal_hooks_diagnostics_v1* out);
    /* Pure ID discovery: 1 for these 30 hooks, 0 for unknown/null. */
    int32_t (*owns_id)(const char* id);
} arsenal_hooks_api_v1;

typedef const arsenal_hooks_api_v1* (*openrac_arsenal_hooks_query_fn)(uint32_t version);

#ifdef __cplusplus
}
#endif
#endif

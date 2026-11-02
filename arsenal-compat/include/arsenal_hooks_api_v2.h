/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Shared native hook service. All methods run on the game/load thread.
 */
#ifndef OPENRAC_ARSENAL_HOOKS_API_V2_H
#define OPENRAC_ARSENAL_HOOKS_API_V2_H
#include "arsenal_hooks_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define OPENRAC_ARSENAL_HOOKS_ABI_V2 2u
typedef uint64_t arsenal_hooks_consumer_v2;
/* Tokens are generation checked. Every original continuation remains valid
 * until its consumer is released, even when another consumer is removed.
 * Each commit activates only that consumer's prepared subset. Last commit is
 * outermost. Mutation during dispatch is refused with ARSENAL_HOOKS_BUSY.
 * Acquire copies consumer_id; bind retains the replacement's module.
 * Consumers must release before unloading and may not use cached original
 * continuations after successful release. No C++ exceptions cross this API. */
typedef struct arsenal_hooks_api_v2 {
 uint32_t size,version;
 int32_t (*acquire)(const char* consumer_id,arsenal_hooks_consumer_v2* out);
 int32_t (*bind)(arsenal_hooks_consumer_v2,const char* id,const char* signature,void* replacement,void** original);
 int32_t (*commit)(arsenal_hooks_consumer_v2);
 int32_t (*release)(arsenal_hooks_consumer_v2);
 int32_t (*owns_id)(const char* id);
 int32_t (*diagnostics)(arsenal_hooks_consumer_v2,arsenal_hooks_diagnostics_v1* out);
} arsenal_hooks_api_v2;
typedef const arsenal_hooks_api_v2* (*openrac_arsenal_hooks_query_v2_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

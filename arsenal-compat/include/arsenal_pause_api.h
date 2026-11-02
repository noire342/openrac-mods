/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Shared pause-root registration, independent of the hook V1/V2 ABI.
 */
#ifndef OPENRAC_ARSENAL_PAUSE_API_H
#define OPENRAC_ARSENAL_PAUSE_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define OPENRAC_ARSENAL_PAUSE_QUERY "openrac_arsenal_pause_query"
#define OPENRAC_ARSENAL_PAUSE_ABI_V1 1u
#define ARSENAL_PAUSE_VISIBLE 1u
#define ARSENAL_PAUSE_CAP_REGISTRY 1u
#define ARSENAL_PAUSE_CAP_PRESENTER_LEASE 2u
#define ARSENAL_PAUSE_CUSTOM_ID_MIN UINT64_C(0x8000000000000000)
typedef uint64_t arsenal_pause_owner_v1;
typedef uint64_t arsenal_pause_entry_token_v1;
typedef uint64_t arsenal_pause_presenter_v1;
enum arsenal_pause_status_v1 {
 ARSENAL_PAUSE_OK=0, ARSENAL_PAUSE_ARGUMENT=-1, ARSENAL_PAUSE_NOT_READY=-2,
 ARSENAL_PAUSE_THREAD=-3, ARSENAL_PAUSE_BUSY=-4, ARSENAL_PAUSE_NOT_FOUND=-5,
 ARSENAL_PAUSE_DUPLICATE=-6, ARSENAL_PAUSE_EXHAUSTED=-7,
 ARSENAL_PAUSE_UNSAFE_CALLBACK=-8, ARSENAL_PAUSE_BACKEND=-9
};
/* IDs are stable application identities, never guest addresses. The high bit
 * separates custom IDs from stock IDs. Labels are copied, bounded UTF-8;
 * callbacks are synchronous on the load/game thread and must not throw.
 * context_id is producer-owned opaque identity, never a game-memory pointer.
 * Retaining callback code does not retain producer state: unregister/release
 * successfully BEFORE destroying that state or unloading the producer. */
typedef struct arsenal_pause_entry_v1 {
 uint32_t size,flags;
 uint64_t id;
 int64_t order;
 char label_en[96],label_it[96];
} arsenal_pause_entry_v1;
typedef int32_t (*arsenal_pause_open_v1)(uint64_t context_id,uint64_t entry_id);
typedef struct arsenal_pause_entry_copy_v1 {
 uint32_t size,reserved;
 arsenal_pause_entry_token_v1 token;
 arsenal_pause_entry_v1 entry;
} arsenal_pause_entry_copy_v1;
typedef struct arsenal_pause_stock_v1 {
 uint32_t size,flags;
 uint64_t id;
 int64_t order;
} arsenal_pause_stock_v1;
typedef struct arsenal_pause_capacity_v1 {
 uint32_t size,max_visible,max_custom,reserved;
} arsenal_pause_capacity_v1;
typedef struct arsenal_pause_state_v1 {
 uint32_t size,registered,visible,presenter_active;
 uint64_t revision,focus;
 uint32_t max_visible,max_custom;
} arsenal_pause_state_v1;
/* One presenter owns native root integration. Producers retain their content
 * pages; the presenter reads copied labels/order and calls activate on entry.
 * This service registers NO SDK callbacks under another mod's identity.
 * Every mutation is atomic and generation checked. Mutations from open()
 * callbacks return BUSY. Snapshots never expose game/C++/producer pointers.
 * An active presenter's capacity is enforced, including visibility changes.
 * Successful release removes all owner's entries and its presenter lease.
 * The presenter must detach borrowed native pages BEFORE relinquishing its
 * lease. A new presenter must acquire a fresh lease and rediscover the root. */
typedef struct arsenal_pause_api_v1 {
 uint32_t size,version,capabilities,max_owners,max_entries;
 int32_t (*acquire)(const char* owner_id,arsenal_pause_owner_v1* out);
 int32_t (*release)(arsenal_pause_owner_v1);
 int32_t (*register_entry)(arsenal_pause_owner_v1,const arsenal_pause_entry_v1*,arsenal_pause_open_v1,uint64_t context_id,arsenal_pause_entry_token_v1* out);
 int32_t (*unregister_entry)(arsenal_pause_owner_v1,arsenal_pause_entry_token_v1);
 int32_t (*update_entry)(arsenal_pause_owner_v1,arsenal_pause_entry_token_v1,const arsenal_pause_entry_v1*);
 int32_t (*entry_count)(uint32_t* count,uint64_t* revision);
 int32_t (*entry_at)(uint32_t index,arsenal_pause_entry_copy_v1* out);
 int32_t (*claim_presenter)(arsenal_pause_owner_v1,const arsenal_pause_capacity_v1*,arsenal_pause_presenter_v1* out);
 int32_t (*release_presenter)(arsenal_pause_owner_v1,arsenal_pause_presenter_v1);
 int32_t (*compose)(arsenal_pause_presenter_v1,const arsenal_pause_stock_v1* stock,uint32_t count);
 int32_t (*set_focus)(arsenal_pause_presenter_v1,uint64_t id);
 /* presenter0 permits read-only observers to inspect the active copied model.
  * capacity0/idsNULL is a size query. Smaller nonzero output capacity returns
  * EXHAUSTED, writes count/state and leaves the output array untouched. */
 int32_t (*snapshot)(arsenal_pause_presenter_v1,arsenal_pause_state_v1* state,uint64_t* ids,uint32_t capacity,uint32_t* count);
 int32_t (*activate)(arsenal_pause_presenter_v1,uint64_t entry_id);
} arsenal_pause_api_v1;
typedef const arsenal_pause_api_v1* (*openrac_arsenal_pause_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

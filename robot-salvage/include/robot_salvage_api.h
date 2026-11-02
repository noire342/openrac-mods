/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Copy-only, game-thread interoperability for Recupero componenti. */
#ifndef ROBOT_SALVAGE_API_H
#define ROBOT_SALVAGE_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ROBOT_SALVAGE_API_VERSION 1u
#define ROBOT_SALVAGE_QUERY "openrac_robot_salvage_query"
enum robot_salvage_status {
 ROBOT_SALVAGE_OK=0,ROBOT_SALVAGE_INVALID=-1,ROBOT_SALVAGE_UNKNOWN_PART=-2,
 ROBOT_SALVAGE_DUPLICATE=-3,ROBOT_SALVAGE_OVERFLOW=-4,ROBOT_SALVAGE_INSUFFICIENT=-5,
 ROBOT_SALVAGE_NOT_FOUND=-6,ROBOT_SALVAGE_STALE=-7,ROBOT_SALVAGE_WRONG_THREAD=-8,
 ROBOT_SALVAGE_NOT_READY=-9,ROBOT_SALVAGE_INTERNAL=-10
};
enum robot_salvage_operation {ROBOT_SALVAGE_ADD=0,ROBOT_SALVAGE_REMOVE=1};
enum robot_salvage_event_kind {
 ROBOT_SALVAGE_COLLECTED=1,ROBOT_SALVAGE_INVENTORY_CHANGED=2,ROBOT_SALVAGE_SPAWNED=3,
 ROBOT_SALVAGE_VISIT_CHANGED=4,ROBOT_SALVAGE_INVENTORY_RESTORED=5,ROBOT_SALVAGE_CATALOG_CHANGED=6
};
typedef struct robot_salvage_vec3_v1 {float x,y,z;} robot_salvage_vec3_v1;
typedef struct robot_salvage_part_v1 {
 uint32_t size;
 uint64_t id;
 char stable_id[96],name_it[128],name_en[128],source_it[128],source_en[128],model_key[96];
 uint32_t source_class,fragment_class;
 float radius,capsule_half_height,mass;
} robot_salvage_part_v1;
typedef struct robot_salvage_inventory_entry_v1 {
 uint32_t size,known;
 uint64_t part,quantity,revision;
} robot_salvage_inventory_entry_v1;
typedef struct robot_salvage_change_v1 {
 uint32_t size,operation;
 uint64_t part,quantity;
} robot_salvage_change_v1;
typedef struct robot_salvage_spawn_v1 {
 uint32_t size;
 uint64_t part;
 robot_salvage_vec3_v1 position,velocity,rotation,angular_velocity;
} robot_salvage_spawn_v1;
typedef struct robot_salvage_fragment_v1 {
 uint32_t size,sleeping,suspended;
 uint64_t instance,part,visit;
 robot_salvage_vec3_v1 position,velocity,rotation,angular_velocity;
} robot_salvage_fragment_v1;
typedef struct robot_salvage_event_v1 {
 uint32_t size,kind;
 uint64_t sequence,part,instance,quantity,total,revision,visit;
} robot_salvage_event_v1;
/* Caller initializes every output/input .size. Strings are copied UTF-8 and
 * NUL-terminated. No API contains a guest address or pointer retained by the mod.
 * Index enumerations are stable within a game-thread call sequence. An event
 * consumer owns its cursor; STALE requires an inventory snapshot and resuming
 * at newest, rather than silently dropping events. Mutation batches are atomic.
 * Spawned models must be available through the native adapter/catalog mapping. */
typedef struct robot_salvage_api_v1 {
 uint32_t size,version;
 int32_t (*catalog_count)(uint64_t* out);
 int32_t (*catalog_entry)(uint64_t index,robot_salvage_part_v1* out);
 int32_t (*get_part)(uint64_t id,robot_salvage_part_v1* out);
 int32_t (*inventory_count)(uint64_t* out,uint64_t* revision);
 int32_t (*inventory_entry)(uint64_t index,robot_salvage_inventory_entry_v1* out);
 int32_t (*apply_changes)(const robot_salvage_change_v1* changes,uint32_t count);
 int32_t (*register_part)(const robot_salvage_part_v1* part,uint64_t* registered_id);
 int32_t (*spawn_part)(const robot_salvage_spawn_v1* spawn,uint64_t* instance);
 int32_t (*world_count)(uint64_t* out,uint64_t* visit);
 int32_t (*world_entry)(uint64_t index,robot_salvage_fragment_v1* out);
 int32_t (*read_events)(uint64_t* cursor,robot_salvage_event_v1* out,uint32_t capacity,uint32_t* count,uint64_t* newest);
} robot_salvage_api_v1;
typedef const robot_salvage_api_v1* (*robot_salvage_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

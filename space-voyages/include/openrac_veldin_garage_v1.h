/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OPENRAC_VELDIN_GARAGE_V1_H
#define OPENRAC_VELDIN_GARAGE_V1_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define VELDIN_GARAGE_API_V1 1u
#define VELDIN_GARAGE_SHIP_ORIGINAL 0u
#define VELDIN_GARAGE_SHIP_GREEN 1u
#define VELDIN_GARAGE_SHIP_RED 2u
#define VELDIN_GARAGE_SHIP_PURPLE 3u
#define VELDIN_GARAGE_SHIP_NONE UINT32_MAX
#define VELDIN_GARAGE_READY 1u
#define VELDIN_GARAGE_ACTIVE 2u
#define VELDIN_GARAGE_TRAVELLING 4u
#define VELDIN_GARAGE_ACTOR_VISIBLE 1u
typedef struct veldin_garage_actor_v1 {
    uint32_t ship_id;
    int32_t native_slot;
    uint32_t native_class;
    uint32_t generation;
    uint32_t flags;
} veldin_garage_actor_v1;
typedef struct veldin_garage_snapshot_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t flags;
    uint32_t owned_mask;
    uint32_t selected_ship;
    uint32_t actor_count;
    uint64_t revision;
    uint32_t generation;
    uint32_t reserved;
    char world_id[48];
    char region[8];
    veldin_garage_actor_v1 actors[4];
} veldin_garage_snapshot_v1;
/* Resolve this named C export from the loaded garage plugin. The call only
 * copies cached state; no game functions or native pointers cross this ABI.
 * Returns1 on a compatible output buffer,0 without writing on invalid args.
 * READY means ownership was verified from this campaign; ACTIVE identifies
 * this logical garage, not either original Veldin. Slots are generation-scoped
 * observations, never durable identities or permission to modify an actor. */
typedef int (*openrac_veldin_garage_query_v1_fn)(veldin_garage_snapshot_v1*,uint32_t);
#ifdef __cplusplus
}
#endif
#endif

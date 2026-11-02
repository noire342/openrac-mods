/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OPENRAC_VELDIN_CAMPAIGN_V1_H
#define OPENRAC_VELDIN_CAMPAIGN_V1_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define VELDIN_CAMPAIGN_API_V1 1u
#define VELDIN_CAMPAIGN_PLANETS_V1 19u
#define VELDIN_CAMPAIGN_READY 1u
#define VELDIN_CAMPAIGN_ACTIVE 2u
#define VELDIN_CAMPAIGN_TRAVELLING 4u

typedef struct veldin_campaign_snapshot_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t flags;
    uint32_t generation;
    uint64_t capture_revision;
    uint32_t source_generation;
    int32_t source_native_level;
    uint8_t coordinates[19];
    uint8_t visits[19];
    uint8_t reserved[2];
    char region[8];
} veldin_campaign_snapshot_v1;

/* Resolve "veldin_garage_query_campaign_v1" from the loaded garage plugin.
 * Returns1 for a compatible cached snapshot, including an unready one.
 * Returns0 on invalid arguments, leaving the supplied bytes untouched.
 * READY means the arrays came from a completely validated canonical PAL
 * checkpoint, not the live RAM of an added world. ACTIVE is positive logical
 * garage identity. generation identifies the current native load; the source
 * generation, level and capture revision continue to identify that checkpoint.
 * coordinates are normalized0/1. visits retain the native raw byte values.
 * No native pointers, game functions or permission to alter progress cross ABI.
 */
typedef int (*veldin_garage_query_campaign_v1_fn)(veldin_campaign_snapshot_v1*, uint32_t);

#ifdef __cplusplus
}
#endif
#endif

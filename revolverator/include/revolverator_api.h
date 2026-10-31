/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (c) 2026 noire342 */
#ifndef REVOLVERATOR_API_H
#define REVOLVERATOR_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum revolverator_phase_v1 { REV_IDLE, REV_WINDUP, REV_SPINNING, REV_FLYING, REV_RECOVERY };
typedef struct revolverator_state_v1 {
 uint32_t size, phase, item_id, target_class;
 float charge;
 uint64_t captures, throws, impacts;
} revolverator_state_v1;
typedef struct revolverator_mechanics_v1 {
 uint32_t size,ammunition,capacity;
 float rotor_angle,rotor_speed;
 uint32_t dry_firing,synchronized;
} revolverator_mechanics_v1;
/* Copy-only game-thread service. No guest pointers cross the public interface. */
typedef struct revolverator_api_v1 {
 uint32_t size, version;
 int (*get_state)(revolverator_state_v1* out);
 int (*cancel)(void);
 /* Optional appended field: check service.size before accessing this member. */
 int (*get_mechanics)(revolverator_mechanics_v1* out);
} revolverator_api_v1;

enum revolverator_weapon_key_v2 {
 REV_NATIVE_WALLOPER_KEY=18,
 REV_REVOLVERATOR_KEY=0x10012
};
typedef struct revolverator_inventory_v2 {
 uint32_t size,key,native_item_id,owned,ammunition,capacity,xp,icon_bank;
 uint32_t normal_price,first_level;
} revolverator_inventory_v2;
typedef struct revolverator_quick_v2 {
 uint32_t size,count,keys[8];
} revolverator_quick_v2;
typedef struct revolverator_progress_v2 {
 uint32_t size,level,xp,level_start_xp,next_level_xp;
 float progress,damage_multiplier,ammo_multiplier;
} revolverator_progress_v2;
typedef struct revolverator_level_up_v2 {
 uint32_t size,old_level,new_level,xp;
} revolverator_level_up_v2;
/* Version 2 preserves the complete v1 prefix. All calls run on the game thread.
 * Native item IDs remain valid only in native APIs; use logical keys here.
 * grant() accepts registered custom keys and returns 1 on success, 0 otherwise.
 * Each caller-owned output must set size before calling. No output contains a
 * pointer into game memory. get_inventory() also accepts native keys 0..36. */
typedef struct revolverator_api_v2 {
 uint32_t size,version;
 int (*get_state)(revolverator_state_v1* out);
 int (*cancel)(void);
 int (*get_mechanics)(revolverator_mechanics_v1* out);
 int (*get_inventory)(uint32_t key,revolverator_inventory_v2* out);
 int (*get_quick)(revolverator_quick_v2* out);
 int (*grant)(uint32_t key,int equip);
 int (*get_progress)(revolverator_progress_v2* out);
 int (*poll_level_up)(revolverator_level_up_v2* out);
} revolverator_api_v2;

/* Version 3 appends copy-only upgrade and flight diagnostics. The logical
 * weapon, ammunition and XP remain the same across the Golden upgrade. */
typedef struct revolverator_upgrade_v3 {
 uint32_t size,golden,normal_price,gold_bolt_price;
 float blast_charge;
 uint32_t release_recoil_active;
 uint64_t golden_explosions,golden_hits;
} revolverator_upgrade_v3;
typedef struct revolverator_api_v3 {
 uint32_t size,version;
 int (*get_state)(revolverator_state_v1* out);
 int (*cancel)(void);
 int (*get_mechanics)(revolverator_mechanics_v1* out);
 int (*get_inventory)(uint32_t key,revolverator_inventory_v2* out);
 int (*get_quick)(revolverator_quick_v2* out);
 int (*grant)(uint32_t key,int equip);
 int (*get_progress)(revolverator_progress_v2* out);
 int (*poll_level_up)(revolverator_level_up_v2* out);
 int (*get_upgrade)(revolverator_upgrade_v3* out);
} revolverator_api_v3;
typedef const revolverator_api_v1* (*revolverator_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 noire342
 * Optional copy-only logical inventory service. No game addresses cross it.
 */
#ifndef OPENRAC_ARSENAL_INVENTORY_API_H
#define OPENRAC_ARSENAL_INVENTORY_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define OPENRAC_ARSENAL_INVENTORY_QUERY "openrac_arsenal_inventory_query"
#define OPENRAC_ARSENAL_INVENTORY_ABI_V1 1u
#define ARSENAL_INVENTORY_START_OWNED 1u
#define ARSENAL_INVENTORY_NO_GOLD 2u
#define ARSENAL_INVENTORY_CURRENCY 4u
#define ARSENAL_INVENTORY_NO_VENDOR 8u
typedef uint64_t arsenal_inventory_token_v1;
enum arsenal_inventory_status_v1 {
 ARSENAL_INVENTORY_OK=0, ARSENAL_INVENTORY_ARGUMENT=-1,
 ARSENAL_INVENTORY_NOT_READY=-2, ARSENAL_INVENTORY_THREAD=-3,
 ARSENAL_INVENTORY_BUSY=-4, ARSENAL_INVENTORY_NOT_FOUND=-5,
 ARSENAL_INVENTORY_DUPLICATE=-6, ARSENAL_INVENTORY_EXHAUSTED=-7,
 ARSENAL_INVENTORY_SEALED=-8, ARSENAL_INVENTORY_CALLBACK=-9
};
typedef struct arsenal_inventory_descriptor_v1 {
 uint32_t size,key,native_item_id,icon_bank,capacity,normal_price,first_level,flags;
 char stable_id[129];
} arsenal_inventory_descriptor_v1;
typedef struct arsenal_inventory_state_v1 {
 uint32_t size,key,native_item_id,owned,ammunition,capacity,xp,golden;
} arsenal_inventory_state_v1;
typedef struct arsenal_inventory_selection_v1 {
 uint32_t size,equipped_key,projected_key,native_item_id,ready,sealed;
 uint32_t quick[8];
} arsenal_inventory_selection_v1;
/* active1 installs this producer's native-carrier view; active0 restores it.
 * commit receives a copied current state and may return an updated copy.
 * selection reports semantic equipment changes, never preview scopes.
 * context_id is opaque producer identity, never a guest-memory pointer.
 * Callbacks run synchronously on the game/load thread and must not throw.
 * Return ARSENAL_INVENTORY_OK on success. Mutations inside callbacks fail BUSY;
 * read-only queries remain available. No retained output contains pointers. */
typedef int32_t (*arsenal_inventory_project_v1)(uint64_t context_id,uint32_t key,uint32_t active);
typedef int32_t (*arsenal_inventory_commit_v1)(uint64_t context_id,uint32_t key,arsenal_inventory_state_v1* state);
typedef int32_t (*arsenal_inventory_selection_changed_v1)(uint64_t context_id,uint32_t key,uint32_t equipped);
/* Copy of the ORIGINAL native carrier, independent of an active custom view.
 * This prevents a custom temporary owned/ammo flag granting the stock weapon. */
typedef int32_t (*arsenal_inventory_native_inventory_v1)(uint64_t context_id,uint32_t native_item_id,arsenal_inventory_state_v1* state);
typedef struct arsenal_inventory_callbacks_v1 {
 uint32_t size,reserved;
 arsenal_inventory_project_v1 project;
 arsenal_inventory_commit_v1 commit;
 arsenal_inventory_selection_changed_v1 selection;
 arsenal_inventory_native_inventory_v1 native_inventory;
} arsenal_inventory_callbacks_v1;
/* Register during mod load, BEFORE the first playable level seals the catalog.
 * Keys >=0x10000 are logical identities; native_item_id is a carrier1..36.
 * Carrier18 remains reserved for the Rotatore adapter. Registrations retain
 * callback modules. Successfully release BEFORE destroying producer state.
 * Release disables ownership and restores its live projection; descriptor
 * tombstones remain stable so native grids and saved identities stay bounded.
 * A failed release retains ownership/code and must not be treated as success. */
typedef struct arsenal_inventory_api_v1 {
 uint32_t size,version;
 int32_t (*register_weapon)(const arsenal_inventory_descriptor_v1*,const arsenal_inventory_callbacks_v1*,uint64_t context_id,arsenal_inventory_token_v1* out);
 int32_t (*release)(arsenal_inventory_token_v1);
 int32_t (*get_inventory)(uint32_t key,arsenal_inventory_state_v1* out);
 int32_t (*get_selection)(arsenal_inventory_selection_v1* out);
 int32_t (*set_owned)(arsenal_inventory_token_v1,uint32_t owned);
 int32_t (*set_state)(arsenal_inventory_token_v1,const arsenal_inventory_state_v1*);
 int32_t (*equip)(uint32_t key);
 int32_t (*assign_quick)(uint32_t slot,uint32_t key);
 /* UTF-8 path of the currently configured asset bundle. capacity0/bufferNULL
  * queries required bytes including NUL. Insufficient capacity returns
  * EXHAUSTED without writing a partial path. No borrowed host pointer escapes. */
 int32_t (*copy_asset_configuration)(char* utf8,uint32_t capacity,uint32_t* required);
} arsenal_inventory_api_v1;
typedef const arsenal_inventory_api_v1* (*openrac_arsenal_inventory_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

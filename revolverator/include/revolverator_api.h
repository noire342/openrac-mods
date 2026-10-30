/* SPDX-License-Identifier: MIT */
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
typedef const revolverator_api_v1* (*revolverator_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

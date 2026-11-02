/* SPDX-License-Identifier: MIT */
#ifndef OPENRAC_SPACE_VOYAGES_API_H
#define OPENRAC_SPACE_VOYAGES_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct space_voyages_state_v1 {
 uint32_t size,phase; /* 0 ground,1 departure,2 freeflight,3 approach,4 arrival */
 int32_t origin,destination,candidate;
 uint32_t candidate_unlocked,paused;
 float position[3],speed;
} space_voyages_state_v1;
typedef void (*space_voyages_listener_v1)(void* user,const space_voyages_state_v1* state);
typedef struct space_voyages_api_v1 {
 uint32_t size,version;
 int (*get_state)(space_voyages_state_v1* out);
 int (*subscribe)(space_voyages_listener_v1,void* user,uint32_t* token);
 int (*unsubscribe)(uint32_t token);
} space_voyages_api_v1;
typedef const space_voyages_api_v1* (*space_voyages_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif

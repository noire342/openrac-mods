/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Test process only. This is not a provider/plugin and does not access a game. */
#include "../include/openrac_veldin_garage_v1.h"
#include "../include/openrac_veldin_campaign_v1.h"
#include <string.h>
#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
static veldin_garage_snapshot_v1 observation;
static uint32_t queries;
static veldin_campaign_snapshot_v1 campaign;
FIXTURE_EXPORT int garage_fixture_campaign(const veldin_campaign_snapshot_v1* value,uint32_t bytes) {
    if(!value||bytes!=sizeof(campaign))return 0;
    memcpy(&campaign,value,sizeof(campaign));return 1;
}
FIXTURE_EXPORT int veldin_garage_query_campaign_v1(veldin_campaign_snapshot_v1* out,uint32_t bytes) {
    if(!out||bytes<sizeof(campaign))return 0;
    memcpy(out,&campaign,sizeof(campaign));return 1;
}
FIXTURE_EXPORT int garage_fixture_snapshot(const veldin_garage_snapshot_v1* value,uint32_t bytes) {
    if(!value||bytes!=sizeof(observation))return 0;
    memcpy(&observation,value,sizeof(observation));return 1;
}
FIXTURE_EXPORT uint32_t garage_fixture_queries(void) {return queries;}
FIXTURE_EXPORT int openrac_veldin_garage_query_v1(veldin_garage_snapshot_v1* out,uint32_t bytes) {
    if(!out||bytes<sizeof(observation))return 0;
    ++queries;memcpy(out,&observation,sizeof(observation));return 1;
}

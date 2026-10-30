/* SPDX-License-Identifier: MIT
 * Example consumer: no link-time dependency, no second producer instance. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "skill_points_api.h"

#include <openrac/openrac_mod.h>
static const openrac_api* host;
static openrac_mod* handle;
static const sp_api* points;
static uint32_t subscription;

static void changed(const sp_event* event, void* user) {
    (void)user;
    if (event->type == SP_EVENT_STOPPING) {
        points = NULL;
        subscription = 0;
        host->log(handle, OPENRAC_LOG_INFO, "observer: producer stopping");
    } else if (event->type == SP_EVENT_UNLOCKED) {
        sp_achievement item = {0};
        item.size = sizeof(item);
        if (points->get(event->id, &item) == SP_OK) {
            host->log(handle, OPENRAC_LOG_INFO, "observer: achievement unlocked");
            host->log(handle, OPENRAC_LOG_INFO, item.title);
        }
    }
}

static void tick(void* user) {
    HMODULE module;
    sp_query_fn query;
    (void)user;
    if (points) {
        return;
    }
    module = GetModuleHandleW(L"skill_points_ra.dll");
    if (!module) {
        return;
    }
    query = (sp_query_fn)GetProcAddress(module, "openrac_skill_points_query");
    if (!query) {
        return;
    }
    points = query(OPENRAC_SKILL_POINTS_ABI);
    if (!points) {
        return;
    }
    if (points->subscribe(changed, NULL, &subscription) != SP_OK) {
        points = NULL;
    }
}
OPENRAC_MOD_DEFINE("skill-points-observer", "0.1.0")

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    host = api;
    handle = mod;
    return api->on_frame(mod, tick, NULL);
}

OPENRAC_MOD_EXPORT void openrac_mod_unload(void) {
    if (points && subscription) {
        points->unsubscribe(subscription);
    }
    points = NULL;
    subscription = 0;
}

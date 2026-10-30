/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * A small independent consumer mod. Windows discovery example; it never loads a second provider.
 * Other platforms can resolve WEAPON_XP_QUERY_SYMBOL on their loader's existing module handle.
 */
#include "openrac/openrac_mod.h"
#include "weapon_xp_api.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const openrac_api* host;
static openrac_mod* self;
static const weapon_xp_api_v1* xp;
static uint32_t subscription;

static void on_event(void* user, const weapon_xp_event_v1* event) {
    (void)user;
    if (!event || event->size < WEAPON_XP_EVENT_V1_BASE_SIZE) { return; }
    if (event->kind == WEAPON_XP_LEVEL_UP) {
        host->log(self, OPENRAC_LOG_INFO, "observer: weapon upgraded");
    } else if (event->kind == WEAPON_XP_STOPPING) {
        /* The producer is stopping first: never call into it after this notification. */
        xp = NULL;
        subscription = 0;
    }
}

OPENRAC_MOD_DEFINE("xp-observer", "0.1.0")

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    host = api;
    self = mod;
    xp = NULL;
    subscription = 0;
#if defined(_WIN32)
    HMODULE module = GetModuleHandleW(L"weapon_xp.dll");
    weapon_xp_query_fn query =
        module == NULL ? NULL
                       : (weapon_xp_query_fn)(void*)GetProcAddress(module, WEAPON_XP_QUERY_SYMBOL);
    if (query != NULL) {
        xp = query(WEAPON_XP_ABI_V1);
    }
#endif
    if (xp == NULL || xp->size < sizeof(*xp) || xp->abi_version != WEAPON_XP_ABI_V1
        || xp->subscribe(on_event, NULL, &subscription) != WEAPON_XP_OK) {
        api->fail(
            mod, "load weapon-xp before xp-observer; use platform loader discovery outside Windows"
        );
        xp = NULL;
        return 1;
    }
    return 0;
}

OPENRAC_MOD_EXPORT void openrac_mod_unload(void) {
    if (xp != NULL && subscription != 0) {
        xp->unsubscribe(subscription);
    }
    subscription = 0;
    xp = NULL;
}

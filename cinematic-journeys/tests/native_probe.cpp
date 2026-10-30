// SPDX-License-Identifier: GPL-3.0-or-later
// Test fixture only: requests one real transition through the game's own entry point.
#include <cstring>

#include <openrac/openrac_mod.h>

namespace {
const openrac_api* api;
openrac_mod* mod;
unsigned playing = 0;
bool requested = false;
void (*travel)(int) = nullptr;

void frame(void*) {
    if (requested || api->loaded_level() != 2) {
        return;
    }
    int mode = -1;
    std::memcpy(&mode, api->memory + api->relocate_level_data(0, 0x15f5c4), 4);
    if (mode != 0) {
        playing = 0;
        return;
    }
    if (++playing < 90) {
        return;
    }
    requested = true;
    api->log(
        mod,
        OPENRAC_LOG_INFO,
        "QA FIXTURE: requesting native planet 2 -> 3 transition; not manual map interaction"
    );
    travel(3);
}
}  // namespace

extern "C" {
OPENRAC_MOD_DEFINE("cinematic-qa", "0.1.0")

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* services, openrac_mod* handle) {
    api = services;
    mod = handle;
    travel = reinterpret_cast<void (*)(int)>(api->function("InitializeGlobalStateEntry"));
    if (!travel) {
        api->fail(mod, "QA: native transition entry missing");
        return 1;
    }
    return api->on_frame(mod, frame, nullptr);
}
}

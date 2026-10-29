// SPDX-License-Identifier: GPL-3.0-or-later
#include <exception>
#include <memory>

#include "runtime.hpp"

namespace {
std::unique_ptr<weaponxp::Runtime> session;
}

extern "C" {
OPENRAC_MOD_DEFINE("weapon-xp", WEAPON_XP_MOD_VERSION)
}

extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    session.reset();
    try {
        auto next = std::make_unique<weaponxp::Runtime>(api, mod);
        next->load();
        session = std::move(next);
        return 0;
    } catch (const std::exception& error) {
        api->fail(mod, error.what());
        return 1;
    }
}

extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(void) {
    session.reset();
}

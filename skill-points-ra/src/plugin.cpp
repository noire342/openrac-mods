// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "runtime.hpp"
#include "service.hpp"

namespace skillpoints {
using Loader = std::uint32_t (*)(std::int32_t);
Loader original_loader = nullptr;

static void frame_callback(void*) noexcept {
    if (active) {
        try {
            active->tick();
        } catch (const std::exception& e) {
            active->log(OPENRAC_LOG_ERROR, e.what());
        }
    }
}

static std::uint32_t relay(std::int32_t value) noexcept {
    if (active) {
        const auto address = static_cast<std::uint32_t>(value);
        if (active->ui.owns(address)) {
            if (!active->ui.handles(address)) {
                return 1;  // an inactive owned panel never becomes a level load
            }
            try {
                return active->ui.invoke(address);
            } catch (const std::exception& e) {
                active->log(OPENRAC_LOG_ERROR, e.what());
                return 0;
            }
        }
        active->ui.restore();
        active->flush();
    }
    return original_loader ? original_loader(value) : 0;
}

}  // namespace skillpoints

namespace {
std::unique_ptr<skillpoints::Runtime> session;
}

extern "C" {
OPENRAC_MOD_DEFINE("skill-points-ra", "0.1.3")
}

extern "C" OPENRAC_MOD_EXPORT const sp_api* openrac_skill_points_query(std::uint32_t abi) {
    return abi == OPENRAC_SKILL_POINTS_ABI && skillpoints::active ? &skillpoints::service : nullptr;
}

extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    session.reset();
    skillpoints::original_loader = nullptr;
    // Check the prefix we use, not sizeof the SDK used to compile this DLL.
    // Newer SDKs append optional services; API 1.1 clients remain valid.
    if (!api || api->size < offsetof(openrac_api, stack_pop) + sizeof(api->stack_pop)) {
        return 1;
    }
    try {
        if (api->api_major != 1 || !api->game || !api->version || std::strcmp(api->game, "rac1")
            || std::strcmp(api->version, "ntsc")) {
            throw std::runtime_error("This achievement set requires Ratchet & Clank NTSC-U");
        }
        if (!api->memory || !api->function || !api->function_at || !api->relocate_data
            || !api->loaded_level || !api->stack_push || !api->stack_pop || !api->log || !api->fail
            || !api->setting_bool || !api->setting_string || !api->hook || !api->on_frame
            || api->api_minor < 1) {
            throw std::runtime_error("Incomplete OpenRAC mod API");
        }
        auto next = std::make_unique<skillpoints::Runtime>(api, mod);
        next->policy.set_enabled(
            skillpoints::kKerwanTimeTrial, api->setting_bool(mod, "kerwan_time_trial") != 0
        );
        const auto* profile = api->setting_string(mod, "profile");
        next->store = std::make_unique<skillpoints::FileStore>(
            skillpoints::profile_path(profile ? profile : ""), next->ledger
        );
        const int result = api->hook(
            mod,
            "rac1.level.load_gameplay",
            "u32(i32)",
            reinterpret_cast<void*>(skillpoints::relay),
            reinterpret_cast<void**>(&skillpoints::original_loader)
        );
        if (result != OPENRAC_HOOK_OK || !skillpoints::original_loader) {
            throw std::runtime_error(
                "Cannot register level/UI callback u32(i32): hook result " + std::to_string(result)
            );
        }
        if (api->on_frame(mod, skillpoints::frame_callback, nullptr) != 0) {
            throw std::runtime_error("Cannot register achievement frame callback");
        }
        skillpoints::active = next.get();
        next->log(
            OPENRAC_LOG_INFO,
            "178 official RetroAchievements rules loaded; 30 original skill points + 148 new; "
            "offline profile ready; active="
                + std::to_string(next->policy.count())
        );
        session = std::move(next);
        return 0;
    } catch (const std::exception& e) {
        skillpoints::original_loader = nullptr;
        if (api->fail) {
            api->fail(mod, e.what());
        }
        return 1;
    }
}

extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    session.reset();
    skillpoints::original_loader = nullptr;
}

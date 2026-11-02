// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller.hpp"
#include "input.hpp"
#include "native_world.hpp"
#include "native_control.hpp"
#include "native_hud.hpp"
#include "tether.hpp"
#include "resources.hpp"
#include <cstddef>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
namespace {
struct Session {
    clank::Resources resources;
    clank::NativeWorld world;
    clank::NativeHud hud;
    clank::NativeControl native_control;
    clank::Controller controller;
    clank::Input input;
    clank::Tether tether;
    const openrac_api &api;
    openrac_mod *mod;
    Session(const openrac_api &a, openrac_mod *m)
        : world(a, m, resources.payload()),
          hud(a, m, resources.gadgebot_icon(), resources.alarm_source()),
          native_control(a, m, world, hud), controller(world),
          tether({a.setting_bool(m, "distance_oxygen") != 0,
                  static_cast<float>(a.setting_float(m, "maximum_distance")),
                  static_cast<float>(a.setting_float(m, "oxygen_seconds"))}),
          api(a), mod(m) {}
    void tick() {
        controller.tick(input.take());
        const bool detached = controller.phase() == clank::Phase::Detached;
        const auto state = world.observe();
        const auto anchor = controller.anchor();
        const float distance = std::hypot(state.position.x - anchor.x, state.position.y - anchor.y,
                                          state.position.z - anchor.z);
        const auto air = tether.update(detached, state.playing, distance, hud.game_seconds(),
                                       hud.native_air_loss());
        if (air.exhausted) {
            hud.clear();
            controller.out_of_air();
            tether.reset();
            api.log(mod, OPENRAC_LOG_INFO,
                    "Clank exhausted his distance energy; native checkpoint requested");
        } else if (detached)
            hud.air(air);
        else
            hud.clear();
    }
    void stop() noexcept {
        try {
            controller.stop();
            world.restore(clank::ReturnReason::Shutdown);
            hud.clear();
        } catch (...) {
            api.log(mod, OPENRAC_LOG_ERROR, "Could not restore Clank control during cleanup");
        }
    }
    ~Session() {
        // OpenRAC's terminal unload can run after graphics/gameplay services
        // have been destroyed. Restore during gameplay, never during atexit.
        hud.abandon();
        world.expire();
    }
};
std::unique_ptr<Session> session;
using Loader = std::uint32_t (*)(std::int32_t);
Loader original{};
void frame(void *) noexcept {
    if (!session)
        return;
    try {
        session->tick();
    } catch (const std::exception &e) {
        session->api.log(session->mod, OPENRAC_LOG_ERROR, e.what());
        session->stop();
    } catch (...) {
        session->api.log(session->mod, OPENRAC_LOG_ERROR, "Clank frame callback failed");
        session->stop();
    }
}
void pad(void *, int port, std::uint8_t *report, int size) noexcept {
    if (!session)
        return;
    session->input.poll(port, report, size, session->controller.phase() == clank::Phase::Detached,
                        session->controller.near_ratchet());
}
std::uint32_t load_gameplay(std::int32_t mode) {
    // Skill Points may route native menu callbacks through this same ABI.
    // They are guest object addresses, never the loader's 0/1 flag.
    if (!session || (mode != 0 && mode != 1))
        return original ? original(mode) : 0;
    session->controller.before_level_change();
    session->hud.before_level_change();
    session->tether.reset();
    session->native_control.clear();
    session->world.expire();
    const auto result = original(mode);
    auto cursor = session->world.reserve_class(result);
    try {
        cursor = session->hud.reserve_assets(cursor);
    } catch (const std::exception &e) {
        session->world.control_ready(false);
        session->api.log(session->mod, OPENRAC_LOG_ERROR, e.what());
        return cursor;
    }
    if (!session->native_control.prepare(session->api.loaded_level()))
        session->api.log(session->mod, OPENRAC_LOG_ERROR,
                         "Clank control unavailable on this level; detachment disabled");
    return cursor;
}
} // namespace
extern "C" {
OPENRAC_MOD_DEFINE("clank-unleashed", clank::profile::version)
OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api *api, openrac_mod *mod) {
    if (!api || api->size < offsetof(openrac_api, figure_held) + sizeof(api->figure_held))
        return 1;
    try {
        if (api->api_major != 1 || api->api_minor < 9 || !api->game ||
            std::string(api->game) != "rac1" || !api->version ||
            std::string(api->version) != clank::profile::region || !api->memory || !api->function ||
            !api->function_at || !api->loaded_level || !api->relocate_data ||
            !api->relocate_level_data || !api->play_status || !api->hero_pose || !api->hero_held ||
            !api->figure_create || !api->figure_pose || !api->figure_held || !api->figure_remove ||
            !api->moby_spawn || !api->moby_kill || !api->hook || !api->on_pad || !api->on_frame ||
            !api->stack_push || !api->stack_pop || !api->log || !api->fail || !api->setting_bool ||
            !api->setting_float)
            throw std::runtime_error("Clank requires the PAL OpenRAC API 1.9 services");
        session = std::make_unique<Session>(*api, mod);
        if (api->hook(mod, "rac1.level.load_gameplay", "u32(i32)",
                      reinterpret_cast<void *>(load_gameplay),
                      reinterpret_cast<void **>(&original)) != 0 ||
            !original)
            throw std::runtime_error("Clank cannot register the native level loader");
        if (api->on_pad(mod, pad, nullptr) != 0 || api->on_frame(mod, frame, nullptr) != 0)
            throw std::runtime_error("Clank cannot register input/frame callbacks");
        api->log(mod, OPENRAC_LOG_INFO,
                 "L3+Triangle detaches Clank; Triangle near Ratchet reattaches");
        return 0;
    } catch (const std::exception &e) {
        if (api->fail)
            api->fail(mod, e.what());
        session.reset();
        original = nullptr;
        return 1;
    }
}
OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    session.reset();
    original = nullptr;
}
}

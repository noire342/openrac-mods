// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_control.hpp"
#include "native_addresses.hpp"
#include "clank_states.hpp"
#include "native_tables.hpp"
#include "asset_hash.hpp"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
namespace clank {
namespace {
using State = int (*)(int, int);
using Void = void (*)();
using Hit = int (*)(int);
using Relocate = std::uint32_t (*)(int, std::uint32_t);
using Lookup = void *(*)(std::uint32_t);
std::atomic<NativeControl *> context{};
// Process-lifetime fallthroughs remain valid if a hook must stay pinned.
std::array<void *, 12> original{};
void *direct_target(const void *function) {
    const auto *adapter = static_cast<const unsigned char *>(function);
    if (adapter[0] != 0xE9)
        throw std::runtime_error("Unexpected native SDK adapter in this executable");
    std::int32_t displacement{};
    std::memcpy(&displacement, adapter + 1, sizeof(displacement));
    auto *target = const_cast<unsigned char *>(adapter + 5 + displacement);
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(target, &region, sizeof(region)) ||
        region.AllocationBase != GetModuleHandleW(nullptr))
        throw std::runtime_error("Native SDK resolver is outside the verified executable");
    return target;
}
int state_hook(int value, int force) {
    auto *c = context.load(std::memory_order_acquire);
    return c && c->active() ? c->state(value, force, false)
                            : reinterpret_cast<State>(original[0])(value, force);
}
int full_state_hook(int value, int force) {
    auto *c = context.load(std::memory_order_acquire);
    return c && c->active() ? c->state(value, force, true)
                            : reinterpret_cast<State>(original[5])(value, force);
}
void motion_hook() {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->active())
        c->motion();
    else
        reinterpret_cast<Void>(original[1])();
}
void transitions_hook() {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->active())
        c->transitions();
    else
        reinterpret_cast<Void>(original[2])();
}
int hit_hook(int argument) {
    auto *c = context.load(std::memory_order_acquire);
    return c && c->active() ? c->hit(argument) : reinterpret_cast<Hit>(original[3])(argument);
}
void capsule_hook() {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->active())
        c->capsule();
    else
        reinterpret_cast<Void>(original[4])();
}
void vehicle_hook() {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->active())
        c->vehicle();
    else
        reinterpret_cast<Void>(original[7])();
}
void terrain_hook() {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->active())
        c->terrain();
    else
        reinterpret_cast<Void>(original[8])();
}
std::uint32_t relocate_hook(int from, std::uint32_t address) {
    auto *c = context.load(std::memory_order_acquire);
    return c && c->active_memory() ? c->relocate(from, address)
                                   : reinterpret_cast<Relocate>(original[6])(from, address);
}
void *lookup_hook(std::uint32_t address) {
    auto *c = context.load(std::memory_order_acquire);
    return c && (c->active_memory() || c->pending_hud())
               ? c->resolve(address)
               : reinterpret_cast<Lookup>(original[9])(address);
}
int icon_hook(int id, int frame) {
    auto *c = context.load(std::memory_order_acquire);
    if (c)
        c->remap_air_icon(id, frame);
    return reinterpret_cast<State>(original[10])(id, frame);
}
int warning_hook(int id, int variant) {
    auto *c = context.load(std::memory_order_acquire);
    if (c && c->drawing_air() && id == 0xB)
        return c->play_warning(variant);
    return reinterpret_cast<State>(original[11])(id, variant);
}
} // namespace
NativeControl::NativeControl(const openrac_api &api, openrac_mod *mod, NativeWorld &world,
                             NativeHud &hud)
    : m_api(api), m_mod(mod), m_world(world), m_hud(hud) {
    for (unsigned i = 0; i < 5; ++i) {
        m_full[i] = api.function(profile::full_functions[i]);
        if (!m_full[i])
            throw std::runtime_error("The complete native Clank controller is unavailable");
    }
    m_vehicle_update = api.function(profile::vehicle_update);
    if (!m_vehicle_update)
        throw std::runtime_error("The native Clank propeller update is unavailable");
    m_full_terrain = api.function(profile::terrain_update);
    if (!m_full_terrain)
        throw std::runtime_error("The native Clank terrain adapter is unavailable");
    // The pinned Windows build's SDK adapter is a direct tail jump to the
    // runtime resolver. Intercept that resolver, not only its SDK wrapper.
    const auto *adapter = reinterpret_cast<const unsigned char *>(api.relocate_level_data);
    char diagnostic[180];
    std::snprintf(diagnostic, sizeof(diagnostic),
                  "Native resolver adapter RVA=%llx bytes=%02x %02x %02x %02x %02x",
                  static_cast<unsigned long long>(
                      reinterpret_cast<std::uintptr_t>(adapter) -
                      reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))),
                  adapter[0], adapter[1], adapter[2], adapter[3], adapter[4]);
    api.log(mod, OPENRAC_LOG_INFO, diagnostic);
    m_resolver = direct_target(adapter);
    m_function_resolver = direct_target(reinterpret_cast<const void *>(api.function_at));
    m_icon_lookup = api.function(profile::icon_lookup);
    m_warning = api.function(profile::oxygen_warning);
    if (!m_icon_lookup || !m_warning)
        throw std::runtime_error("Native oxygen alarm services are unavailable");
    m_world.control_ready(false);
}
NativeControl::~NativeControl() { clear(); }
bool NativeControl::clear() noexcept {
    m_world.control_ready(false);
    context.store(nullptr, std::memory_order_release);
    bool clean = true;
    for (auto &hook : m_hooks) {
        if (!hook.created)
            continue;
        if (hook.enabled) {
            const auto result = MH_DisableHook(hook.target);
            if (result != MH_OK && result != MH_ERROR_DISABLED) {
                clean = false;
                continue;
            }
            hook.enabled = false;
        }
        if (MH_RemoveHook(hook.target) != MH_OK) {
            clean = false;
            continue;
        }
        hook = {};
    }
    return clean;
}
bool NativeControl::prepare(int level) noexcept {
    if (!clear() || level < 0 || level >= 19)
        return false;
    for (const auto &part : profile::companions) {
        const auto map = m_api.relocate_data(profile::class_map);
        const auto slot = static_cast<unsigned>(m_api.memory[map + part.id]);
        std::uint32_t data{};
        if (slot != 255)
            std::memcpy(&data,
                        m_api.memory + m_api.relocate_data(profile::class_resources) + slot * 4, 4);
        if (slot == 255 || data < 0x10000 || data > 0x1FFFFF0 ||
            m_api.memory[data + 0xC] != part.sequences) {
            m_api.log(m_mod, OPENRAC_LOG_ERROR,
                      "Native Clank propeller/glow resources are unavailable; detachment disabled");
            return false;
        }
    }
    for (const auto &table : clank_tables) {
        const auto mapped = m_api.relocate_level_data(0, table.canonical);
        const auto expected = table.addresses[level];
        const auto at = expected;
        const bool valid = at >= 0x10000 && at <= 0x02000000 - table.size &&
                           sha256_matches(m_api.memory + at, table.size, table.digest);
        if (!valid) {
            char line[160];
            std::snprintf(
                line, sizeof(line),
                "Clank native table validation failed: canonical=%x mapped=%x expected=%x level=%d",
                table.canonical, mapped, expected, level);
            m_api.log(m_mod, OPENRAC_LOG_ERROR, line);
            return false;
        }
    }
    for (const auto &region : clank_mutable_regions) {
        const auto mapped = m_api.relocate_level_data(0, region.canonical);
        const auto expected = region.addresses[level];
        if (mapped != expected || expected < 0x10000 || expected > 0x2000000 - region.size) {
            m_api.log(m_mod, OPENRAC_LOG_ERROR,
                      "Native Clank lean-state relocation is unavailable; detachment disabled");
            return false;
        }
    }
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    const auto &a = native_addresses[level];
    m_level_terrain = m_api.function_at(a.terrain);
    if (!m_level_terrain)
        return false;
    const std::array<std::uint32_t, 5> addresses{a.state, a.motion, a.transitions, a.hit,
                                                 a.capsule};
    const std::array<void *, 12> callbacks{
        reinterpret_cast<void *>(state_hook),       reinterpret_cast<void *>(motion_hook),
        reinterpret_cast<void *>(transitions_hook), reinterpret_cast<void *>(hit_hook),
        reinterpret_cast<void *>(capsule_hook),     reinterpret_cast<void *>(full_state_hook),
        reinterpret_cast<void *>(relocate_hook),    reinterpret_cast<void *>(vehicle_hook),
        reinterpret_cast<void *>(terrain_hook),     reinterpret_cast<void *>(lookup_hook),
        reinterpret_cast<void *>(icon_hook),        reinterpret_cast<void *>(warning_hook)};
    for (unsigned i = 0; i < callbacks.size(); ++i) {
        void *target = i == 11   ? m_warning
                       : i == 10 ? m_icon_lookup
                       : i == 9  ? m_function_resolver
                       : i == 8  ? m_full_terrain
                       : i == 7  ? m_api.function_at(a.vehicle)
                       : i == 6  ? m_resolver
                       : i == 5  ? m_full[0]
                                 : m_api.function_at(addresses[i]);
        if (!target) {
            clear();
            return false;
        }
        if ((i < 5 && target == m_full[i]) || (i == 7 && target == m_vehicle_update) ||
            (i == 8 && target == m_level_terrain))
            continue;
        auto &hook = m_hooks[i];
        hook.target = target;
        if (MH_CreateHook(target, callbacks[i], &original[i]) != MH_OK) {
            clear();
            return false;
        }
        hook.created = true;
    }
    m_level = level;
    m_hud.prepare(level);
    context.store(this, std::memory_order_release);
    for (auto &hook : m_hooks)
        if (hook.created) {
            if (MH_EnableHook(hook.target) != MH_OK) {
                clear();
                return false;
            }
            hook.enabled = true;
        }
    m_world.control_ready(true);
    char line[180];
    std::snprintf(
        line, sizeof(line),
        "Complete Clank controller ready on level %d; attack table=%x blend=%x speed=%x ledge=%x",
        level, clank_tables[0].addresses[level], clank_tables[3].addresses[level],
        clank_tables[1].addresses[level], clank_tables[4].addresses[level]);
    m_api.log(m_mod, OPENRAC_LOG_INFO, line);
    return true;
}
int NativeControl::state(int requested, int force, bool) {
    const int adapted = clank_state(requested);
    if (adapted != requested && requested >= 0 && requested < 256 && !m_reported[requested]) {
        m_reported[requested] = true;
        char line[96];
        std::snprintf(line, sizeof(line), "Clank state adapted before animation lookup: %x -> %x",
                      requested, adapted);
        m_api.log(m_mod, OPENRAC_LOG_INFO, line);
    }
    return reinterpret_cast<State>(original[5])(adapted, force);
}
void NativeControl::motion() { reinterpret_cast<Void>(m_full[1])(); }
void NativeControl::transitions() { reinterpret_cast<Void>(m_full[2])(); }
int NativeControl::hit(int argument) { return reinterpret_cast<Hit>(m_full[3])(argument); }
void NativeControl::capsule() { reinterpret_cast<Void>(m_full[4])(); }
void NativeControl::vehicle() { reinterpret_cast<Void>(m_vehicle_update)(); }
void NativeControl::terrain() {
    // The complete visual driver calls the level-0 surface routine. Its state
    // calls are not registered on stripped overlays. Keep each planet's own
    // surface logic and let our state-entry hook adapt its requests for Clank.
    reinterpret_cast<Void>(m_level_terrain)();
}
void *NativeControl::resolve(std::uint32_t address) const {
    if (auto *callback = m_hud.callback(address))
        return callback;
    return reinterpret_cast<Lookup>(original[9])(address);
}
std::uint32_t NativeControl::relocate(int from, std::uint32_t address) const {
    if (from == 0 && m_level >= 0 && m_level < 19)
        for (const auto &table : clank_tables) {
            if (address >= table.canonical && address - table.canonical < table.size)
                return table.addresses[m_level] + address - table.canonical;
        }
    if (from == 0 && m_level >= 0 && m_level < 19)
        for (const auto &region : clank_mutable_regions) {
            if (address >= region.canonical && address - region.canonical < region.size)
                return region.addresses[m_level] + address - region.canonical;
        }
    return reinterpret_cast<Relocate>(original[6])(from, address);
}
} // namespace clank

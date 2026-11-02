// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_world.hpp"
#include "held_loadout.hpp"
#include "native_addresses.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace clank {
void NativeWorld::log(const char *message) const { m_api.log(m_mod, OPENRAC_LOG_INFO, message); }
Observation NativeWorld::observe() const {
    openrac_play_status s{};
    s.size = sizeof(s);
    m_api.play_status(&s);
    openrac_pose pose{};
    pose.size = sizeof(pose);
    const bool present = m_api.hero_pose(&pose) == 0;
    const auto h = global(hero);
    openrac_held held{};
    held.size = sizeof(held);
    m_api.hero_held(&held);
    return {s.loads,
            s.phase == OPENRAC_PLAY_PLAYING,
            s.phase == OPENRAC_PLAY_MENU,
            present && pose.health > 0,
            read<std::uint8_t>(h + 0x20A4) == 0,
            carries_clank(held),
            pose.state >= 0 && pose.state <= 4,
            {pose.position[0], pose.position[1], pose.position[2]}};
}
void *NativeWorld::find_exit() const {
    // The short per-level exits omit Clank's children; use the original complete
    // exit so the native propeller and glow are removed with the owned hero.
    return m_api.function(profile::exit_full);
}
bool NativeWorld::owned() const {
    if (!m_active)
        return false;
    openrac_play_status state{};
    state.size = sizeof(state);
    m_api.play_status(&state);
    return state.loads == m_epoch && read<std::uint32_t>(global(hero) + 0x2080) == m_clank &&
           read<std::uint32_t>(global(hero) + 0xA88) == m_ratchet &&
           read<std::uint32_t>(m_clank + 0x24) == m_pclass &&
           read<std::uint16_t>(m_clank + 0xA6) == 87;
}
void NativeWorld::reset_position(const openrac_pose &pose) {
    using Reset = void (*)(std::uint32_t, std::uint32_t, int, int);
    const auto level = m_api.loaded_level();
    const auto fn = level >= 0 && level < 19
                        ? reinterpret_cast<Reset>(m_api.function_at(native_addresses[level].reset))
                        : nullptr;
    openrac_gaddr saved{};
    const auto at = m_api.stack_push(32, &saved);
    if (!fn || !at) {
        if (at)
            m_api.stack_pop(saved);
        const auto h = global(hero), body = read<std::uint32_t>(h + 0x2080);
        for (unsigned i = 0; i < 3; ++i) {
            write<float>(h + 0x80 + 4 * i, pose.position[i]);
            write<float>(h + 0x90 + 4 * i, pose.rotation[i]);
            write<float>(body + 0x10 + 4 * i, pose.position[i]);
        }
        log("Native reset unavailable; restored owned hero position directly");
        return;
    }
    std::memset(m_api.memory + at, 0, 32);
    std::memcpy(m_api.memory + at, pose.position, 12);
    std::memcpy(m_api.memory + at + 16, pose.rotation, 12);
    fn(at, at + 16, 0, 1);
    m_api.stack_pop(saved);
}
std::uint32_t NativeWorld::reserve_class(std::uint32_t cursor) {
    m_class_ready = false;
    if (cursor < 0x10000 || cursor > 0x01FFFFC0)
        return cursor;
    const auto level = m_api.loaded_level();
    if (level != m_blob_level) {
        m_blob = m_blob_end = 0;
        m_blob_level = level;
    }
    const auto map = global(profile::class_map);
    char admission[200];
    std::snprintf(admission, sizeof(admission),
                  "Clank class admission cursor=%x map=%x slot=%u count=%u registry=%d level=%d",
                  cursor, map, read<std::uint8_t>(map + 87),
                  read<std::uint32_t>(global(profile::class_count)),
                  m_api.function(profile::register_class) ? 1 : 0, m_api.loaded_level());
    log(admission);
    const auto slot = read<std::uint8_t>(map + 87);
    if (slot != 0xFF) {
        const auto data = read<std::uint32_t>(global(profile::class_resources) + 4u * slot);
        if (m_blob && data == m_blob) {
            // Checkpoint reloads retain resident classes but rewind the gameplay
            // heap. Keep our allocation reserved; never return a cursor through it.
            if (cursor > m_blob) {
                log("Clank resource overlaps a reloaded gameplay heap; detachment disabled");
            } else
                m_class_ready = true;
            return std::max(cursor, m_blob_end);
        }
        m_blob = m_blob_end = 0;
        m_class_ready = data >= 0x10000 && read<std::uint8_t>(data + 0xC) == 19;
        return cursor;
    }
    const auto &bytes = m_payload;
    const auto start = (cursor + 63u) & ~63u;
    if (cursor < 0x10000 || start > 0x01FFFFC0 || bytes.size() + 64 > 0x02000000 - start)
        return cursor;
    const auto count = read<std::uint32_t>(global(profile::class_count));
    using Register = void (*)(std::uint32_t, int, std::uint32_t, int);
    const auto fn = reinterpret_cast<Register>(m_api.function(profile::register_class));
    if (!fn || count >= 224)
        return cursor;
    std::memcpy(m_api.memory + start, bytes.data(), bytes.size());
    const auto material = start + static_cast<unsigned>(bytes.size());
    std::memset(m_api.memory + material, 0, 16);
    fn(start, 0, material, 87);
    if (read<std::uint8_t>(map + 87) == count) {
        m_blob = start;
        m_blob_end = (material + 16u + 63u) & ~63u;
        m_class_ready = true;
        log("Playable Clank class registered from the player's disc");
    }
    return (material + 16u + 63u) & ~63u;
}
bool NativeWorld::detach() {
    if (m_active || !m_control_ready || !m_class_ready)
        return false;
    const auto state = observe();
    if (!state.playing || !state.ratchet || !state.carrying_clank)
        return false;
    using Enter = void (*)(int, int, std::uint32_t);
    const auto level = m_api.loaded_level();
    if (level < 0 || level >= 19)
        return false;
    auto enter = reinterpret_cast<Enter>(m_api.function_at(native_addresses[level].enter));
    if (!enter || !find_exit() || !m_api.function_at(native_addresses[level].reset)) {
        log("Character switch is unavailable on this overlay");
        return false;
    }
    const auto slot = read<std::uint8_t>(global(profile::class_map) + 87);
    if (slot == 0xFF) {
        log("Playable Clank class has not been prepared");
        return false;
    }
    m_parked = {};
    m_parked.size = sizeof(m_parked);
    if (m_api.hero_pose(&m_parked) != 0)
        return false;
    openrac_held attached{};
    attached.size = sizeof(attached);
    m_api.hero_held(&attached);
    m_held = parked_loadout(attached);
    m_figure = m_api.figure_create(m_mod);
    if (m_figure < 0)
        return false;
    m_parked.anim_speed = 1;
    m_parked.seq = 0;
    m_parked.frame = 0;
    m_parked.next_frame = 1;
    m_parked.anim_time = 0;
    m_parked.flags = 0;
    m_api.figure_pose(m_mod, m_figure, &m_parked);
    m_api.figure_held(m_mod, m_figure, &m_held);
    openrac_moby actor{};
    actor.size = sizeof(actor);
    actor.position[0] = m_parked.position[0];
    actor.position[1] = m_parked.position[1];
    actor.position[2] = m_parked.position[2];
    actor.scale = 0.15f;
    m_clank_slot = m_api.moby_spawn(m_mod, 87, &actor);
    if (m_clank_slot < 0) {
        m_api.figure_remove(m_mod, m_figure);
        m_figure = -1;
        return false;
    }
    const auto pool = read<std::uint32_t>(global(profile::moby_pool));
    m_clank = pool + static_cast<std::uint32_t>(m_clank_slot) * 256;
    // Verify the pool-derived address against the requested native class before mutation.
    if (read<std::uint16_t>(m_clank + 0xA6) != 87) {
        m_api.moby_kill(m_mod, m_clank_slot);
        m_api.figure_remove(m_mod, m_figure);
        expire();
        return false;
    }
    m_pclass = read<std::uint32_t>(m_clank + 0x24);
    m_ratchet = read<std::uint32_t>(global(hero) + 0x2080);
    m_epoch = state.epoch;
    auto position = m_parked;
    position.position[0] += 0.75f;
    // Match the original Blarg handoff: resetting afterwards would put state 0
    // (Ratchet) on Clank's 19-animation model and enable incompatible gadgets.
    reset_position(position);
    m_active = true;
    enter(1, 0x43, m_clank);
    log("Clank detached; native control active; Ratchet parked");
    return true;
}
void NativeWorld::maintain() {
    if (!owned())
        return;
    m_api.figure_pose(m_mod, m_figure, &m_parked);
    m_api.figure_held(m_mod, m_figure, &m_held);
}
bool NativeWorld::restore(ReturnReason reason) {
    if (!m_active)
        return true;
    if (!owned()) {
        expire();
        return true;
    }
    using Exit = void (*)();
    const auto exit = reinterpret_cast<Exit>(find_exit());
    if (!exit)
        return false;
    const auto killed = reason == ReturnReason::Death
                            ? reinterpret_cast<Exit>(m_api.function(profile::on_killed))
                            : nullptr;
    if (reason == ReturnReason::Death && !killed)
        return false;
    exit();
    if (reason == ReturnReason::Death) {
        // Native death control mode refuses idle/reset transitions. Hand the
        // restored Ratchet to the game's real checkpoint reload instead.
        write<std::int32_t>(m_hero + 0x22A8, 0);
    } else {
        write<std::int32_t>(m_hero + 0x22A8, m_parked.health);
        reset_position(m_parked);
    }
    if (m_clank_slot >= 0)
        m_api.moby_kill(m_mod, m_clank_slot);
    if (m_figure >= 0)
        m_api.figure_remove(m_mod, m_figure);
    expire();
    if (killed) {
        killed();
        log("Clank died; native checkpoint respawn requested for Ratchet");
    } else
        log("Clank reattached; Ratchet restored at the parked position");
    return true;
}
void NativeWorld::expire() noexcept {
    m_active = false;
    m_figure = -1;
    m_clank_slot = -1;
    m_clank = m_ratchet = m_pclass = m_epoch = 0;
}
} // namespace clank

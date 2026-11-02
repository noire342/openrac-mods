// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_addresses.hpp"
#include "native_world.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)
namespace {
constexpr std::uint32_t hero = clank::profile::hero, pool = 0x180000, ratchet = pool + 0x100;
struct Fixture;
Fixture *current;
struct Fixture {
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0x02000000);
    std::vector<std::uint8_t> payload = std::vector<std::uint8_t>(493440);
    openrac_api api{};
    openrac_held attached{}, parked{};
    openrac_pose pose{}, figure{};
    int level{}, enters{}, exits{}, figures{}, removals{}, kills{}, spawns{}, stack_depth{},
        deaths{}, registrations{};
    bool spawn_ok = true, entry_available = true;
    std::uint32_t epoch = 1;
    template <class T> T read(std::uint32_t at) const {
        T value;
        std::memcpy(&value, memory.data() + at, sizeof(value));
        return value;
    }
    template <class T> void write(std::uint32_t at, T value) {
        std::memcpy(memory.data() + at, &value, sizeof(value));
    }
    static void enter(int mode, int state, std::uint32_t moby) {
        auto &f = *current;
        CHECK(mode == 1 && state == 0x43);
        ++f.enters;
        f.write<std::uint8_t>(hero + 0x20A4, 1);
        f.write(hero + 0x2080, moby);
        f.pose.state = state;
        f.write<std::int32_t>(hero + 0x2084, state);
        f.pose.health = 3;
        f.write<std::int32_t>(hero + 0x22A8, 3);
        f.write<std::uint32_t>(hero + 0x1620, pool + 0x1000);
        f.write<std::uint32_t>(hero + 0x1624, pool + 0x1100);
    }
    static void exit() {
        auto &f = *current;
        ++f.exits;
        f.write<std::uint8_t>(hero + 0x20A4, 0);
        f.write(hero + 0x2080, ratchet);
        if (f.pose.state != 0x7D)
            f.pose.state = 0;
    }
    static void reset(std::uint32_t pos, std::uint32_t rot, int a, int b) {
        auto &f = *current;
        CHECK(f.pose.state != 0x7D); // The real native death gate refuses an idle reset.
        CHECK(a == 0 && b == 1 && f.stack_depth == 1);
        std::memcpy(f.pose.position, f.memory.data() + pos, 12);
        std::memcpy(f.pose.rotation, f.memory.data() + rot, 12);
        // The real Hero_ResetToPosition changes state, it is not a teleport-only
        // function. This side effect was missing from the original test double.
        f.pose.state = a;
        f.write<std::int32_t>(hero + 0x2084, a);
    }
    static void complete_exit() {
        current->write<std::uint32_t>(hero + 0x1620, 0);
        current->write<std::uint32_t>(hero + 0x1624, 0);
        exit();
    }
    explicit Fixture(int id) : level(id) {
        current = this;
        api.memory = memory.data();
        api.relocate_data = [](std::uint32_t at) { return at; };
        api.loaded_level = [] { return current->level; };
        api.play_status = [](openrac_play_status *s) {
            s->loads = current->epoch;
            s->phase = OPENRAC_PLAY_PLAYING;
            return 0;
        };
        api.hero_pose = [](openrac_pose *p) {
            *p = current->pose;
            p->health = current->read<std::int32_t>(hero + 0x22A8);
            return 0;
        };
        api.hero_held = [](openrac_held *p) {
            *p = current->attached;
            return 0;
        };
        api.log = [](openrac_mod *, int, const char *) {};
        api.function = [](const char *name) -> void * {
            if (!std::strcmp(name, clank::profile::exit_full))
                return reinterpret_cast<void *>(Fixture::complete_exit);
            if (!std::strcmp(name, clank::profile::register_class))
                return reinterpret_cast<void *>(
                    +[](std::uint32_t data, int, std::uint32_t, int cls) {
                        CHECK(cls == 87);
                        auto &f = *current;
                        const auto slot = f.read<std::uint32_t>(clank::profile::class_count);
                        f.write<std::uint8_t>(clank::profile::class_map + 87,
                                              static_cast<std::uint8_t>(slot));
                        f.write<std::uint32_t>(clank::profile::class_resources + slot * 4, data);
                        f.write<std::uint32_t>(clank::profile::class_count, slot + 1);
                        ++f.registrations;
                    });
            if (std::strcmp(name, clank::profile::on_killed))
                return nullptr;
            return reinterpret_cast<void *>(+[] {
                CHECK(current->read<std::uint8_t>(hero + 0x20A4) == 0);
                CHECK(current->read<std::int32_t>(hero + 0x22A8) == 0);
                ++current->deaths;
            });
        };
        api.function_at = [](std::uint32_t at) -> void * {
            const auto &a = clank::native_addresses.at(current->level);
            if (at == a.enter)
                return current->entry_available ? reinterpret_cast<void *>(enter) : nullptr;
            if (at == a.exit)
                return reinterpret_cast<void *>(exit);
            if (at == a.reset)
                return reinterpret_cast<void *>(reset);
            throw std::runtime_error("Unknown native function address");
        };
        api.stack_push = [](std::uint32_t bytes, std::uint32_t *saved) {
            CHECK(bytes == 32);
            *saved = 0;
            ++current->stack_depth;
            return std::uint32_t(0x01FFF000);
        };
        api.stack_pop = [](std::uint32_t) { --current->stack_depth; };
        api.figure_create = [](openrac_mod *) {
            ++current->figures;
            return 4;
        };
        api.figure_pose = [](openrac_mod *, int id, const openrac_pose *p) {
            CHECK(id == 4);
            current->figure = *p;
            return 0;
        };
        api.figure_held = [](openrac_mod *, int id, const openrac_held *p) {
            CHECK(id == 4);
            current->parked = *p;
            return 0;
        };
        api.figure_remove = [](openrac_mod *, int id) {
            CHECK(id == 4);
            ++current->removals;
            return 0;
        };
        api.moby_spawn = [](openrac_mod *, int cls, const openrac_moby *) {
            CHECK(cls == 87);
            ++current->spawns;
            return current->spawn_ok ? 7 : -1;
        };
        api.moby_kill = [](openrac_mod *, int slot) {
            CHECK(slot == 7);
            ++current->kills;
            return 0;
        };
        pose.size = sizeof(pose);
        pose.position[0] = 10;
        pose.position[1] = 20;
        pose.position[2] = 30;
        pose.health = 4;
        pose.state = 0;
        attached.size = sizeof(attached);
        attached.count = 3;
        attached.pieces[0] = {0, 0, 8, 42, 5};
        attached.pieces[1] = {3, 0, 2, 607, 2};
        attached.pieces[2] = {3, 1, 2, 605, 2};
        write(hero + 0x2080, ratchet);
        write(hero + 0xA88, ratchet);
        write<std::int32_t>(hero + 0x22A8, 4);
        write<std::uint32_t>(clank::profile::moby_pool, pool);
        write<std::uint8_t>(clank::profile::class_map + 87, 10);
        write<std::uint32_t>(clank::profile::class_resources + 10 * 4, 0x140000);
        write<std::uint8_t>(0x140000 + 0xC, 19);
        payload[0xC] = 19;
        write<std::uint16_t>(pool + 7 * 256 + 0xA6, 87);
        write<std::uint32_t>(pool + 7 * 256 + 0x24, 0x140000);
    }
};
} // namespace
int main() {
    try {
        for (int level = 0; level < 19; ++level) {
            Fixture f(level);
            const auto original = f.attached;
            clank::NativeWorld world(f.api, nullptr, f.payload);
            world.control_ready(true);
            CHECK(world.reserve_class(0x1F00000) == 0x1F00000);
            for (int cycle = 0; cycle < 3; ++cycle) {
                CHECK(world.detach());
                CHECK(f.read<std::int32_t>(hero + 0x2084) == 0x43);
                CHECK(f.parked.count == 1 && f.parked.pieces[0].slot == 0);
                CHECK(f.figure.position[0] == 10 && f.pose.position[0] == 10.75f);
                CHECK(std::memcmp(&original, &f.attached, sizeof(original)) == 0);
                f.pose.position[0] = 50;
                world.maintain();
                CHECK(f.figure.position[0] == 10 && f.parked.count == 1);
                CHECK(world.restore(clank::ReturnReason::Nearby));
                CHECK(f.pose.position[0] == 10 && f.read<std::int32_t>(hero + 0x22A8) == 4);
                CHECK(f.read<std::uint8_t>(hero + 0x20A4) == 0);
                CHECK(f.read<std::uint32_t>(hero + 0x1620) == 0 &&
                      f.read<std::uint32_t>(hero + 0x1624) == 0);
                CHECK(f.removals == cycle + 1 && f.kills == cycle + 1 && f.stack_depth == 0);
                CHECK(std::memcmp(&original, &f.attached, sizeof(original)) == 0);
            }
            CHECK(f.enters == 3 && f.exits == 3);
            f.entry_available = false;
            CHECK(!world.detach() && f.figures == 3);
            f.entry_available = true;
            f.spawn_ok = false;
            CHECK(!world.detach() && f.removals == 4 && f.exits == 3);
            f.spawn_ok = true;
            CHECK(world.detach());
            f.pose.state = 0x7D;
            f.write<std::int32_t>(hero + 0x2084, 0x7D);
            f.write<std::int32_t>(hero + 0x22A8, 0);
            CHECK(world.restore(clank::ReturnReason::Death));
            CHECK(f.deaths == 1 && f.pose.state == 0x7D && f.stack_depth == 0);
            CHECK(world.restore(clank::ReturnReason::Death) && f.deaths == 1);
            // The actual native checkpoint reload is covered by the PAL runtime
            // fixture, not simulated by this host test.
            f.pose.state = 0;
            f.write<std::int32_t>(hero + 0x22A8, 4);
            CHECK(world.detach());
            ++f.epoch;
            CHECK(world.restore(clank::ReturnReason::LevelChange));
            CHECK(f.exits == 4 && f.kills == 4); // Retired pointers are never followed.
        }
        Fixture f(2);
        f.write<std::uint8_t>(clank::profile::class_map + 87, 0xFF);
        f.write<std::uint32_t>(clank::profile::class_count, 100);
        clank::NativeWorld world(f.api, nullptr, f.payload);
        const auto end = world.reserve_class(0x1F00000);
        CHECK(end > 0x1F00000 && end < 0x2000000 && f.registrations == 1);
        CHECK(world.reserve_class(0x1F00000 - 128) == end && f.registrations == 1);
        CHECK(f.read<std::uint32_t>(clank::profile::class_count) == 101);
        CHECK(world.reserve_class(0x1F00000 - 256) == end && f.registrations == 1);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}

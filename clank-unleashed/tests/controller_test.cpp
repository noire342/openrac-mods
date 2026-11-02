// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller.hpp"
#include "input.hpp"
#include <array>
#include <stdexcept>
using namespace clank;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)
struct World final : IWorld {
    Observation s{1, true, false, true, true, true, true, {10, 20, 30}};
    unsigned detach_count{}, returns{}, maintains{}, expired{};
    bool available = true, return_ok = true;
    ReturnReason reason{};
    Observation observe() const override { return s; }
    bool detach() override {
        ++detach_count;
        return available;
    }
    void maintain() override { ++maintains; }
    bool restore(ReturnReason why) override {
        ++returns;
        reason = why;
        return return_ok;
    }
    void expire() noexcept override { ++expired; }
};
int main() {
    World world;
    Controller c(world);
    c.tick({true, false});
    CHECK(c.phase() == Phase::Detached && world.detach_count == 1);
    world.s.position.x += 10;
    c.tick({false, true});
    CHECK(world.returns == 0);
    world.s.position = c.anchor();
    c.tick({false, true});
    CHECK(c.phase() == Phase::Attached && world.returns == 1);
    c.tick({true, false});
    world.s.menu = true;
    world.s.playing = false;
    c.tick({false, true});
    CHECK(c.phase() == Phase::Detached);
    world.s.menu = false;
    c.tick({});
    CHECK(world.reason == ReturnReason::Scene);
    world.s.playing = true;
    c.tick({true, false});
    world.s.alive = false;
    c.tick({});
    CHECK(world.reason == ReturnReason::Death);
    world.s.alive = true;
    c.tick({true, false});
    ++world.s.epoch;
    c.tick({});
    CHECK(c.phase() == Phase::Attached && world.expired == 1);
    world.s.grounded = false;
    c.tick({true, false});
    CHECK(c.phase() == Phase::Attached);
    world.s.grounded = true;
    world.available = false;
    c.tick({true, false});
    CHECK(c.phase() == Phase::Attached);
    world.available = true;
    c.tick({true, false});
    c.before_level_change();
    CHECK(world.reason == ReturnReason::LevelChange);
    c.tick({true, false});
    c.stop();
    CHECK(world.reason == ReturnReason::Shutdown);
    Input input;
    std::array<std::uint8_t, 18> report{};
    report[0] = 0xfd;
    report[1] = 0xef;
    report[10] = 255;
    input.poll(0, report.data(), 18, false, false);
    CHECK(input.take().detach && report[1] == 0xff && report[10] == 0);
    report[1] = 0xef;
    input.poll(0, report.data(), 18, false, false);
    CHECK(!input.take().detach);
    report[0] = report[1] = 0xff;
    input.poll(0, report.data(), 18, true, true);
    report[1] = 0xef;
    input.poll(0, report.data(), 18, true, true);
    CHECK(input.take().join);
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "tether.hpp"
#include "air_alarm.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace clank;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)
int main() {
    CHECK(air_alarm(3334, 60, 1, 1).frame == 0);
    CHECK(air_alarm(3333, 60, 1, 1).frame == 1 && air_alarm(3333, 60, 1, 1).warning);
    CHECK(air_alarm(2000, 44, 1, 1).frame == 1 && !air_alarm(2000, 44, 1, 1).warning);
    CHECK(air_alarm(2000, 45, 1, 1).frame == 0 && !air_alarm(2000, 45, 1, 1).warning);
    CHECK(air_alarm(2000, 61, 1, 1).frame == 1 && !air_alarm(2000, 61, 1, 1).warning);
    Tether off;
    CHECK(!off.update(true, true, 100, 0.1f).visible);
    Tether t({true, 25, 5});
    CHECK(!t.update(true, true, 25, 0.1f).visible);
    auto air = t.update(true, true, 26, 0.1f);
    CHECK(air.visible && air.remaining < 1 && !air.exhausted);
    const float paused = air.remaining;
    for (int i = 0; i < 100; ++i)
        air = t.update(true, false, 50, 0.1f);
    CHECK(air.remaining == paused);
    air = t.update(true, true, 24.5f, 0.1f);
    CHECK(air.remaining < paused);
    for (int i = 0; i < 50; ++i)
        air = t.update(true, true, 20, 0.1f);
    CHECK(air.remaining == 1 && !air.visible);
    for (int i = 0; i < 301; ++i)
        air = t.update(true, true, 50, 1.0f / 60);
    CHECK(air.exhausted && air.remaining == 0);
    air = t.update(false, true, 50, 0.1f);
    CHECK(!air.visible && !air.exhausted && air.remaining == 1);
    air = t.update(true, true, std::numeric_limits<float>::quiet_NaN(), 0.1f);
    CHECK(!air.exhausted && air.remaining == 1);
    air = t.update(true, true, 30, -1);
    CHECK(air.remaining == 1);
    bool rejected = false;
    try {
        Tether invalid({true, 0, 30});
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    CHECK(rejected);
    Tether native({true, 25, 0});
    for (int i = 0; i < 909; ++i)
        air = native.update(true, true, 50, 1.0f / 60, 11.0f / 10000);
    CHECK(!air.exhausted && std::abs(air.remaining - 0.0001f) < 0.00001f);
    CHECK(native.update(true, true, 50, 1.0f / 60, 11.0f / 10000).exhausted);
}

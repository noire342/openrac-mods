// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstring>
#include <vector>

#include "check.hpp"
#include "runtime.hpp"
using namespace cinematic;

namespace {
std::vector<std::uint8_t> ram(0x2000000);
const Disc* disc;
int level = 1, calls = 0;
std::uint16_t seen = 0xffff;
bool protect = true;
double duration = 5;

template <class T>
void put(std::uint32_t at, T value) {
    std::memcpy(ram.data() + at, &value, sizeof value);
}

template <class T>
T get(std::uint32_t at) {
    T value;
    std::memcpy(&value, ram.data() + at, sizeof value);
    return value;
}

std::uint32_t data(std::uint32_t at) {
    return at >= 0x15f000 ? at + 0x300000 : at;
}

std::uint32_t ldata(int source, std::uint32_t at) {
    CHECK(source == 0);
    return at + 0x400000;
}

int loaded() {
    return level;
}

int boolean(openrac_mod*, const char*) {
    return protect;
}

double number(openrac_mod*, const char*) {
    return duration;
}

void log(openrac_mod*, int, const char*) {}

void original(std::uint32_t, std::uint32_t report, int) {
    ++calls;
    seen = get<std::uint16_t>(report);
}

void* function(const char*) {
    return reinterpret_cast<void*>(original);
}

struct FakeClock final : Clock {
    double now = 0;

    double seconds() const noexcept override { return now; }
};

void mode(int value) {
    put<std::int32_t>(ldata(0, disc->level_mode), value);
}

void ship(int state, int busy, int phase, int done = 0) {
    put<std::int32_t>(disc->ship + 0x20, state);
    put<std::int16_t>(disc->loader_busy, static_cast<std::int16_t>(busy));
    put<std::int32_t>(disc->ship + 0x5c, phase);
    put<std::int32_t>(data(disc->scene_done), done);
}

constexpr std::uint32_t report_address = 0x10000;

std::uint16_t poll(Runtime& runtime, std::uint16_t raw, bool game = true, int port = 0) {
    std::array<std::uint8_t, 18> report{};
    report[0] = static_cast<std::uint8_t>(raw);
    report[1] = static_cast<std::uint8_t>(raw >> 8);
    runtime.pad(port, report.data(), static_cast<int>(report.size()));
    // Exactly where the host's skip_after_mods runs: after on_pad, before Pad_ProcessReport.
    auto filtered = static_cast<std::uint16_t>(report[0] | (report[1] << 8));
    if ((filtered & start) == 0 && get<std::int32_t>(ldata(0, disc->level_mode)) == 2) {
        filtered = static_cast<std::uint16_t>(filtered & ~shoulders);
    }
    write_buttons(report.data(), report.size(), filtered);
    std::memcpy(ram.data() + report_address, report.data(), report.size());
    if (game) {
        runtime.process(disc->pad, report_address, 18);
    }
    return get<std::uint16_t>(report_address);
}
}  // namespace

int main() {
    for (const auto& version : discs) {
        disc = &version;
        level = 1;
        protect = true;
        duration = 5;
        openrac_api api{};
        api.game = "rac1";
        api.version = disc->version;
        api.memory = ram.data();
        api.setting_bool = boolean;
        api.setting_float = number;
        api.function = function;
        api.log = log;
        api.relocate_data = data;
        api.relocate_level_data = ldata;
        api.loaded_level = loaded;
        FakeClock clock;
        Runtime runtime(api, nullptr, clock);
        runtime.load();
        runtime.set_original(reinterpret_cast<void*>(original));
        mode(0);
        ship(8, 0, 0);
        poll(runtime, 0xfff7);
        CHECK((seen & start) == 0);                                // normal pause works
        CHECK((get<std::uint16_t>(report_address) & start) != 0);  // report restored
        mode(2);
        for (unsigned mask : {0u, 0x100u, 0x300u, 0x700u}) {
            poll(runtime, static_cast<std::uint16_t>(0xfff7 & ~mask));
            CHECK((seen & start) != 0);
        }
        poll(runtime, 0xf0f7);
        CHECK((seen & 0x0f08) == 0);  // full native chord
        mode(0);
        CHECK((poll(runtime, 0xfff7, false) & start) != 0);  // movie even with stale playing mode
        CHECK((poll(runtime, 0xf0f7, false) & start) == 0);  // deliberate movie skip
        level = -1;
        CHECK((poll(runtime, 0xfff7, false) & start) != 0);  // boot movie
        poll(runtime, 0xfff7);
        CHECK((seen & start) == 0);  // title menu still works
        level = 1;
        mode(6);
        ship(8, 0, 0);
        poll(runtime, 0x8ff7);
        CHECK((seen & 0x7008) == 0x7008);  // no face-button skip
        poll(runtime, 0x80f7);
        CHECK((seen & 0x7008) == 0);  // deliberate face/chord allowed
        mode(0);
        ship(8, 0, 0);
        poll(runtime, 0x8fff);
        CHECK(seen == 0x8fff);  // gameplay faces intact
        poll(runtime, 0xfff7, true, 1);
        CHECK(seen == 0xfff7);  // second pad is not filtered
        ship(4, 1, 1);
        clock.now = 100;
        poll(runtime, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 0);
        CHECK(get<std::int16_t>(disc->loader_busy) == 1 && get<int>(data(disc->scene_done)) == 0);
        ship(4, 1, 1);
        clock.now = 104.9;
        poll(runtime, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 0);
        ship(4, 1, 1);
        clock.now = 105;
        poll(runtime, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 1);
        ship(4, 0, 1, 1);
        poll(runtime, 0xffff);
        CHECK(get<int>(data(disc->scene_done)) == 1);
        ship(4, 1, 2);
        clock.now = 120;
        poll(runtime, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 2);
        ship(8, 0, 1);
        poll(runtime, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 1);
        protect = false;
        duration = 0;
        Runtime disabled(api, nullptr, clock);
        disabled.load();
        disabled.set_original(reinterpret_cast<void*>(original));
        mode(2);
        poll(disabled, 0xfff7);
        CHECK((seen & start) == 0);
        ship(4, 1, 1);
        poll(disabled, 0xffff);
        CHECK(get<int>(disc->ship + 0x5c) == 1);
    }
    CHECK(calls > 30);
}

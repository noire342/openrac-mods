// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <limits>

#include "check.hpp"
#include "policy.hpp"
using namespace cinematic;

int main() {
    // Every possible pad combination: no partial chord leaks Start to the host movie player.
    for (unsigned word = 0; word <= 65535; ++word) {
        const auto input = static_cast<std::uint16_t>(word);
        const bool chord = (input & 0x0f08) == 0;
        const auto output = filter_buttons(input, true, true);
        CHECK(chord ? output == input : (output & 0x7008) == 0x7008);
        CHECK(filter_buttons(input, false, true) == input);
        CHECK((output & 0x8ff7) == (input & 0x8ff7));
    }
    CHECK(protected_scene(1, 2, 0, false));
    CHECK(protected_scene(18, 6, 8, false));
    CHECK(!protected_scene(1, 0, 8, false));
    CHECK(!protected_scene(-1, 2, 0, false));
    CHECK(protected_scene(-1, -1, 4, true));
    std::array<std::uint8_t, 18> report{};
    report.fill(111);
    report[0] = 0xf7;
    report[1] = 0x8f;
    write_buttons(report.data(), report.size(), 0xffff);
    CHECK(report[10] == 0 && report[11] == 0 && report[12] == 0);
    CHECK(report[13] == 111 && report[14] == 111);
    CHECK(report[2] == 111 && report[5] == 111);
    auto before = report;
    write_buttons(report.data(), 1, 0);
    CHECK(report == before);
    write_buttons(nullptr, 18, 0);
    FlightMinimum flight(5);
    CHECK(!flight.hold(false, 100));
    CHECK(flight.hold(true, 100));
    CHECK(flight.hold(true, 104.999));
    CHECK(!flight.hold(true, 105));
    CHECK(!flight.hold(true, 150));
    CHECK(!flight.hold(false, 150));
    CHECK(flight.hold(true, 160));  // a second trip has its own minimum
    FlightMinimum disabled(0);
    CHECK(!disabled.hold(true, 0));
    CHECK(valid_seconds(-9) == 0 && valid_seconds(80) == 30);
    CHECK(valid_seconds(std::numeric_limits<double>::quiet_NaN()) == 5);
}

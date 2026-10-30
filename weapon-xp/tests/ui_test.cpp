// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <fstream>

#include "check.hpp"
#include "ui.hpp"

using namespace weaponxp;

int main(int argc, char** argv) {
    Progression state;
    Ui ui;
    ui.reduced_motion = true;
    const WeaponTile tile{40, 18, 120, 128, 0x80E0E0E0};
    CHECK(ui.draw(0x0C, state, tile).empty());
    CHECK(ui.draw(0x0F, state, {40, 18, 120, 0, 0}).empty());
    state.award(0x0F, 2000);
    ui.tick(1.0f / 60.0f, 0x0F, state);
    auto commands = ui.draw(0x0F, state, tile);
    CHECK(commands.size() == 8 && commands[0].text == "LV 3");
    for (const auto& c : commands) {
        CHECK(c.x >= tile.x + 34 && c.x + c.width <= tile.right);
        CHECK(c.y >= tile.y && c.y + c.height <= tile.y + 32);
        CHECK((c.color >> 24) <= 128);
    }
    auto wide = tile;
    wide.x -= 100;
    wide.right -= 100;
    const auto moved = ui.draw(0x0F, state, wide);
    CHECK(moved[0].x == commands[0].x - 100);
    ui.level_up(0x0F, 3);
    CHECK(ui.draw(0x0F, state, tile)[0].text == "LV 3");
    state.award(9, 2000);
    ui.tick(1.0f / 60.0f, 9, state);
    const auto vertical = ui.draw_vertical(9, state, 512, 112, 42, 192, 0x80C0A080);
    CHECK(vertical.size() == 5 && vertical[0].text == "LV 3");
    CHECK(vertical[4].width == 2 && vertical[4].height == 51);
    CHECK(vertical[4].y + vertical[4].height == 112 + 28 + 154 - 1);
    for (const auto& c : vertical) {
        CHECK(c.x >= 512 && c.x + c.width <= 554);
        CHECK(c.y >= 112 && c.y + c.height <= 304);
    }
    for (const int item : {9, 15}) {
        state.award(item, kThresholds.back());
        ui.tick(1.0f / 60.0f, item, state);
        ui.level_up(item, 10);
        const auto maxed = item == 9 ? ui.draw_vertical(item, state, 512, 112, 42, 192, 0x80C0A080)
                                     : ui.draw(item, state, tile);
        CHECK(maxed[0].text == "LV 10");
        if (item == 15) {
            CHECK(maxed[2].x >= maxed[0].x + 30);  // Reserve space for the two digit level.
        }
    }
    ui.enabled = false;
    CHECK(ui.draw(0x0F, state, tile).empty());
    if (argc == 2) {
        std::ofstream out(argv[1]);
        for (const auto& c : commands) {
            out << (c.kind == Draw::Kind::Text ? "text" : "rect") << '\t' << c.x << '\t' << c.y
                << '\t' << c.width << '\t' << c.height << '\t' << c.color << '\t' << c.text << '\n';
        }
    }
    std::puts("existing weapon tile geometry, visibility, palette and widescreen anchoring: ok");
}

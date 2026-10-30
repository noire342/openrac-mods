// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "progression.hpp"

namespace weaponxp {

struct Draw {
    enum class Kind {
        Rectangle,
        Text
    };
    Kind kind;
    int x, y, width, height;
    std::uint32_t color;
    std::string text;
};

// Geometry and presentation of the game's existing weapon tile, supplied by the SDK adapter.
// Relative coordinates naturally follow its animation, widescreen anchors and visibility.
struct WeaponTile {
    int x = 0, y = 0, right = 0;
    int alpha = 0;
    std::uint32_t color = 0;
};

class Ui {
public:
    bool enabled = true;
    bool reduced_motion = false;
    void tick(float seconds, int item, const Progression& state);
    void level_up(int item, int new_level);
    std::vector<Draw> draw(int item, const Progression& state, const WeaponTile& tile) const;
    std::vector<Draw> draw_vertical(
        int item, const Progression& state, int x, int y, int width, int height, std::uint32_t color
    ) const;

private:
    int m_item = -1;
    int m_level = 1;
    float m_progress = 0.0f;
    int m_upgrade_item = -1;
    float m_upgrade_time = 0.0f;
};

}  // namespace weaponxp

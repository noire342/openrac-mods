// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "ui.hpp"

#include <algorithm>
#include <cmath>

namespace weaponxp {

void Ui::tick(float seconds, int item, const Progression& state) {
    const float dt = std::clamp(seconds, 0.0f, 0.1f);
    const int current_level = state.weapon_level(item);
    const float target = state.weapon_progress(item);
    if (item != m_item || reduced_motion) {
        m_progress = target;
    } else if (current_level != m_level) {
        m_progress = 0.0f;
    }
    m_progress += (target - m_progress) * (1.0f - std::exp(-dt * 16.0f));
    m_item = item;
    m_level = current_level;
    m_upgrade_time = std::max(0.0f, m_upgrade_time - dt);
}

void Ui::level_up(int item, int new_level) {
    if (weapon_index(item) >= 0 && new_level >= 2 && new_level <= kMaxLevel) {
        m_upgrade_item = item;
        m_upgrade_time = 2.0f;
    }
}

std::vector<Draw> Ui::draw(int item, const Progression& state, const WeaponTile& tile) const {
    std::vector<Draw> out;
    if (!enabled || weapon_index(item) < 0 || tile.alpha <= 0 || tile.right - tile.x < 32) {
        return out;
    }
    // Everything fits inside the existing tile's 32-pixel height. There is no background,
    // independent visibility timer, window, menu, toast, input binding or additional HUD bank.
    const int left = tile.x + 68;
    const int width = tile.right - left;
    if (width < 8) {
        return out;
    }
    out.reserve(8);
    const auto alpha = static_cast<std::uint32_t>(std::clamp(tile.alpha, 0, 128));
    auto rgb = tile.color & 0xFFFFFFu;
    if (!reduced_motion && m_upgrade_item == item && m_upgrade_time > 0) {
        const float boost =
            std::sin(std::clamp(m_upgrade_time / 2.0f, 0.0f, 1.0f) * 3.14159265f) * 0.18f;
        std::uint32_t lit = 0;
        for (unsigned shift = 0; shift < 24; shift += 8) {
            lit |= static_cast<std::uint32_t>(
                       std::min(255.0f, ((rgb >> shift) & 255) * (1.0f + boost))
                   )
                   << shift;
        }
        rgb = lit;
    }
    const auto color = rgb | (alpha << 24);
    const std::string label = "LV " + std::to_string(state.weapon_level(item));
    // Approved mockup A: a quiet secondary row under the ammo. The icon and original ammo
    // typography are never covered or rewritten. Text uses the game's own atlas at 10/16 size.
    out.push_back(
        {Draw::Kind::Text, tile.x + 35, tile.y + 22, 0, 10, (alpha * 3 / 4) << 24, label}
    );
    out.push_back({Draw::Kind::Text, tile.x + 34, tile.y + 21, 0, 10, color, label});
    auto rect = [&](int x, int y, int w, int h, std::uint32_t c) {
        if (w > 0) {
            out.push_back({Draw::Kind::Rectangle, x, y, w, h, c, {}});
        }
    };
    // Recessed rail with one-pixel curved ends, a dark interior and a restrained native-blue rim.
    rect(left + 1, tile.y + 24, width - 2, 1, rgb | ((alpha / 3) << 24));
    rect(left, tile.y + 25, width, 3, rgb | ((alpha / 3) << 24));
    rect(left + 1, tile.y + 28, width - 2, 1, rgb | ((alpha / 5) << 24));
    rect(left + 1, tile.y + 25, width - 2, 3, (alpha * 3 / 4) << 24);
    const float fraction = item == m_item ? m_progress : state.weapon_progress(item);
    const int fill = static_cast<int>(std::round((width - 2) * std::clamp(fraction, 0.0f, 1.0f)));
    if (fill > 0) {
        rect(left + 1, tile.y + 25, fill, 3, color);
        const auto highlight = ((rgb & 0xFEFEFEu) >> 1) + 0x606060u;
        rect(left + 1, tile.y + 25, fill, 1, highlight | ((alpha * 3 / 4) << 24));
    }
    return out;
}

std::vector<Draw> Ui::draw_vertical(
    int item, const Progression& state, int x, int y, int width, int height, std::uint32_t color
) const {
    if (!enabled || weapon_index(item) < 0 || width < 42 || height < 64) {
        return {};
    }
    const auto rgb = color & 0xFFFFFFu;
    const int left = x + width - 10, top = y + 28, length = height - 38;
    std::vector<Draw> out;
    const std::string label = "LV " + std::to_string(state.weapon_level(item));
    const int text_size = state.weapon_level(item) >= 10 ? 8 : 10;
    const int text_x = x + (text_size == 8 ? 8 : 9);
    out.push_back({Draw::Kind::Text, text_x + 1, y + 9, 0, text_size, 0x60000000, label});
    out.push_back({Draw::Kind::Text, text_x, y + 8, 0, text_size, rgb | 0x80000000, label});
    out.push_back({Draw::Kind::Rectangle, left, top, 4, length, rgb | 0x30000000, {}});
    out.push_back({Draw::Kind::Rectangle, left + 1, top + 1, 2, length - 2, 0x60000000, {}});
    const float fraction = item == m_item ? m_progress : state.weapon_progress(item);
    const int fill = static_cast<int>(std::round((length - 2) * std::clamp(fraction, 0.0f, 1.0f)));
    if (fill > 0) {
        out.push_back(
            {Draw::Kind::Rectangle,
             left + 1,
             top + length - 1 - fill,
             2,
             fill,
             rgb | 0x80000000,
             {}}
        );
    }
    return out;
}

}  // namespace weaponxp

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "discs.hpp"
#include "openrac/openrac_mod.h"
#include "ui.hpp"

namespace weaponxp {
// Draws the existing game's font atlas and filled primitives at the layout's requested size.
// This is an SDK renderer adapter; Ui owns layout, colours and animation.
class NativeHud {
public:
    struct Bounds {
        int x, y, width, height;
    };

    NativeHud(const openrac_api* api, const Disc& disc);
    void draw(const Draw& command) const;
    void draw_gauge(const std::vector<Draw>& commands) const;
    Bounds gauge_bounds(std::uint32_t element) const;
    std::uint32_t packet_cursor() const;
    void reflow_gauge(std::uint32_t first_packet, const Bounds& bounds) const;

private:
    using Quad = void (*)(int, int, int, int, int, int, int, int, std::uint64_t, std::uint64_t);
    using Texture = std::uint64_t (*)(int);
    using Rectangle = void (*)(int, int, int, int, std::uint64_t, int);
    using Position = int (*)(std::uint32_t, std::uint32_t, std::uint32_t);
    using Transform = void (*)(std::uint32_t, std::uint32_t, std::uint32_t, int, int);
    const openrac_api* m_api;
    const Disc& m_disc;
    Quad m_quad;
    Texture m_texture;
    Rectangle m_rectangle;
    Position m_position;
    Transform m_transform;
    void draw_at_depth(const Draw& command, std::uint32_t depth) const;
    void foreground(std::uint32_t packet, bool quad, std::uint32_t depth) const;
};
}  // namespace weaponxp

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace weaponxp {
NativeHud::NativeHud(const openrac_api* api, const Disc& disc)
    : m_api(api),
      m_disc(disc),
      m_quad(reinterpret_cast<Quad>(api->function(disc.quad))),
      m_texture(reinterpret_cast<Texture>(api->function(disc.texture))),
      m_rectangle(reinterpret_cast<Rectangle>(api->function(disc.rectangle))),
      m_position(reinterpret_cast<Position>(api->function(disc.screen_position))),
      m_transform(reinterpret_cast<Transform>(api->function(disc.transform))) {
    if (!m_quad || !m_texture || !m_rectangle || !m_position || !m_transform) {
        throw std::runtime_error("OpenRAC has no compatible native HUD drawing functions");
    }
}

NativeHud::Bounds NativeHud::gauge_bounds(std::uint32_t element) const {
    auto read_int = [&](std::uint32_t at) {
        int value;
        std::memcpy(&value, m_api->memory + at, 4);
        return value;
    };

    // SDK stack handles belong to the port's mapped 0xE0000000 region, beyond console RAM.
    struct Frame {
        const openrac_api* api;
        openrac_gaddr saved = 0, address = 0;

        explicit Frame(const openrac_api* a) : api(a) { address = api->stack_push(8, &saved); }

        ~Frame() { api->stack_pop(saved); }
    } frame(m_api);

    if (!frame.address) {
        return {};
    }
    const int initial[2] = {read_int(element + 0x50), read_int(element + 0x54)};
    std::memcpy(m_api->memory + frame.address, initial, sizeof initial);
    m_position(element, frame.address, frame.address + 4);
    m_transform(element, frame.address, frame.address + 4, read_int(element + 0x6C), 0);
    std::int16_t offsets[2];
    std::memcpy(offsets, m_api->memory + element + 0x48, sizeof offsets);
    return {
        read_int(frame.address) + offsets[0],
        read_int(frame.address + 4) + offsets[1],
        read_int(element + 0x58),
        read_int(element + 0x5C)
    };
}

std::uint32_t NativeHud::packet_cursor() const {
    const auto cursor =
        m_api->relocate_data(std::strcmp(m_disc.version, "pal") == 0 ? 0x161000 : 0x160F00);
    unsigned at = 0;
    std::memcpy(&at, m_api->memory + cursor, 4);
    return at;
}

void NativeHud::reflow_gauge(std::uint32_t first, const Bounds& bounds) const {
    const auto end = packet_cursor();
    if (bounds.width < 42 || bounds.height < 64 || first < 0x10000 || end < first || end > 0x2000000
        || end - first > 2048) {
        return;
    }
    auto read = [&](unsigned at) {
        unsigned v = 0;
        std::memcpy(&v, m_api->memory + at, 4);
        return v;
    };
    if (read(first) != 0x10000003) {
        return;
    }
    const int old_top = int((read(first + 48) >> 16) & 0xFFFF),
              bottom = int((read(first + 56) >> 16) & 0xFFFF);
    const int original_margin = bounds.height * 10 / 256;
    const int new_top = old_top + (28 - original_margin) * 16;
    if (bottom <= new_top || old_top >= bottom) {
        return;
    }
    // Reserve a header and a slim XP channel within the existing casing. Reflow only the
    // original well/fill/separators; the native textured frame, ammo count and ratios stay intact.
    for (unsigned at = first; at + 16 <= end;) {
        const auto tag = read(at);
        const unsigned bytes = ((tag & 0xFFFF) + 1) * 16;
        if (bytes > end - at || bytes < 16) {
            return;
        }
        if (tag == 0x10000003 && bytes == 64) {
            for (unsigned offset : {48u, 56u}) {
                auto xy = read(at + offset);
                int x = int(xy & 0xFFFF), y = int((xy >> 16) & 0xFFFF);
                y = new_top + (y - old_top) * (bottom - new_top) / (bottom - old_top);
                if (offset == 56) {
                    x -= 4 * 16;
                }
                xy = unsigned(x & 0xFFFF) | (unsigned(y & 0xFFFF) << 16);
                std::memcpy(m_api->memory + at + offset, &xy, 4);
            }
        }
        at += bytes;
    }
}

void NativeHud::draw_gauge(const std::vector<Draw>& commands) const {
    const bool pal = std::strcmp(m_disc.version, "pal") == 0;
    const auto layer = m_api->relocate_level_data(0, pal ? 0x17E5D8 : 0x17E4D8);
    std::uint32_t frame_depth = 0;
    std::memcpy(&frame_depth, m_api->memory + layer + 12, 4);
    const auto depth = std::min(0xFFFFFFu, std::max(0xFFFFF1u, frame_depth + 1));
    const auto general = m_api->relocate_data(pal ? 0x161000 : 0x160F00);
    const auto hud = m_api->relocate_level_data(0, pal ? 0x161280 : 0x1611C0);
    if (general == hud) {
        for (const auto& command : commands) {
            draw_at_depth(command, depth);
        }
        return;
    }
    std::uint32_t saved = 0, cursor = 0;
    std::memcpy(&saved, m_api->memory + general, 4);
    std::memcpy(&cursor, m_api->memory + hud, 4);
    if (cursor < 0x10000 || cursor >= 0x2000000 - 2048) {
        return;
    }
    // The gauge frame is emitted into the HUD's later packet stream. Append decorations there
    // too; general effects are rendered before that opaque frame and would be covered by it.
    std::memcpy(m_api->memory + general, &cursor, 4);
    try {
        for (const auto& command : commands) {
            draw_at_depth(command, depth);
        }
    } catch (...) {
        std::memcpy(m_api->memory + general, &saved, 4);
        throw;
    }
    std::memcpy(m_api->memory + hud, m_api->memory + general, 4);
    std::memcpy(m_api->memory + general, &saved, 4);
}

void NativeHud::foreground(std::uint32_t packet, bool quad, std::uint32_t depth) const {
    if (!depth || packet < 0x10000 || packet > 0x2000000 - 128) {
        return;
    }
    std::uint32_t tag = 0;
    std::memcpy(&tag, m_api->memory + packet, 4);
    if (tag != (quad ? 0x10000007u : 0x10000003u)) {
        return;
    }
    // These native helpers use fixed GS packet layouts. Only our newly emitted vertices move
    // in front of the gauge's opaque HUD plane; no existing frame or ammo packet is rewritten.
    if (quad) {
        for (unsigned offset : {68u, 84u, 100u, 116u}) {
            std::memcpy(m_api->memory + packet + offset, &depth, 4);
        }
    } else {
        for (unsigned offset : {52u, 60u}) {
            std::memcpy(m_api->memory + packet + offset, &depth, 4);
        }
    }
}

void NativeHud::draw(const Draw& c) const {
    draw_at_depth(c, 0);
}

void NativeHud::draw_at_depth(const Draw& c, std::uint32_t depth) const {
    const auto cursor =
        m_api->relocate_data(std::strcmp(m_disc.version, "pal") == 0 ? 0x161000 : 0x160F00);
    auto packet = [&]() {
        unsigned address = 0;
        std::memcpy(&address, m_api->memory + cursor, 4);
        return address;
    };
    if (c.kind == Draw::Kind::Rectangle) {
        if (c.width > 0 && c.height > 0) {
            const auto before = packet();
            m_rectangle(c.x, c.y, c.x + c.width, c.y + c.height, c.color, 0);
            foreground(before, false, depth);
        }
        return;
    }
    const auto metrics = m_api->relocate_data(m_disc.small_font_metrics);
    if (metrics < 0x10000 || metrics > 0x02000000 - 0x3A0) {
        return;
    }
    const int pixels = std::clamp(c.height, 8, 16);
    const float scale = pixels / 16.0f;
    const auto texture = m_texture(2);
    float x = static_cast<float>(c.x);
    // Same u/v, baseline and advance as FontPrintSmall; size is reduced without changing
    // the global font tables or the ammo drawer. Only our short ASCII level label is drawn.
    for (unsigned char ch : c.text) {
        if (ch < 0x20 || ch >= 0x80) {
            continue;
        }
        const auto* glyph = m_api->memory + metrics + 4 * ch;
        const auto top = static_cast<std::int8_t>(glyph[2]);
        const auto advance = static_cast<std::int8_t>(glyph[3]);
        if (ch != 0x20 && advance > 0) {
            const auto before = packet();
            m_quad(
                static_cast<int>(std::round(x)),
                c.y + static_cast<int>(std::round(top * scale)),
                pixels,
                pixels,
                glyph[0],
                glyph[1],
                16,
                16,
                c.color,
                texture
            );
            foreground(before, true, depth);
        }
        x += advance * scale;
    }
}
}  // namespace weaponxp

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace clank {
// Keep native glass, frame and particle motion. Only their pixels change into
// softly lit electrical arcs, sampled at 64x64 for smooth small HUD sprites.
struct EnergyArtwork {
    static constexpr std::uint16_t icon = 0x7FFE, meter_icon = 0x7FFD;
    static constexpr unsigned frames = 2, side = 64, log2_side = 6;
    std::array<std::uint8_t, 1024> palette{};
    std::array<std::array<std::uint8_t, side * side>, frames> images{};
};
inline EnergyArtwork make_energy_artwork() {
    EnergyArtwork out;
    for (unsigned critical = 0; critical < 2; ++critical)
        for (unsigned level = 1; level < 128; ++level) {
            const float intensity = level / 127.0f;
            const float core = std::clamp((intensity - 0.45f) / 0.55f, 0.0f, 1.0f);
            const unsigned index = level + critical * 128;
            const unsigned clut = (index & ~24u) | ((index & 8u) << 1) | ((index & 16u) >> 1);
            auto *color = out.palette.data() + clut * 4;
            color[0] = static_cast<std::uint8_t>(critical ? 255 : 70 + core * 185);
            color[1] = static_cast<std::uint8_t>(critical ? 70 + core * 180 : 195 + core * 60);
            color[2] = static_cast<std::uint8_t>(critical ? 25 + core * 175 : 255);
            color[3] = static_cast<std::uint8_t>(std::pow(intensity, 0.8f) * 128);
        }
    struct Point {
        float x, y;
    };
    struct Segment {
        Point a, b;
    };
    constexpr std::array<Segment, 9> paths{{{{31, 2}, {21, 17}},
                                            {{21, 17}, {38, 24}},
                                            {{38, 24}, {26, 40}},
                                            {{26, 40}, {40, 46}},
                                            {{40, 46}, {30, 61}},
                                            {{31, 33}, {11, 27}},
                                            {{11, 27}, {17, 19}},
                                            {{36, 51}, {53, 54}},
                                            {{53, 54}, {46, 61}}}};
    for (unsigned y = 0; y < EnergyArtwork::side; ++y)
        for (unsigned x = 0; x < EnergyArtwork::side; ++x) {
            float distance = std::numeric_limits<float>::max();
            for (const auto &line : paths) {
                const float dx = line.b.x - line.a.x, dy = line.b.y - line.a.y;
                const float t = std::clamp(
                    ((x + 0.5f - line.a.x) * dx + (y + 0.5f - line.a.y) * dy) / (dx * dx + dy * dy),
                    0.0f, 1.0f);
                distance = std::min(distance, std::hypot(x + 0.5f - line.a.x - dx * t,
                                                         y + 0.5f - line.a.y - dy * t));
            }
            const float core = std::clamp((4.0f - distance) * 0.5f, 0.0f, 1.0f);
            const float glow = std::exp(-distance * distance / 49.0f) * 0.28f;
            const auto level = static_cast<std::uint8_t>(std::clamp(core + glow, 0.0f, 1.0f) * 127);
            out.images[0][y * EnergyArtwork::side + x] = level;
            out.images[1][y * EnergyArtwork::side + x] = level ? level + 128 : 0;
        }
    return out;
}
} // namespace clank

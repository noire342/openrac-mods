// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <array>
#include <cstddef>

namespace spacevoyages {
namespace galaxy_bounds {
inline constexpr float soft_radius=1300.f,hard_radius=1450.f;
inline constexpr float catalog_limit=hard_radius-40.f;
}
enum class DestinationKind {Planet,Station,Fleet};
struct Destination {
    int level;
    const char* name;
    Vec3 position;
    float radius;
    DestinationKind kind=DestinationKind::Planet;
    const char* asset_id=nullptr;
};
const std::array<Destination,3>& prototype_catalog() noexcept;
const std::array<Destination,18>& galaxy_catalog() noexcept;
const Destination* find_destination(const Destination* entries,std::size_t count,int level) noexcept;
// Different native visits name the same celestial body. Native progress must
// resolve the actual landing program; this function does not unlock a level.
int celestial_body_for_level(int level) noexcept;
bool valid_catalog(const Destination* entries,std::size_t count) noexcept;
bool coordinates_for_body(const std::array<bool,19>& coordinates,int level) noexcept;
}

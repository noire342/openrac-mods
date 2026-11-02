// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "catalog.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace spacevoyages {
// Static collision geometry; rotation is cosmetic and must never move the
// sphere. The renderer chooses the original model indexed by variant.
struct Asteroid {
    Vec3 position{};
    float radius{};
    Vec3 rotation_axis{0,0,1};
    float angular_speed{}; // radians per second
    std::uint32_t variant{};
    float rotation_phase{}; // initial radians; visual angle=phase+speed*time
    std::uint32_t field_id{}; // stable authored chunk, never viewer-relative
};
struct AsteroidFieldDefinition {
    std::uint32_t id{},seed{};
    Vec3 center{},half_extents{};
    Vec3 passage_axis{1,0,0};
    float passage_radius{};
    std::size_t count{};
};
struct ProtectedFlightCorridor {Vec3 start{},end{};float radius{};};
struct AsteroidRadiusRange {float minimum{},maximum{};};
// Rendering/model ids stay outside core. Variant0 is the larger native rock,
// variant1 the smaller fragment; future catalogs may supply other intervals.
inline constexpr std::array<AsteroidRadiusRange,2> asteroid_default_radius_ranges{{{4.f,6.4f},{2.4f,4.5f}}};
struct AsteroidLayoutPolicy {
    // An orientation-independent empty envelope covers every existing intro
    // (radius+64) and radial approach (up to radius+60), with spare clearance.
    // The tests use the actual VoyageController; no flight coefficients enter
    // the generator. Future longer authored paths can supply extra capsules.
    float planet_clearance=90.f;
    float minimum_radius=2.4f,maximum_radius=6.4f;
    float separation=3.5f;
    float outer_radius=galaxy_bounds::soft_radius-40.f;
    std::uint32_t variants=2;
    const AsteroidRadiusRange* variant_radii=asteroid_default_radius_ranges.data();
    std::size_t variant_radius_count=asteroid_default_radius_ranges.size(); //0: uniform global interval
    std::size_t attempts_per_asteroid=64;
};
struct AsteroidFieldRange {
    std::uint32_t id{};
    std::size_t begin{},count{},requested{};
    Vec3 center{};
    float radius{}; // conservative static sphere for the entire chunk
};
inline constexpr std::size_t asteroid_field_count=8;
const std::array<AsteroidFieldDefinition,asteroid_field_count>& asteroid_field_catalog() noexcept;
// Pure geometry policy. Invalid inputs are refused; touching a protected
// volume is excluded. This function neither queries nor changes native state.
bool asteroid_outside_protected_volumes(const Asteroid& asteroid,
    const Destination* planets,std::size_t planet_count,
    const ProtectedFlightCorridor* corridors,std::size_t corridor_count,
    const AsteroidLayoutPolicy& policy) noexcept;
class AsteroidLayout {
public:
    const std::vector<Asteroid>& asteroids()const noexcept{return asteroids_;}
    const std::vector<AsteroidFieldRange>& fields()const noexcept{return fields_;}
    bool complete()const noexcept;
private:
    std::vector<Asteroid> asteroids_;
    std::vector<AsteroidFieldRange> fields_;
    friend AsteroidLayout build_asteroid_layout(const Destination*,std::size_t,
        const AsteroidFieldDefinition*,std::size_t,const AsteroidLayoutPolicy&,
        const ProtectedFlightCorridor*,std::size_t);
};
// Generation is explicit, bounded and independent of unlocks, disc version,
// frame count or the standard library's implementation-defined random engines.
// Throws invalid_argument for invalid specifications; crowded layouts return
// fewer objects and complete()==false instead of relaxing exclusion rules.
AsteroidLayout build_asteroid_layout(const Destination* planets,std::size_t planet_count,
    const AsteroidFieldDefinition* fields,std::size_t field_count,
    const AsteroidLayoutPolicy& policy={},
    const ProtectedFlightCorridor* corridors=nullptr,std::size_t corridor_count=0);
AsteroidLayout build_asteroid_layout(const Destination* planets,std::size_t planet_count,
    const AsteroidLayoutPolicy& policy={});

struct AsteroidSweepHit {
    std::size_t index{};
    float time{}; // earliest contact in the closed interval[0,1]
    Vec3 normal{1,0,0}; // outward from the asteroid at first contact
};
struct AsteroidQueryStats {std::size_t candidate_count{},occupied_cells_examined{};};
struct AsteroidQueryScratch {
    std::vector<std::size_t> candidates;
    std::vector<std::uint32_t> seen;
    std::uint32_t stamp{};
    // Call once alongside output.reserve(index.asteroids().size()). The
    // scratch overloads then allocate nothing, including stamp rollover.
    void reserve(std::size_t asteroid_count);
};
// Owns immutable positions/radii, so an index cannot dangle after a layout is
// destroyed or be invalidated by cosmetic rotation. Copy/move are safe.
class AsteroidSpatialIndex {
public:
    explicit AsteroidSpatialIndex(std::vector<Asteroid> asteroids,float cell_size=32.f);
    const std::vector<Asteroid>& asteroids()const noexcept{return asteroids_;}
    // Clears output. Returns false for invalid geometry; an empty valid result
    // returns true. Results are deterministic, unique and sorted by index or,
    // for a sweep, by earliest contact then index. No collision response.
    bool query_sphere(Vec3 center,float radius,std::vector<std::size_t>& hits,
        AsteroidQueryStats* stats=nullptr)const;
    bool sweep_sphere(Vec3 start,Vec3 end,float radius,std::vector<AsteroidSweepHit>& hits,
        AsteroidQueryStats* stats=nullptr)const;
    bool query_sphere(Vec3 center,float radius,std::vector<std::size_t>& hits,
        AsteroidQueryScratch& scratch,AsteroidQueryStats* stats=nullptr)const;
    bool sweep_sphere(Vec3 start,Vec3 end,float radius,std::vector<AsteroidSweepHit>& hits,
        AsteroidQueryScratch& scratch,AsteroidQueryStats* stats=nullptr)const;
private:
    struct Cell {int x{},y{},z{};bool operator==(const Cell& b)const noexcept{return x==b.x&&y==b.y&&z==b.z;}};
    struct CellHash {std::size_t operator()(const Cell&)const noexcept;};
    std::vector<Asteroid> asteroids_;
    float cell_size_{};
    std::unordered_map<Cell,std::vector<std::size_t>,CellHash> cells_;
    Cell cell(Vec3 point)const noexcept;
    void candidates(Vec3 lower,Vec3 upper,AsteroidQueryScratch&,AsteroidQueryStats*)const;
};
}

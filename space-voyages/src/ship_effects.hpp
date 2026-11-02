// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "flight.hpp"
#include "gl_api.hpp"
#include "ship_identity.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spacevoyages {
// Original sprite pixels, optionally tiled into an atlas in row-major order.
// Empty pixels mean absent. No generated substitute is required by this API.
struct EffectSprite {
    std::vector<std::uint8_t> rgba;
    int width{},height{};
    std::uint32_t columns=1,rows=1,frames=1;
    float fps{};
};
struct ExhaustMount {
    // Same local coordinates and scale as the baked ship model. Direction
    // points out of the nozzle; radius/length are local world-unit measures.
    Vec3 position{},direction{-1,0,0};
    float radius{},length{};
};
struct ShipExhaustMounts {
    static constexpr std::size_t capacity=16;
    std::array<ExhaustMount,capacity> mounts{};
    std::size_t count{};
    // Original GS modulation:128 is neutral, so bright RGB can exceed1.
    std::array<float,4> color{1,1,1,1};
};
struct ShipEffectsAssets {
    std::array<ShipExhaustMounts,ship_profile_count> ships{}; // stock0..2, Original3
    EffectSprite flame,hit,explosion;
};
namespace ship_effects_math {
using Matrix=std::array<float,16>;
struct FlameScale {float radius{},length{},intensity{};};
Vec3 transform_point(const Matrix&,Vec3)noexcept;
Vec3 transform_direction(const Matrix&,Vec3)noexcept;
FlameScale flame_scale(float boost_fraction,float presentation_time,float seed=0)noexcept;
}

// Context/thread ownership matches AsteroidRenderer. Assets are borrowed only
// during initialize. A legacy empty pack performs no GL lookup/allocation.
class ShipEffects final {
public:
    using Matrix=ship_effects_math::Matrix;
    struct Stats {unsigned draws{},thrusters{},sprites{},triangles{};};
    ShipEffects()=default;
    ~ShipEffects(){close();}
    ShipEffects(const ShipEffects&)=delete;
    ShipEffects& operator=(const ShipEffects&)=delete;
    ShipEffects(ShipEffects&&)=delete;
    ShipEffects& operator=(ShipEffects&&)=delete;
    void initialize(GlApi::GetProc,const ShipEffectsAssets&);
    // Model must be the exact interpolated matrix used for the hull. Time is
    // the owner's paused simulation time. -1 means normal ship; >=0 disables
    // exhaust and presents the optional native destruction sprite instead.
    // Depth test is on, depth writes off, additive blend. Caller restores its
    // following pass. Device errors propagate to the owner's scene guard.
    void draw(const Matrix& vp,const Matrix& ship_model,const CameraState&,
        int ship_id,float boost_fraction,float presentation_time,
        float hit_flash=0,float destruction_progress=-1);
    void draw(const Matrix& vp,const Matrix& ship_model,const CameraState&,
        ShipIdentity,float boost_fraction,float presentation_time,
        float hit_flash=0,float destruction_progress=-1);
    void close()noexcept;
    const Stats& last_stats()const noexcept{return stats_;}
    bool initialized()const noexcept{return flame_program_||sprite_program_;}
private:
    struct Uniforms {
        int vp{},model{},camera_right{},camera_up{},time_boost{},
            plume_params{},atlas{},atlas_texel{},effect{},effect_tint{};
    };
    struct SpriteGpu {unsigned texture{};int width{},height{},columns=1,rows=1,frames=1;float fps{};};
    struct MountGpu {unsigned vao{},vbo{};int count{};std::array<float,4> color{1,1,1,1};};
    GlApi gl_{};
    unsigned flame_program_{},sprite_program_{};
    unsigned flame_vbo_{},flame_ebo_{},quad_vao_{},quad_vbo_{},quad_ebo_{};
    int flame_indices_{};
    std::array<MountGpu,ship_profile_count> mounts_{};
    SpriteGpu flame_{},hit_{},explosion_{};
    Uniforms flame_uniforms_{},sprite_uniforms_{};
    Stats stats_{};
    unsigned shader(unsigned,const char*);
    unsigned program(const char*,const char*);
    SpriteGpu texture(const EffectSprite&);
};
}

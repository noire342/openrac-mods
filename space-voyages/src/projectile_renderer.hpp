// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ship_effects.hpp"
#include <string>

namespace spacevoyages {
struct MeshAsset;
class SpaceCombat;
// The original Gemlik laser mesh/texture is supplied separately. Empty native
// effect sprites mean absent, with no generated substitute. Scales are visual
// only; simulation range, collision and gun ownership belong to SpaceCombat.
struct ProjectileEffectsAssets {
    EffectSprite impact,explosion;
    std::string bolt_asset_id="projectile_laser";
    std::string fragment_asset_id="asteroid_1";
    std::array<float,3> bolt_scales{1,1,1}; // classes531,532,533
    // Declared visual adaptation: fit the original 3D laser to the muzzle at
    // birth, then expand it outside the barrel for readable distant fire.
    std::array<float,3> bolt_growth{1,1,1};
    float bolt_growth_distance=6;
};
namespace projectile_effects_math {
float bolt_scale(float muzzle_scale,float growth,float growth_distance,float travelled)noexcept;
// Offset from the moving projectile tip. The tail never reaches behind its
// birth plane; plasma still touching that plane retains the muzzle-fit radius.
Vec3 bolt_offset(Vec3 model_vertex,float tip_x,float muzzle_scale,float growth,float growth_distance,float travelled)noexcept;
}
// Same scene-thread/context ownership as ShipEffects. The caller restores GL
// state before HUD. No borrowed asset/pool pointers survive initialize/draw.
class ProjectileRenderer final {
public:
    using Matrix=std::array<float,16>;
    struct Stats {unsigned draws{},projectiles{},effects{},triangles{},instance_uploads{},fragments{};};
    ProjectileRenderer()=default;
    ~ProjectileRenderer(){close();}
    ProjectileRenderer(const ProjectileRenderer&)=delete;
    ProjectileRenderer& operator=(const ProjectileRenderer&)=delete;
    void initialize(GlApi::GetProc,const MeshAsset&,const ProjectileEffectsAssets&,const MeshAsset* fragment=nullptr);
    // Uses paused/interpolated simulation time. Birth data are uploaded only
    // when the fixed projectile pool or persistent event ring changes. Optional
    // original1412 debris is visual only and draws opaque before additive FX.
    void draw(const Matrix&,const CameraState&,const SpaceCombat&,int ship_class,double presentation_seconds);
    void draw(const Matrix&,const CameraState&,const SpaceCombat&,ShipIdentity,double presentation_seconds);
    void close()noexcept;
    bool initialized()const noexcept{return bolt_program_!=0;}
    const Stats& last_stats()const noexcept{return stats_;}
private:
    struct SpriteGpu {unsigned texture{};int width{},height{},columns=1,rows=1,frames=1;float fps{};};
    GlApi gl_{};
    unsigned bolt_program_{},effect_program_{};
    unsigned bolt_vao_{},bolt_vbo_{},bolt_ebo_{},bolt_instances_{},bolt_texture_{};
    unsigned effect_vao_{},effect_vbo_{},effect_ebo_{},effect_instances_{};
    unsigned fragment_program_{},fragment_vao_{},fragment_vbo_{},fragment_ebo_{},fragment_instances_{},fragment_texture_{};
    int fragment_indices_{},fragment_count_{};
    int bolt_indices_{},bolt_count_{},effect_count_{};
    float tip_{};
    double origin_seconds_{};
    std::array<float,3> scales_{1,1,1};
    std::array<float,3> growth_{1,1,1};
    float growth_distance_=6;
    std::uint64_t projectile_revision_{},event_revision_{},fragment_revision_{};
    bool pool_uploaded_{},events_uploaded_{},fragments_uploaded_{};
    SpriteGpu impact_{},explosion_{};
    struct Uniforms {int vp{},time{},scale_tip{},camera_right{},camera_up{},atlas{},atlas_texel{},effect_kind{},wrap{};} bolt_u_{},effect_u_{},fragment_u_{};
    std::array<float,4> texture_wrap_{};
    std::array<float,4> fragment_wrap_{};
    Stats stats_{};
    unsigned shader(unsigned,const char*);
    unsigned program(const char*,const char*);
    SpriteGpu texture(const EffectSprite&);
};
}

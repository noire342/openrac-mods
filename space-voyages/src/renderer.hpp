// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "assets.hpp"
#include "gl_api.hpp"
#include "voyage.hpp"
#include "asteroid_renderer.hpp"
#include "ship_effects.hpp"
#include <array>
#include <unordered_map>
namespace spacevoyages {
class SceneRenderer final {
public:
 struct Stats {
  unsigned draws{},triangles{},artificial_draws{},asteroid_draws{},asteroid_instances{},asteroid_culled_batches{},exhaust_draws{},thrusters{},effect_sprites{},projectile_draws{},projectiles{},impact_sprites{};
  int prompt_candidate=-1;unsigned prompt_glyphs{},font_texture{};
  float prompt_x{},prompt_y{},prompt_width{},prompt_alpha{};
 };
 explicit SceneRenderer(const SceneAssets& assets):assets_(assets){}
 void initialize(GlApi::GetProc loader,const AsteroidLayout* asteroids=nullptr,bool engine_flames=true);
 void draw(const VoyageController&,const FlightState& presentation,int width,int height,int ship,bool paused,float entry_progress,float approach_progress,float scene_seconds=0.f,const ShipDamageState* damage=nullptr);
 void draw(const VoyageController&,const FlightState& presentation,int width,int height,ShipIdentity,bool paused,float entry_progress,float approach_progress,float scene_seconds=0.f,const ShipDamageState* damage=nullptr);
 bool supports_ship(ShipIdentity)const noexcept;
 void close();
 const Stats& last_stats()const noexcept{return stats_;}
private:
 Stats stats_{};
 AsteroidRenderer asteroids_;
 ShipEffects ship_effects_;
 ProjectileRenderer projectiles_;
 struct GpuMesh {std::string id;unsigned vao{},vbo{},ebo{},texture{};int count{};AlphaMode alpha=AlphaMode::Opaque;float alpha_cutoff=.5f;Vec3 center{};bool atlas{},sphere_uv{},metal_overlay{};unsigned wrap_s=33071,wrap_t=33071;};
 struct GpuGroup {std::vector<std::size_t> opaque,overlays,blend;};
 const SceneAssets& assets_;GlApi gl_;std::vector<GpuMesh> meshes_;
 std::unordered_map<std::string,GpuGroup> groups_;
 std::vector<unsigned> mesh_textures_;
 unsigned program_{},ui_vao_{},ui_vbo_{},font_{},white_{},sky_{},radar_{},health_indices_{},health_palette_{};
 std::array<unsigned,3> reticle_{};
 int health_mode_{},health_threshold_{};
 int mvp_{},model_{},view_{},sphere_uv_{},tint_{},ambient_{},alpha_mode_{},alpha_cutoff_{},atlas_mode_{},wrap_s_{},wrap_t_{};
 float ui_width_=512;
 void mesh(const GpuMesh&,const std::array<float,16>& vp,const std::array<float,16>& model,float ambient=0.45f);
 void group(const std::string&,const std::array<float,16>& vp,const std::array<float,16>& model,float ambient,Vec3 eye);
 void triangles(const std::vector<Vertex>&,unsigned texture,const std::array<float,4>& color);
 void quad(float x,float y,float w,float h,const std::array<float,4>& color,unsigned texture=0,float u=0,float v=0,float uw=1,float vh=1);
 void text(float x,float y,const std::string&,float pixels=14,const std::array<float,4>& color={.63f,.73f,1,1});
 void circle(float x,float y,float radius,const std::array<float,4>& color,bool fill=false);
 float text_width(const std::string&,float pixels)const;
 const GpuMesh& get(const std::string&)const;
 unsigned texture(const unsigned char*,int,int);
 unsigned shader(unsigned,const char*);
};
}

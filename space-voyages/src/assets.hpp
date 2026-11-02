// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ship_effects.hpp"
#include "space_combat.hpp"
#include "projectile_renderer.hpp"
#include "scene_audio.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace spacevoyages {
struct Vertex {float position[3],normal[3],uv[2];float color[4]={1,1,1,1};float atlas_rect[4]={1,1,0,0};};
static_assert(sizeof(Vertex)==64,"Scene vertex layout is part of the asset contract");
enum class AlphaMode {Opaque,Mask,Blend};
struct MeshAsset {
 std::string id;std::vector<Vertex> vertices;std::vector<unsigned> indices;std::vector<unsigned char> rgba;int width{},height{};
 unsigned wrap_s=33071,wrap_t=33071;AlphaMode alpha=AlphaMode::Opaque;float alpha_cutoff=.5f;
 std::string texture_key;bool atlas{};
 // Explicit native metal/glass semantic; ordinary blended materials keep UVs.
 bool sphere_uv{};
 // Native chrome overlays repeat an actor's already opaque surface. Glass
 // shells keep their ordinary transparent pass and never get this semantic.
 bool metal_overlay{};
};
struct FontGlyph {int u{},v{},top{},advance{};};
struct IndexedEffectSprite {
 std::vector<unsigned char> indices,palette_rgba;
 int width{},height{};
};
struct OriginalShipAsset {
 // Optional in legacy packs. Availability is published only after validating
 // the whole identity/pose/material/bounds/two-nozzle contract.
 bool available{};
 // Version2 additionally validates both independently baked cockpit actors.
 bool cockpit_available{};
 std::string mesh_group="ship_original";
 float radius{},length{};
};
struct SceneAssets {
 std::vector<MeshAsset> meshes;
 std::vector<unsigned char> font_rgba;
 std::vector<unsigned char> sky_rgba;
 std::vector<unsigned char> radar_rgba;
 IndexedEffectSprite health_meter;
 ShipEffectsAssets ship_effects;
 SpaceCombatAssets combat;
 ProjectileEffectsAssets projectile_effects;
 std::array<EffectSprite,3> reticle; // original ring, marks, centre
 SceneAudioAssets audio;
 bool audio_available{};
 OriginalShipAsset original_ship;
 FontGlyph glyphs[128]{};
 int font_width=256,font_height=256;
 bool full_galaxy{};
 static SceneAssets read(const std::filesystem::path& directory);
 const MeshAsset& get(const std::string& id)const;
 bool has(const std::string& id)const noexcept;
};
}

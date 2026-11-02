// SPDX-License-Identifier: GPL-3.0-or-later
#include "assets.hpp"
#include <cstdio>
#include <stdexcept>
#include <array>
#include <cmath>
#include "catalog.hpp"
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  const auto assets=spacevoyages::SceneAssets::read(argv[1]);
  if(assets.meshes.size()<6||assets.glyphs['A'].advance<=0||assets.sky_rgba.size()!=2048*1024*4)throw std::runtime_error("Missing scene resources");
  std::array<float,3> ship_lo{10000,10000,10000},ship_hi{-10000,-10000,-10000};bool native_colours=false;
  for(const auto& m:assets.meshes){
   if(m.indices.empty()||m.indices.size()%3||m.vertices.empty())throw std::runtime_error("Invalid triangle mesh");
   float lo[3]={10000,10000,10000},hi[3]={-10000,-10000,-10000};
   for(const auto& v:m.vertices)for(unsigned i=0;i<3;++i){if(v.position[i]<lo[i])lo[i]=v.position[i];if(v.position[i]>hi[i])hi[i]=v.position[i];}
   for(unsigned ship=0;ship<3;++ship)if(m.id.find("ship_"+std::to_string(531+ship))==0){ship_lo[ship]=std::min(ship_lo[ship],lo[0]);ship_hi[ship]=std::max(ship_hi[ship],hi[0]);}
   if(m.id.find("station_")==0||m.id.find("fleet_")==0)for(const auto& v:m.vertices){
    const float radius=std::sqrt(v.position[0]*v.position[0]+v.position[1]*v.position[1]+v.position[2]*v.position[2]);if(radius>1.0001f)throw std::runtime_error("Artificial destination escaped its common bounding sphere");
    if(v.color[0]!=1||v.color[1]!=1||v.color[2]!=1)native_colours=true;
   }
  }
  for(unsigned ship=0;ship<3;++ship)if(std::abs(ship_hi[ship]-ship_lo[ship]-2.5f)>.001f)throw std::runtime_error("Combined ship scale/orientation contract changed");
  if(assets.full_galaxy){for(const auto& d:spacevoyages::galaxy_catalog())assets.get(d.asset_id);if(!native_colours)throw std::runtime_error("Native structure vertex colours were lost");}
  if(assets.has("asteroid_0")||assets.has("asteroid_1")){
   constexpr unsigned original_triangles[2]={3584,522};
   for(unsigned i=0;i<2;++i){const auto& rock=assets.get("asteroid_"+std::to_string(i));
    if(rock.indices.size()!=original_triangles[i]*3||!rock.atlas||rock.alpha!=spacevoyages::AlphaMode::Opaque)
     throw std::runtime_error("Original Gemlik asteroid geometry/material contract changed");
    for(const auto& v:rock.vertices)if(v.position[0]*v.position[0]+v.position[1]*v.position[1]+v.position[2]*v.position[2]>1.0002f)
     throw std::runtime_error("Asteroid geometry escaped its collision/rotation envelope");
   }
   if(assets.get("asteroid_0").texture_key!=assets.get("asteroid_1").texture_key)
    throw std::runtime_error("Asteroid variants lost their shared native texture atlas");
  }
  if(!assets.health_meter.indices.empty()){
   const auto& health=assets.health_meter;
   if(health.width!=128||health.height!=128||health.indices.size()!=128*128||health.palette_rgba.size()!=256*4)
    throw std::runtime_error("Original Gemlik health arc indexed palette contract changed");
   bool colored=false;for(unsigned i=2;i<254;++i)if(health.palette_rgba[i*4]||health.palette_rgba[i*4+1]||health.palette_rgba[i*4+2])colored=true;
   if(!colored)throw std::runtime_error("Original health arc lost its native colored palette");
  }
  if(!assets.ship_effects.flame.rgba.empty())for(std::size_t ship_index=0;ship_index<assets.ship_effects.ships.size();++ship_index){
   if(ship_index==3&&!assets.original_ship.available)continue;
   const auto& ship=assets.ship_effects.ships[ship_index];
   if(!ship.count||ship.count>ship.capacity)throw std::runtime_error("Original ship variant has no exhaust mounts");
   for(std::size_t i=0;i<ship.count;++i){const auto& mount=ship.mounts[i];
    if(mount.direction.x>-.1f||std::abs(spacevoyages::length_squared(mount.direction)-1.f)>.002f||spacevoyages::length_squared(mount.position)>9||mount.radius<=0||mount.length<=0)
     throw std::runtime_error("Native ship exhaust geometry escaped the baked coordinate contract");
   }
  }
  if(assets.original_ship.available){
   if(!assets.has("ship_original")||!assets.get("ship_original_part1").sphere_uv||assets.ship_effects.ships.at(3).count!=2||
      assets.original_ship.radius>1.450001f||assets.original_ship.length<=0)
    throw std::runtime_error("Independent Original visual profile was lost");
  }
  if(std::filesystem::exists(std::filesystem::path(argv[1])/"original-cockpit-extension.json")){
   unsigned parts=0,overlays=0;for(const auto& mesh:assets.meshes)if(mesh.id.find("ship_original")==0){++parts;if(mesh.metal_overlay)++overlays;}
   if(!assets.original_ship.cockpit_available||parts!=9||overlays!=1||!assets.get("ship_original_part8").metal_overlay||assets.get("ship_original_part1").metal_overlay)
    throw std::runtime_error("Captured Original cockpit or its independent chrome overlay was lost");
  }
  std::puts("All scene meshes, font, background and ship orientations are valid");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

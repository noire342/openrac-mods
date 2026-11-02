// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "space_combat.hpp"
#include "render_math.hpp"
namespace spacevoyages {
struct ReticlePlacement {float x{},y{};bool visible{};};
inline ReticlePlacement weapon_reticle(const render_math::Matrix& vp,const FlightState& ship,
    const SpaceCombatAssets& assets,ShipIdentity identity,float ui_width)noexcept {
 const auto* profile=ship_profile(identity);
 if(!profile||!profile->weapons_allowed||!std::isfinite(ui_width)||ui_width<=0)return {};
 const auto& weapon=assets.ships[profile->slot];
 if(!weapon.armed||!weapon.count||weapon.count>weapon.muzzles.size())return {};
 Vec3 origin{},aim{};
 for(std::size_t i=0;i<weapon.count;++i){origin=origin+weapon.muzzles[i].position;aim=aim+weapon.muzzles[i].direction;}
 origin=weapon_local_to_world(origin/float(weapon.count),ship,true);
 const Vec3 velocity=weapon_local_to_world(unit(aim),ship,false)*assets.projectile_speed+direction(ship.yaw,ship.pitch)*ship.speed;
 // Native Gemlik HUD projects23 units along the weapon's aim. Adapt only
 // the start to the actual pair of muzzle ports on these cinematic hulls.
 const Vec3 p=origin+unit(velocity)*23.f;
 const float w=vp[3]*p.x+vp[7]*p.y+vp[11]*p.z+vp[15];
 if(!std::isfinite(w)||w<=.00001f)return {};
 const float x=(vp[0]*p.x+vp[4]*p.y+vp[8]*p.z+vp[12])/w;
 const float y=(vp[1]*p.x+vp[5]*p.y+vp[9]*p.z+vp[13])/w;
 const float z=(vp[2]*p.x+vp[6]*p.y+vp[10]*p.z+vp[14])/w;
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::abs(x)>1||std::abs(y)>1||z< -1||z>1)return {};
 return {(x*.5f+.5f)*ui_width,(.5f-y*.5f)*416.f,true};
}
inline ReticlePlacement weapon_reticle(const render_math::Matrix& vp,const FlightState& ship,
    const SpaceCombatAssets& assets,int native_class,float ui_width)noexcept {
 const auto identity=ship_identity_from_native_class(native_class);
 return identity?weapon_reticle(vp,ship,assets,*identity,ui_width):ReticlePlacement{};
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "reticle.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {
unsigned checks;
void require(bool okay,const char* why){++checks;if(!okay){std::fprintf(stderr,"reticle: %s\n",why);std::exit(1);}}
}
int main(){
 // Independent orthographic screen: +X is depth, +Y screen-left, +Z up.
 const render_math::Matrix vp{0,0,.01f,0,-.01f,0,0,0,0,.01f,0,0,0,0,0,1};
 SpaceCombatAssets assets;
 for(auto& weapon:assets.ships){weapon.armed=true;weapon.count=2;weapon.muzzles[0]={{.5f,.4f,.2f},{1,0,0}};weapon.muzzles[1]={{.5f,-.4f,.2f},{1,0,0}};}
 FlightController flight;flight.reset({});const auto ship=flight.state();
 for(float aspect:{4.f/3,16.f/9,16.f/10})for(int type:{532,533}){
  const float width=416*aspect;const auto aim=weapon_reticle(vp,ship,assets,type,width);
  require(aim.visible&&std::abs(aim.x-width/2)<.0001f,"symmetric gun pair projects to the same horizontal aim at every supported aspect");
  require(std::abs(aim.y-207.584f)<.0001f,"actual muzzle height projects upward with the renderer's Y convention");
  auto rolled=ship;rolled.model_roll=pi*.5f;const auto bank=weapon_reticle(vp,rolled,assets,type,width);
  require(bank.visible&&bank.x>width/2&&std::abs(bank.y-208)<.0001f,"muzzle midpoint follows native bank rather than remaining at screen centre");
 }
 require(!weapon_reticle(vp,ship,assets,531,600).visible,"green ship has no aiming reticle even if metadata tries to arm it");
 auto behind=vp;behind[15]=-1;require(!weapon_reticle(behind,ship,assets,532,600).visible,"aim behind camera cannot wrap onto the HUD");
 auto outside=vp;outside[12]=2;require(!weapon_reticle(outside,ship,assets,532,600).visible,"offscreen aim is not clamped into a misleading centre reticle");
 require(!weapon_reticle(vp,ship,assets,532,0).visible&&!weapon_reticle(vp,ship,assets,999,600).visible,"invalid viewport and unsupported ship remain hidden");
 assets.ships[1].armed=false;require(!weapon_reticle(vp,ship,assets,532,600).visible,"disabled gun metadata hides the weapon-only HUD");
 std::printf("Reticle native projection and ownership checks:%u passed\n",checks);
}

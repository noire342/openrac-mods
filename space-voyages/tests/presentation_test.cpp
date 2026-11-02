// SPDX-License-Identifier: GPL-3.0-or-later
#include "flight_presentation.hpp"
#include "render_math.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {
int checks;
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"presentation: %s\n",why);std::exit(1);}}
Vec3 transform(const render_math::Matrix& m,Vec3 v){return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};}
}
int main(){
 // All moving layers must use the same render instant. At high refresh the
 // hull must neither freeze for alternate refreshes nor move against a camera
 // sampled at the other end of the physics tick.
 for(float speed:{24.f,30.f})for(float yaw:{-3.13f,-1.2f,0.f,3.13f})for(float pitch:{-1.39f,0.f,1.39f}){
  FlightController controller;controller.reset({920,-830,640},yaw,pitch);
  FlightState a=controller.state(),b=a;
  const Vec3 displacement=direction(yaw,pitch)*(speed/60.f);
  b.position=b.position+displacement;b.camera.position=b.camera.position+displacement;
  const auto expected=transform(render_math::view(a.camera.position,a.camera.yaw,a.camera.pitch,a.camera.roll),a.position);
  float traveled=-1;
  for(int i=0;i<=40;++i){
   const float alpha=i/40.f;const auto sample=presentation::interpolate(a,b,alpha);
   const auto relative=transform(render_math::view(sample.camera.position,sample.camera.yaw,sample.camera.pitch,sample.camera.roll),sample.position);
   require(length(relative-expected)<.001f,"hull and chase camera stay at the same interpolated instant, including turbo");
   const float distance=dot(sample.position-a.position,unit(displacement));
   require(distance+1e-5f>=traveled,"interpolated forward flight never steps backwards");traveled=distance;
   require(length(sample.position-lerp(a.position,b.position,alpha))<.0001f,"rendering interpolates rather than snapping to a physics endpoint");
  }
 }
 FlightState a{},b{};a.yaw=a.model_yaw=a.camera.yaw=pi-.02f;b.yaw=b.model_yaw=b.camera.yaw=-pi+.02f;
 a.model_roll=a.camera.roll=pi-.04f;b.model_roll=b.camera.roll=-pi+.04f;
 a.camera.fov=1.2f;b.camera.fov=1.4f;
 const auto middle=presentation::interpolate(a,b,.5f);
 require(std::abs(wrap_angle(middle.yaw-pi))<1e-6f&&std::abs(wrap_angle(middle.model_yaw-pi))<1e-6f&&std::abs(wrap_angle(middle.camera.yaw-pi))<1e-6f,"hull and camera yaw interpolate across the wrap by the short arc");
 require(std::abs(wrap_angle(middle.model_roll-pi))<1e-6f&&std::abs(wrap_angle(middle.camera.roll-pi))<1e-6f,"bank also uses the short arc");
 require(std::abs(middle.camera.fov-1.3f)<1e-6f,"turbo FOV shares the same fractional time");
 require(presentation::interpolate(a,b,-1).camera.fov==a.camera.fov&&presentation::interpolate(a,b,2).camera.fov==b.camera.fov,"render alpha is clamped without extrapolation");
 require(presentation::interpolate(a,b,NAN).camera.fov==b.camera.fov,"invalid alpha safely chooses the current complete frame");
 for(float yaw:{-3.1f,-.5f,0.f,1.8f,3.1f})for(float pitch:{-1.39f,0.f,1.39f}){
  FlightController flight;flight.reset({20,-90,70},yaw,pitch);const auto& ship=flight.state();
  for(int i=0;i<=120;++i){
   const float p=i/120.f;const auto camera=presentation::space_entry_camera(ship,p);
   require(finite(camera.position)&&std::isfinite(camera.yaw)&&std::isfinite(camera.pitch),"cinematic camera stays finite near both pitch limits");
   const auto screen=transform(render_math::view(camera.position,camera.yaw,camera.pitch,camera.roll),ship.position);
   require(screen.z<-2&&std::abs(screen.x/-screen.z)<.5f&&std::abs(screen.y/-screen.z)<.5f,"ship remains in the establishing shot throughout the camera move");
  }
  const auto end=presentation::space_entry_camera(ship,1);
  require(length(end.position-ship.camera.position)==0&&end.yaw==ship.camera.yaw&&end.pitch==ship.camera.pitch&&end.roll==ship.camera.roll&&end.fov==ship.camera.fov,"cinematic ends at the exact native chase camera, with no one-frame cut");
 }
 require(presentation::entry_black(0)==1&&presentation::entry_black(1)==0,"space entry opens from black");
 require(presentation::approach_black(0)==0&&presentation::approach_black(1)==1,"native arrival receives a fully black final frame");
 require(presentation::entry_hud(0)==0&&presentation::entry_hud(1)==1,"native radar fades in before player control");
 float last=0;for(int i=0;i<=120;++i){const auto value=presentation::approach_black(i/120.f);require(value>=last&&value>=0&&value<=1,"approach fade is monotonic and bounded");last=value;}
 std::printf("Presentation: %d camera continuity, framing and transition checks passed\n",checks);
}

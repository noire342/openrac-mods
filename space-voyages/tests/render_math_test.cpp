// SPDX-License-Identifier: GPL-3.0-or-later
#include "render_math.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {
void require(bool ok,const char* why){if(!ok){std::fprintf(stderr,"%s\n",why);std::exit(1);}}
Vec3 transform(const render_math::Matrix& m,Vec3 v){return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};}
// Independent literal rotation sequence from native FUN_001fa030: Rx then Ry
// then Rz. Not a second call to the production forward/right basis helper.
Vec3 native_rotation(Vec3 v,float yaw,float pitch,float roll){
 const Vec3 x{v.x,std::cos(roll)*v.y-std::sin(roll)*v.z,std::sin(roll)*v.y+std::cos(roll)*v.z};
 const Vec3 y{std::cos(pitch)*x.x+std::sin(pitch)*x.z,x.y,-std::sin(pitch)*x.x+std::cos(pitch)*x.z};
 return {std::cos(yaw)*y.x-std::sin(yaw)*y.y,std::sin(yaw)*y.x+std::cos(yaw)*y.y,y.z};
}
}
int main(){
 unsigned checks=0;
 for(float yaw:{-3.13f,-1.2f,0.f,.7f,3.13f})for(float pitch:{-3.13f,-1.90f,-pi/2,-1.39f,-.3f,0.f,.4f,1.39f,pi/2,1.90f,3.13f})for(float bank:{-1.4f,-.4f,0.f,.4f,1.4f}){
  const auto m=render_math::model({},yaw,pitch,bank,1);
  for(Vec3 v: {Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}}){require(length(transform(m,v)-native_rotation(v,yaw,pitch,bank))<.00001f,"Hull Euler differs from native Rz*Ry*Rx");++checks;}
  const Vec3 eye{3,-7,11};const auto camera=render_math::view(eye,yaw,pitch,bank);
  const auto f=native_rotation({1,0,0},yaw,pitch,bank),left=native_rotation({0,1,0},yaw,pitch,bank),up=native_rotation({0,0,1},yaw,pitch,bank);
  require(length(transform(camera,eye+f)-Vec3{0,0,-1})<.00001f,"Camera looks opposite to flight heading");
  require(length(transform(camera,eye+left)-Vec3{-1,0,0})<.00001f,"Camera and hull bank in opposite directions");
  require(length(transform(camera,eye+up)-Vec3{0,1,0})<.00001f,"Camera up differs from native banked up");checks+=3;
 }
 const auto right_bank=render_math::model({},0,0,.4f,1);
 require(transform(right_bank,{0,1,0}).z>0&&transform(right_bank,{0,-1,0}).z<0,"Right turn must raise left wing, not lower it");
 const render_math::Matrix cube{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 require(render_math::sphere_visible(cube,{0,0,0},.2f)&&render_math::sphere_visible(cube,{1.1f,0,0},.2f),"Frustum keeps inside and intersecting bodies");
 require(!render_math::sphere_visible(cube,{1.3f,0,0},.2f)&&!render_math::sphere_visible(cube,{0,0,-1.3f},.2f),"Frustum omits only completely outside bodies");checks+=2;
 std::printf("%u native Euler, direction and bank projection checks passed\n",checks+1);
}

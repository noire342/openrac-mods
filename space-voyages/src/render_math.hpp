// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <array>
namespace spacevoyages::render_math {
using Matrix=std::array<float,16>;
inline Matrix model(Vec3 position,float yaw,float pitch,float roll,float scale){
 const auto f=direction(yaw,pitch);
 // The native cosmetic pitch can exceed90 degrees even while the flight
 // heading is clamped below it. A cross with world-up flips at that pole;
 // the native Euler lateral axis remains continuous and depends only on yaw.
 const Vec3 right{std::sin(yaw),-std::cos(yaw),0};const auto up=cross(right,f);
 // Native Euler rows are axis images of Rz*Ry*Rx. In a forward+X world,
 // right is -Y: rotating a RIGHT vector with a LEFT-vector formula reversed
 // bank in the first prototype. Keep this basis shared by camera and hull.
 const auto r=right*std::cos(roll)-up*std::sin(roll),u=up*std::cos(roll)+right*std::sin(roll);
 return {f.x*scale,f.y*scale,f.z*scale,0,-r.x*scale,-r.y*scale,-r.z*scale,0,u.x*scale,u.y*scale,u.z*scale,0,position.x,position.y,position.z,1};
}
inline Matrix view(Vec3 eye,float yaw,float pitch,float roll){
 const auto m=model({},yaw,pitch,roll,1);
 const Vec3 f{m[0],m[1],m[2]},r{-m[4],-m[5],-m[6]},u{m[8],m[9],m[10]};
 return {r.x,u.x,-f.x,0,r.y,u.y,-f.y,0,r.z,u.z,-f.z,0,-dot(r,eye),-dot(u,eye),dot(f,eye),1};
}
inline bool sphere_visible(const Matrix& vp,Vec3 center,float radius){
 for(int row=0;row<3;++row)for(float sign:{-1.f,1.f}){
  const Vec3 n{vp[3]+sign*vp[row],vp[7]+sign*vp[4+row],vp[11]+sign*vp[8+row]};
  const float d=vp[15]+sign*vp[12+row];
  if(dot(n,center)+d < -radius*length(n))return false;
 }
 return true;
}
}

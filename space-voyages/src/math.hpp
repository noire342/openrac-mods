// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace spacevoyages {
inline constexpr float pi=3.14159265358979323846f;
struct Vec3 {float x{},y{},z{};};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator-(Vec3 a){return {-a.x,-a.y,-a.z};}
inline Vec3 operator*(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline Vec3 operator*(float s,Vec3 a){return a*s;}
inline Vec3 operator/(Vec3 a,float s){return a*(1.f/s);}
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length_squared(Vec3 a){return dot(a,a);}
inline float length(Vec3 a){return std::sqrt(length_squared(a));}
inline bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline Vec3 unit(Vec3 a,Vec3 fallback={1,0,0}){const float d=length(a);return d>1e-6f&&std::isfinite(d)?a/d:fallback;}
inline float wrap_angle(float a){return std::remainder(a,2.f*pi);}
inline float angle_lerp(float a,float b,float t){return wrap_angle(a+wrap_angle(b-a)*t);}
inline Vec3 lerp(Vec3 a,Vec3 b,float t){return a+(b-a)*t;}
inline Vec3 direction(float yaw,float pitch){return {std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),-std::sin(pitch)};}
inline float pitch_of(Vec3 d){return -std::atan2(d.z,std::sqrt(d.x*d.x+d.y*d.y));}
inline float yaw_of(Vec3 d){return std::atan2(d.y,d.x);}
}

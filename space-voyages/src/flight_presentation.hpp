// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "flight.hpp"
namespace spacevoyages::presentation {
// Simulation is native60Hz; a display can present between those ticks. Keep
// hull, camera and radar in the same interpolated frame instead of showing a
// new hull against a camera from another instant. No extrapolation or new
// physics is introduced here.
inline FlightState interpolate(const FlightState& previous,const FlightState& current,float alpha){
 const float t=std::isfinite(alpha)?std::clamp(alpha,0.f,1.f):1.f;
 if(t==0.f)return previous;if(t==1.f)return current;
 const auto scalar=[&](float a,float b){return a+(b-a)*t;};
 FlightState result=current;
 result.position=lerp(previous.position,current.position,t);
 result.yaw=angle_lerp(previous.yaw,current.yaw,t);
 result.pitch=angle_lerp(previous.pitch,current.pitch,t);
 result.roll=angle_lerp(previous.roll,current.roll,t);
 result.speed=scalar(previous.speed,current.speed);
 result.model_yaw=angle_lerp(previous.model_yaw,current.model_yaw,t);
 result.model_pitch=angle_lerp(previous.model_pitch,current.model_pitch,t);
 result.model_roll=angle_lerp(previous.model_roll,current.model_roll,t);
 result.boost_fraction=scalar(previous.boost_fraction,current.boost_fraction);
 result.camera.position=lerp(previous.camera.position,current.camera.position,t);
 result.camera.yaw=angle_lerp(previous.camera.yaw,current.camera.yaw,t);
 result.camera.pitch=angle_lerp(previous.camera.pitch,current.camera.pitch,t);
 result.camera.roll=angle_lerp(previous.camera.roll,current.camera.roll,t);
 result.camera.fov=scalar(previous.camera.fov,current.camera.fov);
 return result;
}
inline float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6.f-15.f)+10.f);}
// Presentation only: fly the actual ship forward under the voyage controller,
// and settle a rear-quarter establishing shot onto its native chase camera.
inline CameraState space_entry_camera(const FlightState& ship,float progress){
 const float blend=smooth(progress);
 if(blend>=1.f)return ship.camera;
 const auto forward=direction(ship.yaw,ship.pitch);
 const Vec3 left{-std::sin(ship.yaw),std::cos(ship.yaw),0};const auto up=cross(forward,left);
 const auto eye=ship.position-forward*7.5f+left*2.8f+up*1.8f;
 const auto look=unit(ship.position+forward*1.5f-eye);
 CameraState camera=ship.camera;camera.position=lerp(eye,ship.camera.position,blend);
 camera.yaw=angle_lerp(yaw_of(look),ship.camera.yaw,blend);
 camera.pitch=angle_lerp(pitch_of(look),ship.camera.pitch,blend);
 camera.roll=ship.camera.roll*blend;return camera;
}
inline float entry_black(float progress){return 1.f-smooth(progress/.28f);}
inline float approach_black(float progress){return smooth((progress-.7f)/.3f);}
inline float entry_hud(float progress){return smooth((progress-.55f)/.45f);}
}

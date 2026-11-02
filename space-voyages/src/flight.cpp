// SPDX-License-Identifier: GPL-3.0-or-later
// Gemlik flight equations adapted from OpenRAC/rac1-decomp, GPL-3.0-or-later.
#include "flight.hpp"
#include <algorithm>
#include <cmath>

namespace spacevoyages {
namespace {
// FUN_L00_0025b798 / 0025b8c0 / 0025bc98: native damped velocity,
// scalar spring and wrapped angular spring, respectively.
void damp(float& velocity,float error,float gain,float damping,float limit){
    velocity+=(gain*error-damping*velocity);
    if(limit>0)velocity=std::clamp(velocity,-limit,limit);
    velocity=std::clamp(velocity,-std::abs(error),std::abs(error));
}
void spring(float& value,float& velocity,float target,float gain,float damping,float limit,bool native_snap=true){
    damp(velocity,target-value,gain,damping,limit);value+=velocity;
    if(native_snap&&std::abs(target-value)<limit*.01f){value=target;velocity=0;}
}
void spring_angle(float& value,float& velocity,float target,float gain,float damping,float limit,bool native_snap=true){
    damp(velocity,wrap_angle(target-value),gain,damping,limit);value=wrap_angle(value+velocity);
    if(native_snap&&std::abs(wrap_angle(target-value))<limit*.01f){value=target;velocity=0;}
}
float axis(float v){return std::isfinite(v)?std::clamp(v,-1.f,1.f):0.f;}
}
void FlightController::reset(Vec3 position,float yaw,float pitch){
    state_={};state_.position=finite(position)?position:Vec3{};
    state_.yaw=std::isfinite(yaw)?wrap_angle(yaw):0.f;
    state_.pitch=std::isfinite(pitch)?std::clamp(pitch,-pitch_limit,pitch_limit):0.f;
    state_.speed=cruise_speed;state_.model_yaw=state_.yaw;state_.model_pitch=state_.pitch;
    native_motion_={};visual_motion_={};
    accumulator_=0;update_camera(state_,native_motion_,true,true);
    presentation_=state_;update_camera(presentation_,visual_motion_,true,false);
}
void FlightController::tick(double seconds,const FlightInput& input){
    if(!std::isfinite(seconds)||seconds<=0)return;
    // A long OS stall cannot fling the ship through the galaxy on resuming.
    accumulator_+=std::min(seconds,.25);
    constexpr double step_seconds=1.0/60.0;
    while(accumulator_+1e-10>=step_seconds){native_tick(input);accumulator_-=step_seconds;}
}
void FlightController::native_tick(const FlightInput& input,StepConstraint constraint,void* user){
    const Vec3 previous_position=state_.position;
    // Native stores displacement-per-frame, not units/second. Preserve the
    // exact coefficient arithmetic at frame_scale=frame_scale_sq=1.
    float displacement=state_.speed/60.f;
    if(input.boost)displacement=std::min(displacement+.035f,boost_speed/60.f);
    else {
        constexpr float cruise=cruise_speed/60.f;
        if(displacement<cruise-.025f)displacement+=.025f;
        else if(displacement>cruise)displacement+=(cruise-displacement)*.1f;
        else displacement=cruise;
    }
    state_.speed=displacement*60.f;
    advance_steering(native_motion_,input,true);advance_bank(state_,native_motion_,true);
    state_.pitch=std::clamp(wrap_angle(state_.pitch-native_motion_.steer_y*.012271847f),-pitch_limit,pitch_limit);
    state_.yaw=wrap_angle(state_.yaw-native_motion_.steer_x*.012271847f);
    state_.position=state_.position+direction(state_.yaw,state_.pitch)*displacement;
    state_.boost_fraction=std::clamp((displacement-.4f)/(.5f-.4f),0.f,1.f);
    if(constraint)constraint(user,*this,previous_position);
    publish_model(state_,native_motion_);
    update_camera(state_,native_motion_,false,true);++state_.ticks;
    update_presentation(input);
}
void FlightController::advance_steering(PresentationMotion& motion,const FlightInput& input,bool native_snap){
    spring(motion.steer_x,motion.steer_x_velocity,axis(input.horizontal),.03f,1.f,.1f,native_snap);
    spring(motion.steer_y,motion.steer_y_velocity,axis(input.vertical),.03f,1.f,.25f,native_snap);
}
void FlightController::advance_bank(FlightState& state,PresentationMotion& motion,bool native_snap){
    spring_angle(state.roll,motion.roll_velocity,motion.steer_x,.05f,1.f,1.f,native_snap);
}
void FlightController::publish_model(FlightState& state,const PresentationMotion& motion){
    state.model_roll=wrap_angle(state.roll+motion.steer_x*.5f);
    state.model_pitch=wrap_angle(state.pitch-motion.steer_y*.5f);
    state.model_yaw=state.yaw;
}
void FlightController::update_presentation(const FlightInput& input){
    const auto camera=presentation_.camera;const float roll=presentation_.roll;
    // All physical fields are copied after this tick's geometry constraints.
    // Only the independent cosmetic/camera spring channels retain history.
    presentation_=state_;presentation_.camera=camera;presentation_.roll=roll;
    advance_steering(visual_motion_,input,false);advance_bank(presentation_,visual_motion_,false);
    publish_model(presentation_,visual_motion_);update_camera(presentation_,visual_motion_,false,false);
}
void FlightController::update_camera(FlightState& state,PresentationMotion& motion,bool immediate,bool native_snap){
    const Vec3 desired{(-15.f-motion.steer_y-state.boost_fraction*4.f)*.25f,
                       motion.steer_x*-3.f*.25f,(4.f+motion.steer_y*2.f)*.25f};
    const float desired_roll=wrap_angle(state.roll*.5f+motion.steer_x*.03125f);
    const float desired_pitch=wrap_angle(state.pitch+.09817477f-motion.steer_y*.012271847f);
    if(immediate){
        motion.camera_offset=desired;state.camera.roll=desired_roll;
        state.camera.pitch=desired_pitch;state.camera.yaw=state.yaw;
    }else {
        spring(motion.camera_offset.x,motion.camera_offset_velocity.x,desired.x,.15f,.8f,1.f,native_snap);
        spring(motion.camera_offset.y,motion.camera_offset_velocity.y,desired.y,.15f,.8f,1.f,native_snap);
        spring(motion.camera_offset.z,motion.camera_offset_velocity.z,desired.z,.15f,.8f,1.f,native_snap);
        spring_angle(state.camera.roll,motion.camera_roll_velocity,desired_roll,.15f,.8f,1.f,native_snap);
        spring_angle(state.camera.pitch,motion.camera_pitch_velocity,desired_pitch,.15f,.8f,1.f,native_snap);
        spring_angle(state.camera.yaw,motion.camera_yaw_velocity,state.yaw,.15f,.8f,1.f,native_snap);
    }
    const Vec3 forward=direction(state.yaw,state.pitch);
    const Vec3 left=unit(cross({0,0,1},forward),{0,1,0});
    const Vec3 up=unit(cross(forward,left),{0,0,1});
    // Rotate the local lateral/up axes by native hull roll.
    const Vec3 rolled_left=left*std::cos(state.roll)+up*std::sin(state.roll);
    const Vec3 rolled_up=up*std::cos(state.roll)-left*std::sin(state.roll);
    state.camera.position=state.position+forward*motion.camera_offset.x+
        rolled_left*motion.camera_offset.y+rolled_up*motion.camera_offset.z;
    spring(state.camera.fov,motion.fov_velocity,1.134464f+(1.4835298f-1.134464f)*state.boost_fraction,.05f,1.f,1.f,native_snap);
}
void FlightController::constrain(Vec3 position,Vec3 safe_direction,float speed_multiplier){
    if(!finite(position)||!finite(safe_direction))return;
    state_.position=position;
    const Vec3 d=unit(safe_direction,direction(state_.yaw,state_.pitch));
    state_.yaw=angle_lerp(state_.yaw,yaw_of(d),.1f);
    state_.pitch=std::clamp(angle_lerp(state_.pitch,pitch_of(d),.1f),-pitch_limit,pitch_limit);
    if(std::isfinite(speed_multiplier))state_.speed=std::max(cruise_speed*.3f,state_.speed*std::clamp(speed_multiplier,0.f,1.f));
}
void FlightController::frame(Vec3 position,float yaw,float pitch){
    if(!finite(position)||!std::isfinite(yaw)||!std::isfinite(pitch))return;
    state_.position=position;state_.yaw=wrap_angle(yaw);state_.pitch=std::clamp(pitch,-pitch_limit,pitch_limit);
    state_.model_yaw=state_.yaw;state_.model_pitch=state_.pitch;update_camera(state_,native_motion_,true,true);
    const auto camera=presentation_.camera;const float roll=presentation_.roll,model_roll=presentation_.model_roll;
    presentation_=state_;presentation_.camera=camera;presentation_.roll=roll;presentation_.model_roll=model_roll;
    update_camera(presentation_,visual_motion_,true,false);
}
}

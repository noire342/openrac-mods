// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <cstdint>

namespace spacevoyages {
struct FlightInput {
    // Native normalized stick convention: positive horizontal turns yaw down;
    // positive vertical turns pitch down (toward world+Z).
    float horizontal{},vertical{};
    bool boost{};
};
struct CameraState {
    Vec3 position{};
    float yaw{},pitch{},roll{},fov=1.134464f;
};
struct FlightState {
    Vec3 position{};
    float yaw{},pitch{},roll{},speed=24.f;
    float model_yaw{},model_pitch{},model_roll{},boost_fraction{};
    CameraState camera{};
    std::uint64_t ticks{};
};
// Portable adaptation of Gemlik FUN_L13_002ba178, not a call into an unloaded
// native overlay. Simulation uses native60Hz steps, independent of render rate.
class FlightController {
public:
    using StepConstraint=void(*)(void* user,FlightController& flight,Vec3 previous_position);
    void reset(Vec3 position,float yaw=0,float pitch=0);
    void tick(double seconds,const FlightInput& input);
    // One native60Hz transaction: integrate, apply optional geometry policy,
    // then publish the hull pose and advance camera/FOV springs exactly once.
    void native_tick(const FlightInput& input,StepConstraint constraint=nullptr,void* user=nullptr);
    const FlightState& state()const noexcept{return state_;}
    // Render-only companion: same native coefficients and corrected physical
    // trajectory, with asymptotic cosmetic/camera settling instead of retail's
    // final limit*.01 snap. It never feeds input, movement or collision state.
    const FlightState& presentation_state()const noexcept{return presentation_;}
    // Called from a StepConstraint, before presentation is published. Geometry
    // owns correction policy; native steering and spring state are preserved.
    void constrain(Vec3 position,Vec3 safe_direction,float speed_multiplier=1.f);
    void frame(Vec3 position,float yaw,float pitch);
    static constexpr float cruise_speed=24.f,boost_speed=30.f;
    static constexpr float native_step=1.f/60.f;
    static constexpr float pitch_limit=1.3962634f;
private:
    struct PresentationMotion {
        float steer_x{},steer_y{},steer_x_velocity{},steer_y_velocity{},roll_velocity{};
        Vec3 camera_offset{-3.75f,0,1.f},camera_offset_velocity{};
        float camera_roll_velocity{},camera_pitch_velocity{},camera_yaw_velocity{},fov_velocity{};
    };
    FlightState state_{},presentation_{};
    PresentationMotion native_motion_{},visual_motion_{};
    double accumulator_{};
    static void advance_steering(PresentationMotion&,const FlightInput&,bool native_snap);
    static void advance_bank(FlightState&,PresentationMotion&,bool native_snap);
    static void publish_model(FlightState&,const PresentationMotion&);
    static void update_camera(FlightState&,PresentationMotion&,bool immediate,bool native_snap);
    void update_presentation(const FlightInput& input);
};
}

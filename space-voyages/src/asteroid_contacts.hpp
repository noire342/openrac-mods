// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "asteroid_field.hpp"
#include <array>
#include <cstdint>

namespace spacevoyages {
struct AsteroidMotionResult {
    Vec3 position{};
    unsigned contacts{};
    bool blocked{};
    float impact_speed{}; // maximum inward normal speed, units/second
    std::size_t contact_id=static_cast<std::size_t>(-1);
    bool touching{};
};
struct AsteroidContactStats {
    unsigned contacts{};
    std::uint64_t total_contacts{};
    bool camera_occluded{};
    float camera_fraction=1.f;
    bool camera_lateral{};
};
// Geometry policy only. It never changes input, yaw/roll, health, inventory or
// native spring coefficients. Its caller applies the returned safe position.
class AsteroidContacts final {
public:
    explicit AsteroidContacts(std::vector<Asteroid> asteroids);
    // Optional borrowed mask with the same stable indexing as the layout.
    // Its storage outlives this controller and is not resized during travel;
    // changing bytes or moving its owning vector needs no reindexing.
    // A mismatched mask is refused without changing the current binding.
    bool set_alive_mask(const std::vector<std::uint8_t>* mask)noexcept;
    AsteroidMotionResult move(Vec3 previous,Vec3 desired,float seconds=1.f/60.f);
    Vec3 camera(Vec3 ship,Vec3 desired);
    void reset()noexcept{stats_={};camera_fraction_=1.f;camera_history_=camera_modified_=false;camera_length_=camera_previous_native_length_=0;camera_direction_=camera_previous_native_direction_={{-1,0,0}};}
    const AsteroidContactStats& stats()const noexcept{return stats_;}
    static constexpr float ship_radius=1.50f,camera_radius=.30f;
    static constexpr float camera_minimum_distance=1.85f;
private:
    AsteroidSpatialIndex index_;
    AsteroidQueryScratch scratch_;
    std::vector<AsteroidSweepHit> sweep_;
    std::vector<std::size_t> overlaps_;
    const std::uint8_t* alive_mask_{};
    std::size_t alive_count_{};
    AsteroidContactStats stats_{};
    float camera_fraction_=1.f;
    bool camera_history_{},camera_modified_{};
    double camera_length_{};
    std::array<double,3> camera_direction_{{-1,0,0}};
    double camera_previous_native_length_{};
    std::array<double,3> camera_previous_native_direction_{{-1,0,0}};
    Vec3 camera_previous_ship_{},camera_previous_eye_{};
    bool clear(Vec3,float);
    bool alive(std::size_t)const noexcept;
    bool query(Vec3,float);
    bool sweep(Vec3,Vec3,float);
    bool camera_pose_clear(Vec3 ship,Vec3 eye);
    bool camera_transition_clear(Vec3 ship,Vec3 eye);
    bool camera_shoulder(Vec3 ship,Vec3 native_eye,double distance,bool check_transition,Vec3& eye);
    Vec3 camera_endpoint_recovery(Vec3 ship,Vec3 native_eye);
    // Exceptional repair for an externally injected overlapping pose. Normal
    // contact keeps the swept/sliding path; only this branch may reposition
    // directly to a verified exit, without changing the pilot's orientation.
    Vec3 recover_external(Vec3 origin);
};
}

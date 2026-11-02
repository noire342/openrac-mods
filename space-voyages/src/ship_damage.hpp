// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace spacevoyages {
struct ShipDamageState {
    int hull=100,max_hull=100;
    float hit_flash{},destruction_progress{};
    bool destroyed{},respawn_ready{},respawn_intro{};
    std::uint32_t respawns{};
    float last_impact_speed{};
    std::uint32_t damage_events{};
    float hud_hull=100.f;
};
struct ShipImpactSample {
    float normal_speed{}; // inward units/second, not total forward speed
    bool touching{}; // includes resting/sliding contact, even without a new hit
};
// Pure scene-local policy. It knows no native hero, game memory, geometry,
// renderer, planet coordinates, inventory or save format.
class ShipDamage final {
public:
    const ShipDamageState& state()const noexcept{return state_;}
    void reset()noexcept;
    // Called once per native60Hz simulation step; seconds advances policy
    // timers, while the display follows its original per-step HUD equation.
    void tick(float seconds,ShipImpactSample impact={},bool damage_window=true,bool paused=false)noexcept;
    // The journey owner calls this only after a final destruction frame was
    // presented. Reaching respawn_ready by itself never changes the scene.
    bool respawn()noexcept;
    void finish_respawn_intro()noexcept{state_.respawn_intro=false;}
    static int damage_for_impact(float normal_speed)noexcept;
    static constexpr float minimum_impact_speed=4.f;
    static constexpr float contact_cooldown=.45f,detachment_rearm=.25f;
    static constexpr float respawn_grace=1.f,destruction_duration=1.f,flash_duration=.35f;
    static constexpr float hud_gain=.2f,hud_maximum_step=100.f/256.f;
private:
    ShipDamageState state_{};
    bool armed_=true;
    double cooldown_{},detached_{},grace_{},destruction_time_{};
};
}

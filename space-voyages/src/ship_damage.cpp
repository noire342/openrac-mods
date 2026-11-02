// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_damage.hpp"
#include <algorithm>
#include <cmath>

namespace spacevoyages {
void ShipDamage::reset()noexcept {
    state_={};armed_=true;cooldown_=detached_=grace_=destruction_time_=0;
}
int ShipDamage::damage_for_impact(float normal_speed)noexcept {
    if(!std::isfinite(normal_speed)||normal_speed<=minimum_impact_speed)return 0;
    // Normal cruise24 ->22 hull; turbo30 ->29. Clamp before rounding so an
    // invalid external-scale velocity cannot overflow the integer hull type.
    const double amount=(double(normal_speed)-minimum_impact_speed)*1.1;
    return int(std::lround(std::clamp(amount,1.,100.)));
}
void ShipDamage::tick(float seconds,ShipImpactSample impact,bool damage_window,bool paused)noexcept {
    if(paused||!std::isfinite(seconds)||seconds<=0)return;
    const double dt=seconds;
    state_.hit_flash=std::max(0.f,state_.hit_flash-seconds/flash_duration);
    if(state_.destroyed){
        destruction_time_=std::min(double(destruction_duration),destruction_time_+dt);
        state_.destruction_progress=float(destruction_time_/destruction_duration);
        state_.respawn_ready=destruction_time_>=double(destruction_duration)-1e-7;
    }else if(damage_window){
        // Cinematics do not consume the respawn's playable grace period. Only
        // FreeFlight enables impact/cooldown/rearm accounting at the call site.
        const bool was_protected=grace_>0;
        grace_=std::max(0.,grace_-dt);cooldown_=std::max(0.,cooldown_-dt);
        if(!impact.touching)detached_=std::min(double(detachment_rearm),detached_+dt);
        if(!armed_&&cooldown_<=1e-7&&detached_>=double(detachment_rearm)-1e-7)armed_=true;
        if(impact.touching)detached_=0;
        const int damage=damage_for_impact(impact.normal_speed);
        if(armed_&&damage){
            // A harmless grace-period hit still consumes its contact episode.
            // Holding the same surface cannot become a fresh hit at expiry.
            armed_=false;cooldown_=contact_cooldown;detached_=0;
            if(!was_protected){
                state_.hull=std::max(0,state_.hull-damage);state_.hit_flash=1.f;
                state_.last_impact_speed=impact.normal_speed;++state_.damage_events;
                if(state_.hull==0){
                    state_.destroyed=true;state_.destruction_progress=0;state_.respawn_ready=false;
                    state_.respawn_intro=false;destruction_time_=0;
                }
            }
        }
    }
    // Native Gemlik health display: gain.2 and a maximum change of one in
    // its256-unit domain, represented here on the scene's100-hull scale.
    state_.hud_hull+=std::clamp((float(state_.hull)-state_.hud_hull)*hud_gain,-hud_maximum_step,hud_maximum_step);
    state_.hud_hull=std::clamp(state_.hud_hull,0.f,float(state_.max_hull));
}
bool ShipDamage::respawn()noexcept {
    if(!state_.destroyed||!state_.respawn_ready)return false;
    state_.hull=state_.max_hull;state_.hud_hull=float(state_.max_hull);state_.hit_flash=state_.destruction_progress=0;
    state_.destroyed=state_.respawn_ready=false;state_.respawn_intro=true;++state_.respawns;
    armed_=true;cooldown_=detached_=destruction_time_=0;grace_=respawn_grace;
    return true;
}
}

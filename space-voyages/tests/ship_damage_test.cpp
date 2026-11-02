// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_damage.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spacevoyages;
namespace {
unsigned checks;
constexpr float dt=1.f/60.f;
void require(bool pass,const char* why){++checks;if(!pass){std::fprintf(stderr,"ship_damage: %s\n",why);std::exit(1);}}
void detached(ShipDamage& damage,int ticks=60){for(int i=0;i<ticks;++i)damage.tick(dt);}
void kill(ShipDamage& damage){for(int i=0;i<4;++i){damage.tick(dt,{30,true});if(i<3)detached(damage);}}
}
int main(){
    require(ShipDamage::damage_for_impact(24)==22&&ShipDamage::damage_for_impact(30)==29,"native cruise and turbo normal impacts have the agreed22/29 hull cost");
    for(float speed:{-1.f,0.f,3.999f,4.f,NAN,std::numeric_limits<float>::infinity()})
        require(ShipDamage::damage_for_impact(speed)==0,"grazing, subthreshold and invalid impact speeds cannot damage hull");
    require(ShipDamage::damage_for_impact(4.01f)==1&&ShipDamage::damage_for_impact(1e20f)==100,"damage mapping is bounded before integer conversion");
    {
        ShipDamage damage;const auto initial=damage.state();
        require(initial.hull==100&&initial.max_hull==100&&initial.hud_hull==100&&initial.damage_events==0&&!initial.destroyed,"fresh scene starts with a full independent ship hull");
        damage.tick(dt,{24,true});
        require(damage.state().hull==78&&damage.state().damage_events==1&&damage.state().last_impact_speed==24&&damage.state().hit_flash==1,"one damaging contact changes actual hull immediately and records its normal speed");
        require(damage.state().hud_hull==100.f-100.f/256.f,"HUD follows the original Gemlik maximum one-of256-unit step");
        for(int i=0;i<600;++i){const float previous=damage.state().hud_hull;damage.tick(dt,{30,true});
            require(damage.state().hull==78&&std::abs(damage.state().hud_hull-previous)<=ShipDamage::hud_maximum_step+.00001f,"sustained contact cannot repeatedly damage and HUD retains its native rate limit");}
        require(damage.state().damage_events==1&&damage.state().hit_flash==0,"resting/sliding contact does not restart the damage flash");
        detached(damage,14);damage.tick(dt,{30,true});
        require(damage.state().hull==78,"less than250ms consecutive separation does not rearm a contact");
        detached(damage,15);damage.tick(dt,{30,true});
        require(damage.state().hull==49&&damage.state().damage_events==2,"a detached new impact rearms after cooldown and applies turbo damage once");
    }
    {
        ShipDamage damage;damage.tick(dt,{24,true});detached(damage,15);damage.tick(dt,{30,true});
        require(damage.state().hull==78,"separation alone cannot bypass the independent450ms contact cooldown");
        for(int i=0;i<60;++i)damage.tick(dt,{30,true});
        require(damage.state().hull==78,"waiting out cooldown while still touching does not create a new impact episode");
        detached(damage);damage.tick(dt,{30,false});
        require(damage.state().hull==49,"a swept hit can damage even when tangential movement ends outside the contact band");
    }
    {
        ShipDamage damage;
        for(int i=0;i<500;++i)damage.tick(dt,{3.9f,true});
        require(damage.state().hull==100&&damage.state().damage_events==0,"fast tangential flight with little inward velocity remains harmless");
        damage.tick(dt,{24,true});const auto frozen=damage.state();
        for(int i=0;i<100;++i)damage.tick(.25f,{30,true},true,true);
        require(damage.state().hull==frozen.hull&&damage.state().hit_flash==frozen.hit_flash&&damage.state().hud_hull==frozen.hud_hull&&damage.state().damage_events==frozen.damage_events,"pause freezes actual health, hit flash and native HUD easing");
        damage.tick(0,{30,true});damage.tick(NAN,{30,true});damage.tick(-1,{30,true});
        require(damage.state().hull==frozen.hull&&damage.state().hit_flash==frozen.hit_flash,"invalid elapsed time has no damage or visual side effects");
    }
    {
        ShipDamage damage;kill(damage);
        require(damage.state().hull==0&&damage.state().destroyed&&!damage.state().respawn_ready&&damage.state().damage_events==4,"four separated turbo crashes start destruction without instant respawn");
        require(!damage.respawn(),"death cannot reset before the complete destruction duration");
        damage.tick(.5f,{},false);const auto paused=damage.state();damage.tick(10.f,{100,true},true,true);
        require(damage.state().destruction_progress==paused.destruction_progress&&!damage.state().respawn_ready,"paused destruction does not complete behind the paused scene");
        damage.tick(.5f,{},false);
        require(damage.state().respawn_ready&&damage.state().destruction_progress==1&&damage.state().respawns==0&&damage.state().hull==0,"ready destruction stays black and dead until its owner explicitly respawns");
        for(int i=0;i<100;++i)damage.tick(.25f,{100,true});
        require(damage.state().destroyed&&damage.state().respawns==0,"unacknowledged destruction never skips its final frame during catch-up");
        require(damage.respawn()&&damage.state().hull==100&&damage.state().hud_hull==100&&damage.state().respawns==1&&damage.state().respawn_intro,"explicit local respawn restores actual/display hull and exposes intro state");
        require(damage.state().damage_events==4&&damage.state().last_impact_speed==30&&!damage.respawn(),"respawn preserves journey telemetry and cannot run twice");
        for(int i=0;i<105;++i)damage.tick(dt,{30,true},false);
        damage.finish_respawn_intro();damage.tick(dt,{30,true});
        require(damage.state().hull==100&&!damage.state().respawn_intro,"the cinematic does not consume the playable respawn grace period");
        for(int i=0;i<120;++i)damage.tick(dt,{30,true});
        require(damage.state().hull==100&&damage.state().damage_events==4,"a grace-period contact stays harmless after expiry until the player detaches");
        detached(damage);damage.tick(dt,{24,true});
        require(damage.state().hull==78&&damage.state().damage_events==5,"a genuinely new hit after grace and detachment damages normally");
        damage.reset();require(damage.state().hull==100&&damage.state().hud_hull==100&&damage.state().respawns==0&&damage.state().damage_events==0,"new journeys reset scene-local health and telemetry");
    }
    std::printf("Ship damage: %u normal-impact, contact episode, HUD, pause and lifecycle checks passed\n",checks);
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "voyage.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
using namespace spacevoyages;
namespace {std::size_t allocations;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks{};
void require(bool okay,const char* why){++checks;if(!okay){std::fprintf(stderr,"asteroid_voyage: %s\n",why);std::exit(1);}}
std::array<bool,19> coordinates(){std::array<bool,19> c{};c[1]=c[3]=true;return c;}
AsteroidLayout layout(){const auto& c=galaxy_catalog();return build_asteroid_layout(c.data(),c.size());}
void fixture_pose(VoyageController& v,const FlightState& pose){
    // Pure host fixture: no native memory, saved game, hero or SDK is used.
    const_cast<FlightState&>(v.flight())=pose;
}
void impact(VoyageController& v,const Asteroid& rock){
    auto pose=v.flight();pose.position=rock.position-Vec3{rock.radius+AsteroidContacts::ship_radius+.15f,0,0};
    pose.yaw=pose.pitch=pose.roll=pose.model_yaw=pose.model_pitch=pose.model_roll=0;pose.speed=30;
    fixture_pose(v,pose);v.tick(1./60,{0,0,true},false);
}
void detach_at_safe_origin(VoyageController& v,const FlightState& spawn){
    fixture_pose(v,spawn);for(int i=0;i<60;++i)v.tick(1./60,{},false);
}
}
int main(){
    VoyageController v;v.set_asteroids(layout(),true);
    require(v.asteroid_layout()&&v.asteroid_layout()->asteroids().size()==176,"install deterministic fields before boarding");
    require(v.begin(3,coordinates()),"eligible native origin begins with fields");
    bool refused=false;try{v.set_asteroids(layout(),false);}catch(const std::logic_error&){refused=true;}
    require(refused,"live geometry cannot be replaced during departure");
    v.begin_space_entry();
    for(unsigned i=0;i<110;++i)v.tick(1./60,{},false);
    require(v.phase()==Phase::FreeFlight&&v.asteroid_contacts().total_contacts==0,"protected intro reaches free flight without contact");
    const double seconds=v.scene_seconds();const auto before=v.presentation_flight();
    for(unsigned i=0;i<40;++i)v.tick(.25,{1,1,true},true,true);
    require(v.scene_seconds()==seconds&&v.presentation_flight().ticks==before.ticks,"pause freezes asteroid and ship simulation clocks");
    require(length(v.presentation_flight().camera.position-before.camera.position)==0,"pause does not run camera collision springs");
    v.tick(1./60,{},false);
    require(v.scene_seconds()>seconds&&v.scene_seconds()-seconds<.017,"resume advances exactly one tick, without paused elapsed time");
    v.cancel();
    require(v.scene_seconds()==0&&v.asteroid_layout()->asteroids().size()==176,"ending a journey resets time while retaining fixed geometry");
    require(v.asteroid_contacts().total_contacts==0&&v.asteroid_contacts().camera_fraction==1,"contact and camera state reset for the next journey");
    require(v.begin(3,coordinates())&&v.departure_complete(),"next eligible boarding reuses the static fields");
    v.tick(1./60,{},true);
    require(v.phase()==Phase::Approach,"radial return to origin is still available");
    const auto approach_tick=v.flight().ticks;const double approach_time=v.scene_seconds();
    for(unsigned i=0;i<10;++i)v.tick(1./60,{},false);
    require(v.phase()==Phase::Approach&&v.flight().ticks==approach_tick,"authored approach does not invent native fighter ticks");
    require(v.scene_seconds()>approach_time+.16,"asteroid animation continues through the authored approach");
    for(unsigned i=0;i<60;++i)v.tick(1./60,{},false);
    require(v.phase()==Phase::Arrival&&v.take_destination()==3,"asteroids preserve the native origin destination");
    const double arrived=v.scene_seconds();v.tick(.25,{1,1,true},true);
    require(v.scene_seconds()==arrived,"arrival waiting for the draw acknowledgement does not animate further");
    require(v.arrival_complete()&&v.phase()==Phase::Ground,"native arrival still restores Ground");
    VoyageController plain,decorative;decorative.set_asteroids(layout(),false);
    require(plain.begin(3,coordinates())&&decorative.begin(3,coordinates()),"optional visual-only comparison begins from identical origins");
    plain.begin_space_entry();decorative.begin_space_entry();
    for(unsigned i=0;i<1500;++i){const FlightInput input{i%400<170?.7f:0.f,i%500<80?-.4f:0.f,i%300<160};
        plain.tick(1./60,input,false);decorative.tick(1./60,input,false);
        const auto& a=plain.presentation_flight();const auto& b=decorative.presentation_flight();
        require(length(a.position-b.position)==0&&a.yaw==b.yaw&&a.pitch==b.pitch&&a.model_roll==b.model_roll&&a.speed==b.speed&&a.camera.fov==b.camera.fov&&length(a.camera.position-b.camera.position)==0,"decorative-only asteroids cannot affect native flight or camera");
    }
    require(int(Phase::Ground)==0&&int(Phase::Departure)==1&&int(Phase::FreeFlight)==2&&int(Phase::Approach)==3&&int(Phase::Arrival)==4,
        "scene-local destruction does not change the existing phase API values");
    {
        const std::vector<Asteroid> rock{{{0,0,0},3}};AsteroidContacts contacts(rock);
        const auto cruise=contacts.move({-4.65f,0,0},{-4.25f,0,0});
        require(std::abs(cruise.impact_speed-24)<.001f&&cruise.contact_id==0&&cruise.touching,"geometry reports actual cruise inward speed and persistent contact identity");
        contacts.reset();const auto turbo=contacts.move({-4.65f,0,0},{-4.15f,0,0});
        require(std::abs(turbo.impact_speed-30)<.001f,"turbo normal impact is measured before geometric braking");
        contacts.reset();const auto glancing=contacts.move({-10,4.51f,0},{10,4.51f,0},1.f);
        ShipDamage policy;policy.tick(1.f,{glancing.impact_speed,glancing.touching});
        require(glancing.contacts>0&&glancing.impact_speed<4&&policy.state().hull==100,"a fast but tangential geometric contact remains below the damage threshold");
        const auto repaired=contacts.move({0,0,0},{0,0,0});
        require(repaired.impact_speed==0,"repairing an externally injected overlapping pose cannot manufacture crash damage");
    }
    {
        VoyageController damaged;damaged.set_asteroids(layout(),true);
        require(damaged.asteroid_damage_enabled()&&damaged.ship_damage().hull==100,"new scene enables independent ship damage by default");
        require(damaged.begin(3,coordinates()),"damage fixture starts from the existing native origin");
        const auto safe_spawn=damaged.flight();const auto rock=damaged.asteroid_layout()->asteroids().front();
        bool changed_live=false;try{damaged.set_asteroid_damage(false);}catch(const std::logic_error&){changed_live=true;}
        require(changed_live,"damage configuration cannot change while the native world is suspended");
        require(!damaged.acknowledge_destruction_frame()&&damaged.begin_space_entry(),"destruction acknowledgement cannot skip an ordinary departure");
        for(int i=0;i<105;++i)damaged.tick(1./60,{},false);
        require(damaged.phase()==Phase::FreeFlight&&damaged.ship_damage().hull==100,"protected original intro does not apply asteroid damage");
        const auto before_allocations=allocations;
        for(unsigned generation=0;generation<2;++generation){
            if(generation){
                // Grace is playable time and the episode remains latched even
                // after it expires; this still must not kill the fresh ship.
                impact(damaged,rock);for(int i=0;i<120;++i)damaged.tick(1./60,{0,0,true},false);
                require(damaged.ship_damage().hull==100&&damaged.ship_damage().damage_events==4,"respawn grace and a sustained surface contact cannot repeatedly consume hull");
                detach_at_safe_origin(damaged,safe_spawn);
            }
            for(int crash=0;crash<4;++crash){
                impact(damaged,rock);const int expected=std::max(0,100-29*(crash+1));
                require(damaged.ship_damage().hull==expected&&damaged.ship_damage().damage_events==generation*4+unsigned(crash)+1,"each genuinely detached turbo impact damages hull exactly once");
                if(crash<3)detach_at_safe_origin(damaged,safe_spawn);
            }
            require(damaged.ship_damage().destroyed&&damaged.ship_damage().hull==0&&!damaged.ship_damage().respawn_ready&&damaged.phase()==Phase::FreeFlight,
                "zero hull freezes in the same free-flight scene without native arrival or an extra phase");
            const auto dead_pose=damaged.flight(),dead_visual=damaged.presentation_flight();const auto paused=damaged.ship_damage();const double dead_time=damaged.scene_seconds();
            for(int i=0;i<20;++i)damaged.tick(.25,{1,-1,true},true,true);
            require(damaged.scene_seconds()==dead_time&&damaged.ship_damage().hit_flash==paused.hit_flash&&damaged.ship_damage().hud_hull==paused.hud_hull&&damaged.ship_damage().destruction_progress==0,
                "pause freezes zero-hull destruction, HUD, hit flash and the scene clock");
            require(!damaged.acknowledge_destruction_frame(),"a premature black-frame acknowledgement cannot reset destruction");
            for(int i=0;i<4;++i)damaged.tick(.25,{1,-1,true},true);
            require(damaged.ship_damage().respawn_ready&&damaged.ship_damage().destruction_progress==1&&damaged.ship_damage().respawns==generation,"one second of catch-up reaches final black but cannot automatically respawn");
            for(int i=0;i<4;++i)damaged.tick(.25,{-1,1,true},false);
            require(damaged.ship_damage().destroyed&&damaged.ship_damage().respawns==generation&&damaged.flight().ticks==dead_pose.ticks&&length(damaged.flight().position-dead_pose.position)==0&&
                damaged.flight().yaw==dead_pose.yaw&&damaged.flight().pitch==dead_pose.pitch&&damaged.flight().roll==dead_pose.roll&&length(damaged.presentation_flight().camera.position-dead_visual.camera.position)==0,
                "unacknowledged final black keeps input, fighter pose and camera completely frozen");
            require(!damaged.candidate()&&!damaged.candidate_unlocked()&&damaged.take_destination()==-1&&damaged.target_level()==-1,"Triangle and target selection cannot escape the destruction state");
            const double before_respawn=damaged.scene_seconds();
            require(damaged.acknowledge_destruction_frame()&&!damaged.acknowledge_destruction_frame(),"only the first presented-final-frame acknowledgement is accepted per destruction");
            damaged.tick(.25,{1,1,true},true,true);
            require(damaged.ship_damage().destroyed&&damaged.ship_damage().respawns==generation&&damaged.scene_seconds()==before_respawn,"acknowledgement during pause waits for an unpaused simulation tick");
            damaged.tick(1./60,{1,1,true},true);
            require(damaged.phase()==Phase::Departure&&damaged.ship_damage().hull==100&&damaged.ship_damage().hud_hull==100&&damaged.ship_damage().respawns==generation+1&&
                damaged.ship_damage().respawn_intro&&!damaged.ship_damage().destroyed&&damaged.space_entry_progress()==0&&damaged.flight().ticks==0,
                "next active tick resets only the local ship and exposes the black first frame of respawn intro");
            require(length(damaged.flight().position-safe_spawn.position)==0&&damaged.flight().yaw==safe_spawn.yaw&&damaged.flight().pitch==safe_spawn.pitch&&damaged.origin()==3&&
                damaged.destination_unlocked(1)&&damaged.destination_unlocked(3)&&!damaged.destination_unlocked(4)&&damaged.scene_seconds()>before_respawn,
                "respawn reuses the initial safe pose without changing coordinates/origin or restarting asteroid time");
            const auto intro_pose=damaged.flight();const double intro_time=damaged.scene_seconds();damaged.tick(.25,{-1,-1,true},true,true);
            require(damaged.scene_seconds()==intro_time&&damaged.flight().ticks==intro_pose.ticks&&damaged.space_entry_progress()==0,"pausing the new intro never advances or catches up input");
            for(int i=0;i<105;++i)damaged.tick(1./60,{1,-1,true},true);
            require(damaged.phase()==Phase::FreeFlight&&!damaged.ship_damage().respawn_intro&&damaged.flight().ticks==105&&damaged.flight().yaw==safe_spawn.yaw&&damaged.flight().pitch==safe_spawn.pitch&&damaged.flight().roll==0,
                "respawn replays the existing1.75second protected entry and releases controls only at its end");
            require(length(damaged.flight().position-(safe_spawn.position+direction(safe_spawn.yaw,safe_spawn.pitch)*42.f))<.03f,"respawn intro preserves native24-unit cruise and its original42-unit travel distance");
        }
        require(allocations==before_allocations,"damage, two destruction/ack/respawn cycles and HUD easing add no per-tick heap allocations");
        fixture_pose(damaged,safe_spawn);damaged.tick(1./60,{},true);
        require(damaged.phase()==Phase::FreeFlight,"Triangle held through destruction and intro does not automatically enter a planet");
        damaged.tick(1./60,{},false);damaged.tick(1./60,{},true);
        require(damaged.phase()==Phase::Approach,"a new Triangle edge works again after the full local respawn lifecycle");
        damaged.cancel();require(damaged.phase()==Phase::Ground&&damaged.ship_damage().hull==100&&damaged.ship_damage().damage_events==0&&damaged.ship_damage().respawns==0,"leaving the scene resets only scene-local damage bookkeeping");
        damaged.set_asteroid_damage(false);require(damaged.begin(3,coordinates())&&damaged.departure_complete(),"Ground-only optional damage setting can be disabled for another journey");
        for(int i=0;i<8;++i){impact(damaged,rock);detach_at_safe_origin(damaged,safe_spawn);}
        require(damaged.ship_damage().hull==100&&damaged.ship_damage().hud_hull==100&&damaged.ship_damage().damage_events==0&&!damaged.ship_damage().destroyed&&damaged.asteroid_contacts().total_contacts>=8,
            "disabled damage retains the preexisting soft geometric contacts without touching ship health");
        damaged.cancel();damaged.set_asteroid_damage(true);damaged.begin(3,coordinates());damaged.departure_complete();
        for(int i=0;i<4;++i){impact(damaged,rock);if(i<3)detach_at_safe_origin(damaged,safe_spawn);}
        for(int i=0;i<8;++i)damaged.tick(.25,{1,1,true},true);
        require(damaged.ship_damage().destroyed&&damaged.ship_damage().respawn_ready&&damaged.ship_damage().respawns==0,"large unacknowledged catch-up batches cannot skip the final destruction presentation");
        require(damaged.acknowledge_destruction_frame(),"catch-up respawn still requires a presented-black acknowledgement");
        const double acknowledged_time=damaged.scene_seconds();damaged.tick(.25,{1,1,true},true);
        require(damaged.ship_damage().respawns==1&&damaged.phase()==Phase::Departure&&damaged.ship_damage().respawn_intro&&damaged.ship_damage().hull==100&&damaged.flight().ticks==14&&
            damaged.space_entry_progress()>0&&damaged.space_entry_progress()<1&&std::abs(damaged.scene_seconds()-acknowledged_time-.25)<.00001,
            "after acknowledgement one catch-up batch resets once and applies only its remaining14 protected intro ticks without losing scene time");
        require(!damaged.acknowledge_destruction_frame(),"stale acknowledgement cannot restart a completed respawn generation");
    }
    std::printf("Asteroid voyage integration: %u checks passed\n",checks);
}

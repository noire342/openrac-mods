// SPDX-License-Identifier: GPL-3.0-or-later
#include "space_combat.hpp"
#include "voyage.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
using namespace spacevoyages;
namespace {std::size_t allocations;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks;
void require(bool okay,const char* why){++checks;if(!okay){std::fprintf(stderr,"space_combat: %s\n",why);std::exit(1);}}
bool close(Vec3 a,Vec3 b,float epsilon=.0001f){return length(a-b)<epsilon;}
SpaceCombatAssets weapons(){
    // Synthetic mounts for mathematical fixtures, not production model data.
    SpaceCombatAssets a;for(auto& ship:a.ships){ship.armed=true;ship.count=1;ship.muzzles[0]={{0,0,0},{1,0,0}};}return a;
}
FlightState stationary(){FlightState s;s.speed=0;return s;}
std::size_t active(const SpaceCombat& c){return std::count_if(c.projectiles().begin(),c.projectiles().end(),[](const auto& p){return p.active;});}
const SpaceProjectile* projectile(const SpaceCombat& c,std::uint64_t id){for(const auto& p:c.projectiles())if(p.id==id)return &p;return nullptr;}
void pulse(SpaceCombat& c,const FlightState& ship,int ticks=10){c.step(ship,true);for(int t=1;t<ticks;++t)c.step(ship,false);}
Vec3 reference_euler(Vec3 v,float yaw,float pitch,float roll){
    // Independent successive right-handed rotations around X, then Y, then Z.
    const double x=v.x,y=v.y*std::cos(double(roll))-v.z*std::sin(double(roll));
    const double z=v.y*std::sin(double(roll))+v.z*std::cos(double(roll));
    const double px=x*std::cos(double(pitch))+z*std::sin(double(pitch));
    const double pz=-x*std::sin(double(pitch))+z*std::cos(double(pitch));
    return {float(px*std::cos(double(yaw))-y*std::sin(double(yaw))),float(px*std::sin(double(yaw))+y*std::cos(double(yaw))),float(pz)};
}
const Destination test_planets[]={{1,"Origin",{-500,0,0},10,DestinationKind::Planet},{3,"Other",{500,0,0},10,DestinationKind::Planet}};
AsteroidLayout test_layout(){
    AsteroidLayoutPolicy policy;policy.minimum_radius=policy.maximum_radius=3;policy.variants=1;policy.variant_radii=nullptr;policy.variant_radius_count=0;
    const AsteroidFieldDefinition field{77,0x317af62du,{0,100,0},{40,40,40},{1,0,0},0,2};
    return build_asteroid_layout(test_planets,2,&field,1,policy);
}
void pose(VoyageController& voyage,Vec3 position){
    auto& s=const_cast<FlightState&>(voyage.flight());s.position=position;
    s.yaw=s.pitch=s.roll=s.model_yaw=s.model_pitch=s.model_roll=0;s.speed=30;
}
}
int main(){
    const auto assets=weapons();const auto ship=stationary();
    {
        SpaceCombat c;c.set_world({},nullptr,0);require(c.set_assets(assets),"finite explicit model-local mounts configure the core");
        for(int cls:{531,0,534,-1}){c.reset(cls);for(int t=0;t<100;++t)c.step(ship,true);
            require(!c.state().armed&&c.state().shots==0&&active(c)==0,"green531 and unsupported classes never fire, even when asset metadata falsely marks them armed");}
        for(int cls:{532,533}){c.reset(cls);c.step(ship,true);require(c.state().armed&&c.state().shots==1,"both verified armed native ship classes can fire");}
        auto missing=assets;missing.ships[1].count=0;require(c.set_assets(missing),"an absent weapon binding is a supported unarmed capability");
        c.reset(532);c.step(ship,true);require(!c.state().armed&&!c.state().shots,"armed metadata without actual mounts never invents a center gun");
        auto invalid=assets;invalid.ships[2].count=9;require(!c.set_assets(invalid),"out-of-bounds mount counts are refused");
        invalid=assets;invalid.ships[1].muzzles[0].direction={0,0,0};require(!c.set_assets(invalid),"zero firing direction is refused");
        invalid=assets;invalid.ships[1].muzzles[0].direction={1e38f,1e38f,1e38f};require(!c.set_assets(invalid),"overflowing direction normalization is refused instead of inventing a forward vector");
        invalid=assets;invalid.projectile_speed=NAN;require(!c.set_assets(invalid),"nonfinite projectile speed is refused");
        invalid=assets;invalid.cadence_ticks=0;require(!c.set_assets(invalid),"zero cadence cannot produce unbounded firing");
    }
    for(float yaw:{0.f,.7f,-2.5f})for(float pitch:{-1.9f,0.f,1.9f})for(float roll:{-1.2f,0.f,.8f}){
        auto s=ship;s.position={12,-5,9};s.model_yaw=yaw;s.model_pitch=pitch;s.model_roll=roll;
        const Vec3 local{1.2f,-.7f,.4f};const auto reference=reference_euler(local,yaw,pitch,roll);
        require(close(weapon_local_to_world(local,s,false),reference)&&close(weapon_local_to_world(local,s,true),s.position+reference),"muzzles use the actual native RzRyRx model basis, including pitch across90degrees and bank");
    }
    {
        SpaceCombat c;c.set_world({},nullptr,0);auto a=assets;
        a.ships[1].count=2;a.ships[1].muzzles[0]={{2,1,.5f},{1,0,0}};a.ships[1].muzzles[1]={{2,-1,.5f},{1,0,0}};
        require(c.set_assets(a),"alternating explicit muzzle fixture is valid");c.reset(532);
        auto s=ship;s.position={10,20,30};s.model_yaw=.7f;s.model_pitch=-.4f;s.model_roll=.6f;s.yaw=-.2f;s.pitch=.3f;s.speed=24;
        c.step(s,true,9.);const auto* p=projectile(c,1);require(p&&p->active,"first held tick spawns a projectile from a free pool slot");
        const auto expected=s.position+reference_euler({2,1,.5f},s.model_yaw,s.model_pitch,s.model_roll);
        const auto expected_velocity=reference_euler({1,0,0},s.model_yaw,s.model_pitch,s.model_roll)*400+direction(s.yaw,s.pitch)*24;
        require(close(p->position,expected)&&close(p->birth_position,expected)&&close(p->velocity,expected_velocity)&&p->birth_seconds==9.,"spawn comes from the transformed muzzle and inherits native ship velocity, not the ship center");
        require(std::abs(p->lifetime-200.f/length(p->velocity))<1e-6f,"GPU birth/lifetime metadata describes the same physical range");
        const auto revision=c.projectile_revision();const auto initial_velocity=p->velocity;
        for(int t=0;t<3;++t)c.step(s,true,9.+(t+1)/60.);
        require(c.state().shots==1&&c.projectile_revision()==revision&&close(projectile(c,1)->velocity,initial_velocity),"three intermediate native ticks do not fire or upload a new pool generation");
        c.step(s,true,9.+4./60.);p=projectile(c,2);
        require(p&&close(p->birth_position,s.position+reference_euler({2,-1,.5f},s.model_yaw,s.model_pitch,s.model_roll)),"the second shot at four ticks alternates to the other actual mount");
        c.reset(532);for(int t=0;t<60;++t)c.step(ship,true);
        require(c.state().shots==15&&active(c)<=8,"native four-tick cadence produces15shots per simulated second with bounded lifetime");
    }
    {
        SpaceCombat c;c.set_world({{{20,0,0},3},{{40,0,0},6}},nullptr,0);c.set_assets(assets);c.reset(532);
        const auto revision=c.revision();pulse(c,ship);
        require(c.asteroid_hp()[0]==2&&c.asteroid_hp()[1]==6&&c.alive_mask()[0]&&c.event_count()==1,"a projectile damages only the nearest live rock once");
        pulse(c,ship);pulse(c,ship);
        require(!c.alive_mask()[0]&&c.asteroid_hp()[0]==0&&c.alive_mask()[1]&&c.state().destroyed==1&&c.revision()==revision+1,"small rocks require three hits, then change their stable alive-mask byte and revision once");
        for(int i=0;i<6;++i)pulse(c,ship);
        require(!c.alive_mask()[1]&&c.asteroid_hp()[1]==0&&c.state().destroyed==2&&c.state().shots==9,"large rocks require six hits and destroyed nearer rocks no longer block shots");
        const auto count=c.event_count();pulse(c,ship);require(c.event_count()==count,"bullets pass through already destroyed rocks without repeated impacts or rewards");
        const auto* mask_address=&c.alive_mask();const auto before=allocations;c.reset(533);
        require(allocations==before&&&c.alive_mask()==mask_address&&c.alive_mask()[0]&&c.alive_mask()[1]&&c.asteroid_hp()[0]==3&&c.asteroid_hp()[1]==6&&!c.state().shots&&!c.state().destroyed,"only a new journey resets all rock health, with the same mask lifetime and no allocation");
    }
    {
        SpaceCombat c;c.set_world({{{1.25f,0,0},1}},nullptr,0);c.set_assets(assets);c.reset(532);
        const auto* storage=c.destroyed_seconds().data();const auto before=allocations;
        require(c.destroyed_seconds().size()==1&&c.destroyed_seconds()[0]==-1.,"live rocks have a stable allocated death-time slot with the explicit unset sentinel");
        for(int hit=0;hit<3;++hit)pulse(c,ship);
        const auto* last=projectile(c,3);require(last&&!last->active,"third fractional-contact projectile destroys the fixture rock");
        // Entry distance.25 divided by one400u/s native step is fraction.0375.
        const double expected=last->birth_seconds+(1./60.)*.0375;
        require(!c.alive_mask()[0]&&std::abs(c.destroyed_seconds()[0]-expected)<1e-7&&c.destroyed_seconds()[0]==c.events()[2].scene_seconds,
            "fraction.0375 CCD destruction time exactly matches the event and is not rounded to the simulation tick boundary");
        const double death=c.destroyed_seconds()[0];c.clear_projectiles();c.step(ship,false);
        require(c.destroyed_seconds()[0]==death,"clearing bullets and later quiet steps preserve the exact rock death time");
        c.reset(533);
        require(c.destroyed_seconds()[0]==-1.&&c.destroyed_seconds().data()==storage&&allocations==before,"new journey resets death timestamps in place without any tick or reset allocations");
    }
    {
        const Destination planet{1,"Blocker",{10,0,0},2,DestinationKind::Planet};
        SpaceCombat c;c.set_world({{{20,0,0},3}},&planet,1);c.set_assets(assets);c.reset(532);pulse(c,ship);
        require(c.asteroid_hp()[0]==3&&c.event_count()==1&&c.events()[0].kind==SpaceCombatEventKind::PlanetImpact&&close(c.events()[0].position,{8,0,0})&&c.events()[0].radius==0,"an indestructible planet blocks the continuous shot before the rock behind it");
        c.set_world({{{5,0,0},1}},&planet,1);c.reset(532);pulse(c,ship);
        require(c.asteroid_hp()[0]==2&&c.events()[0].kind==SpaceCombatEventKind::Impact&&c.events()[0].radius==1,"nearest rock takes the shot before a more distant planet");
        c.set_world({{{10,0,0},2}},&planet,1);c.reset(532);pulse(c,ship);
        require(c.asteroid_hp()[0]==3&&c.events()[0].kind==SpaceCombatEventKind::PlanetImpact,"a coincident planet surface wins a deterministic contact tie");
    }
    {
        SpaceCombat c;c.set_world({{{203,0,0},1}},nullptr,0);c.set_assets(assets);c.reset(532);
        c.step(ship,true);for(int t=0;t<40;++t)c.step(ship,false);
        require(!active(c)&&c.asteroid_hp()[0]==3&&!c.event_count(),"range200 expires in free space before an out-of-range target");
        const auto* p=projectile(c,1);require(p&&std::abs(p->position.x-200)<.001f&&p->remaining_range==0,"last projectile step is clipped to range rather than overshooting");
        auto fast=assets;fast.projectile_speed=4000;c.set_assets(fast);c.set_world({{{50,0,0},.1f}},nullptr,0);c.reset(532);
        c.step(ship,true);c.step(ship,false);
        require(c.asteroid_hp()[0]==2&&c.event_count()==1&&std::abs(c.events()[0].position.x-49.9f)<.0001f,"a fast shot cannot tunnel through a tiny rock between two clear endpoints");
    }
    {
        const Destination planet{1,"Near impact",{4,0,0},1,DestinationKind::Planet};
        auto a=assets;a.cadence_ticks=1;SpaceCombat c;c.set_world({},&planet,1);c.set_assets(a);c.reset(532);
        c.step(ship,true);c.step(ship,true);
        const auto* retired=projectile(c,1);
        require(retired&&!retired->active&&std::abs(retired->lifetime-3.f/400)<1e-7f&&retired->retired_seconds>retired->birth_seconds,"CCD clips visual lifetime to actual first contact and retains the retired slot for this interpolation interval");
        require(c.projectiles()[0].id==1&&c.projectiles()[1].id==2,"a new shot cannot overwrite a projectile retired earlier in the same native tick");
        require(c.event_count()==1&&std::abs(c.events()[0].scene_seconds-(retired->birth_seconds+retired->lifetime))<1e-8,"impact visuals start at continuous contact time rather than the later tick boundary");
        c.step(ship,true);require(c.projectiles()[0].id==3,"a retired slot becomes reusable on the following native tick");
        c.clear_projectiles();bool cleared=true;for(const auto& p:c.projectiles())cleared=cleared&&!p.active&&p.lifetime==0;
        require(cleared,"death and transitions also invalidate recently retired visual projectiles");
    }
    {
        const Destination planet{1,"Impact",{10,0,0},2,DestinationKind::Planet};SpaceCombat c;
        c.set_world({},&planet,1);c.set_assets(assets);c.reset(532);
        for(int i=0;i<40;++i)pulse(c,ship);
        std::uint64_t minimum=std::numeric_limits<std::uint64_t>::max(),maximum=0;
        for(std::size_t i=0;i<c.event_count();++i){minimum=std::min(minimum,c.events()[i].id);maximum=std::max(maximum,c.events()[i].id);}
        require(c.event_count()==32&&minimum==9&&maximum==40,"bounded persistent impact ring retains the newest32events independent of draw/catchup partitioning");
        const auto event_revision=c.event_revision();c.step(ship,false);
        require(c.event_revision()==event_revision&&c.event_count()==32,"a later quiet tick does not erase impacts before a renderer can see them");
        c.clear_projectiles();require(c.event_count()==32,"clearing active bullets on ship death preserves recent impact visuals");
        c.reset(532);require(!c.event_count()&&c.event_revision()>event_revision,"new journey invalidates the old event ring explicitly");
    }
    {
        SpaceCombat c;c.set_world({{{40,0,0},3}},nullptr,0);c.set_assets(assets);c.reset(532);
        const auto before=allocations;bool okay=true;
        for(int t=0;t<10000;++t){if(t%500==0)c.reset(532);c.step(ship,t%11<8);
            okay=okay&&c.event_count()<=32&&active(c)<=128;}
        require(okay&&allocations==before,"ten thousand combat ticks, resets, hits, destruction and pool reuse allocate no heap memory");
        auto long_lived=assets;long_lived.projectile_speed=1;long_lived.projectile_range=100000;long_lived.cadence_ticks=1;
        c.set_world({},nullptr,0);c.set_assets(long_lived);c.reset(532);
        for(int t=0;t<200;++t)c.step(ship,true);
        require(c.state().shots==128&&active(c)==128,"a deliberately saturated projectile pool refuses extra spawns instead of allocating or overwriting live shots");
    }
    {
        std::array<bool,19> coordinates{};coordinates[1]=coordinates[3]=true;
        VoyageController a(test_planets,2),b(test_planets,2),c(test_planets,2);
        for(auto* v:{&a,&b,&c}){v->set_combat_assets(assets);v->begin(1,coordinates,532);v->departure_complete();}
        for(int t=0;t<240;++t)a.tick(1./60,{},false,false,true);
        for(int t=0;t<576;++t)b.tick(1./144,{},false,false,true);
        for(int t=0;t<16;++t)c.tick(.25,{},false,false,true);
        require(a.combat().state().shots==60&&a.combat().state().shots==b.combat().state().shots&&a.combat().state().shots==c.combat().state().shots,"60Hz,144Hz and capped250ms catchup produce exactly the same held-fire cadence");
        for(std::size_t i=0;i<a.combat().projectiles().size();++i){const auto& p=a.combat().projectiles()[i];const auto& q=b.combat().projectiles()[i];const auto& r=c.combat().projectiles()[i];
            require(p.id==q.id&&p.id==r.id&&p.active==q.active&&p.active==r.active&&close(p.position,q.position)&&close(p.position,r.position)&&p.birth_seconds==q.birth_seconds&&p.birth_seconds==r.birth_seconds,"projectile slots, CCD paths and render birth metadata are independent of frame partition");}
    }
    {
        std::array<bool,19> coordinates{};coordinates[1]=coordinates[3]=true;
        VoyageController v(test_planets,2);v.set_combat_assets(assets);v.set_asteroids(test_layout());
        require(v.begin(1,coordinates,532)&&v.begin_space_entry(),"armed voyage uses the same native departure lifecycle");
        for(int t=0;t<105;++t)v.tick(1./60,{},false,false,true);
        require(v.phase()==Phase::FreeFlight&&!v.combat().state().shots,"holding fire through the whole intro creates no cinematic shots or backlog");
        v.tick(1./60,{},false,false,true);const auto first_shot=v.combat().state().shots;
        const auto p=v.combat().projectiles();const double time=v.scene_seconds();
        for(int t=0;t<80;++t)v.tick(.25,{1,1,true},true,true,true);
        require(v.scene_seconds()==time&&v.combat().state().shots==first_shot,"pause freezes firing cadence, projectiles and scene time without accumulating elapsed work");
        for(std::size_t i=0;i<p.size();++i)require(close(p[i].position,v.combat().projectiles()[i].position),"pause preserves every projectile slot position");
        bool refused=false;try{v.set_combat_assets(assets);}catch(const std::logic_error&){refused=true;}
        require(refused,"weapon metadata cannot be replaced while the world is suspended");
        v.cancel();require(v.begin(1,coordinates,532)&&v.departure_complete(),"fresh armed voyage still supports immediate radial return");
        v.tick(1./60,{},false,false,true);const auto shots=v.combat().state().shots;
        v.tick(1./60,{},true,false,true);
        require(v.phase()==Phase::Approach&&v.combat().state().shots==shots&&!active(v.combat()),"Triangle entry stops bullets and does not fire on the first Approach tick");
        for(int t=0;t<100;++t)v.tick(1./60,{},false,false,true);
        require(v.phase()==Phase::Arrival&&v.combat().state().shots==shots,"Approach and Arrival cannot fire even if the trigger remains held");
        v.arrival_complete();require(v.begin(1,coordinates)&&v.departure_complete(),"legacy begin signature keeps native531 as its default");
        for(int t=0;t<20;++t)v.tick(1./60,{},false,false,true);
        require(!v.combat().state().armed&&!v.combat().state().shots,"legacy callers never arm the green ship accidentally");
    }
    {
        std::array<bool,19> coordinates{};coordinates[1]=coordinates[3]=true;
        VoyageController v(test_planets,2);v.set_combat_assets(assets);v.set_asteroids(test_layout());
        require(v.begin(1,coordinates,532)&&v.departure_complete(),"damage/combat coexist in a private pure-core fixture");
        const auto spawn=v.flight();const auto& rocks=v.asteroid_layout()->asteroids();const auto killed=rocks[0],wall=rocks[1];
        pose(v,killed.position-Vec3{30,0,0});
        for(int t=0;t<35;++t)v.tick(1./60,{},false,false,t<12);
        require(!v.combat().alive_mask()[0]&&v.combat().state().destroyed==1,"three timed native shots destroy the fixture rock before the ship reaches it");
        // Moving the complete owner must not leave Contacts pointing at the
        // old vector object; only its stable heap storage is borrowed.
        VoyageController moved(std::move(v));
        pose(moved,killed.position);moved.tick(1./60,{},false);
        require(!moved.asteroid_contacts().contacts&&!moved.combat().alive_mask()[0],"moving a voyage preserves the stable borrowed mask and ignores the destroyed rock");
        v=std::move(moved);
        pose(v,killed.position);v.tick(1./60,{},false);
        require(!v.asteroid_contacts().contacts&&v.ship_damage().hull==100,"the destroyed rock contributes no recovery or hull damage in the integrated voyage");
        const auto shots=v.combat().state().shots;const auto revision=v.combat().revision();const auto death_seconds=v.combat().destroyed_seconds()[0];
        const auto before=allocations;
        for(int hit=0;hit<4;++hit){
            if(hit){const_cast<FlightState&>(v.flight())=spawn;for(int t=0;t<60;++t)v.tick(1./60,{},false);}
            pose(v,wall.position-Vec3{wall.radius+AsteroidContacts::ship_radius+.15f,0,0});
            v.tick(1./60,{0,0,true},false);
        }
        require(v.ship_damage().destroyed&&!active(v.combat())&&!v.combat().alive_mask()[0],"ship death clears active bullets but never respawns destroyed rocks");
        for(int t=0;t<100;++t)v.tick(1./60,{},true,false,true);
        require(v.ship_damage().respawn_ready&&v.ship_damage().respawns==0&&v.combat().state().shots==shots,"held fire and catchup cannot skip final-black acknowledgement or shoot during destruction");
        require(v.acknowledge_destruction_frame(),"only a rendered final destruction frame authorizes the local respawn");
        v.tick(.25,{},false,true,true);require(v.ship_damage().destroyed,"a paused acknowledgement cannot perform a hidden respawn");
        const double old_time=v.scene_seconds();v.tick(1./60,{},false,false,true);
        require(v.phase()==Phase::Departure&&v.ship_damage().respawns==1&&v.scene_seconds()>old_time&&!v.combat().alive_mask()[0]&&v.combat().revision()==revision&&v.combat().destroyed_seconds()[0]==death_seconds,"local respawn preserves scene time, rock destruction, exact death timestamps and combat revision");
        for(int t=0;t<105;++t)v.tick(1./60,{},false,false,true);
        require(v.phase()==Phase::FreeFlight&&v.combat().state().shots==shots&&v.origin()==1&&v.destination_unlocked(3)&&allocations==before,"respawn intro preserves world progress and never queues cinematic shots or heap allocations");
        v.cancel();require(v.begin(1,coordinates,533)&&v.combat().alive_mask()[0]&&v.combat().alive_mask()[1]&&!v.combat().state().shots&&!v.combat().state().destroyed,"only the next distinct journey restores all asteroid health and resets combat counters");
    }
    std::printf("space_combat: %u muzzle, firing, CCD, destruction, masking, pause and respawn checks passed\n",checks);
}

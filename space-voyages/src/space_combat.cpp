// SPDX-License-Identifier: GPL-3.0-or-later
#include "space_combat.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace spacevoyages {
namespace {
constexpr auto no_asteroid=static_cast<std::size_t>(-1);
bool planet_hit(const Destination& p,Vec3 start,Vec3 end,float& time,Vec3& normal)noexcept {
    const double mx=double(start.x)-p.position.x,my=double(start.y)-p.position.y,mz=double(start.z)-p.position.z;
    const double dx=double(end.x)-start.x,dy=double(end.y)-start.y,dz=double(end.z)-start.z;
    const double c=mx*mx+my*my+mz*mz-double(p.radius)*p.radius;
    double t=0;
    if(c>0){
        const double a=dx*dx+dy*dy+dz*dz,b=mx*dx+my*dy+mz*dz;
        if(a==0||b>=0)return false;
        const double discriminant=b*b-a*c;if(discriminant<0)return false;
        const double divisor=-b+std::sqrt(discriminant);if(divisor<=0)return false;
        t=c/divisor;if(t<0||t>1)return false;
    }
    time=float(t);normal=unit({float(mx+dx*t),float(my+dy*t),float(mz+dz*t)},unit(start-end));return true;
}
bool valid_assets(const SpaceCombatAssets& assets)noexcept {
    if(!std::isfinite(assets.projectile_speed)||assets.projectile_speed<=0||assets.projectile_speed>100000.f||
       !std::isfinite(assets.projectile_range)||assets.projectile_range<=0||assets.projectile_range>100000.f||
       !assets.cadence_ticks||assets.cadence_ticks>3600)return false;
    for(const auto& ship:assets.ships){
        if(ship.count>ship.muzzles.size())return false;
        for(std::size_t i=0;i<ship.count;++i){const auto& muzzle=ship.muzzles[i];
            const float l2=length_squared(muzzle.direction);
            if(!finite(muzzle.position)||!finite(muzzle.direction)||!std::isfinite(l2)||l2<1e-10f)return false;}
    }
    return true;
}
}
Vec3 weapon_local_to_world(Vec3 local,const FlightState& ship,bool position)noexcept {
    const auto forward=direction(ship.model_yaw,ship.model_pitch);
    const Vec3 right{std::sin(ship.model_yaw),-std::cos(ship.model_yaw),0};
    const auto up=cross(right,forward);
    const auto banked_right=right*std::cos(ship.model_roll)-up*std::sin(ship.model_roll);
    const auto banked_up=up*std::cos(ship.model_roll)+right*std::sin(ship.model_roll);
    return (position?ship.position:Vec3{})+forward*local.x-banked_right*local.y+banked_up*local.z;
}
bool SpaceCombat::set_assets(const SpaceCombatAssets& assets)noexcept {
    if(!valid_assets(assets))return false;
    assets_=assets;
    state_.armed=active_weapon()!=nullptr;
    return true;
}
void SpaceCombat::set_world(const std::vector<Asteroid>& asteroids,const Destination* planets,std::size_t count){
    if((!planets&&count)||count>64)throw std::invalid_argument("invalid combat planet catalog");
    for(std::size_t i=0;i<count;++i)if(!finite(planets[i].position)||!std::isfinite(planets[i].radius)||planets[i].radius<=0)
        throw std::invalid_argument("invalid combat planet volume");
    auto index=std::make_unique<AsteroidSpatialIndex>(asteroids);
    alive_.resize(asteroids.size());hp_.resize(asteroids.size());destroyed_seconds_.resize(asteroids.size());scratch_.reserve(asteroids.size());hits_.reserve(asteroids.size());
    index_=std::move(index);planets_=planets;planet_count_=count;reset_profile(ship_profile_);
}
void SpaceCombat::clear_projectiles()noexcept {
    bool changed=false;
    for(auto& projectile:projectiles_)if(projectile.active||projectile.lifetime>0){
        projectile.active=false;projectile.lifetime=0;changed=true;}
    if(changed)++projectile_revision_;
}
void SpaceCombat::reset(int native_ship_class)noexcept {
    const auto identity=ship_identity_from_native_class(native_ship_class);
    reset_profile(identity?ship_profile(*identity):nullptr);
}
void SpaceCombat::reset(ShipIdentity identity)noexcept {reset_profile(ship_profile(identity));}
const ShipWeaponDefinition* SpaceCombat::active_weapon()const noexcept {
    if(!ship_profile_||!ship_profile_->weapons_allowed)return nullptr;
    const auto& weapon=assets_.ships[ship_profile_->slot];
    return weapon.armed&&weapon.count&&weapon.count<=weapon.muzzles.size()?&weapon:nullptr;
}
void SpaceCombat::reset_profile(const ShipProfile* profile)noexcept {
    ship_profile_=profile;
    clear_projectiles();state_={};cooldown_=0;next_muzzle_=0;clock_=0;
    event_count_=next_event_slot_=0;++event_revision_;
    state_.armed=active_weapon()!=nullptr;
    std::fill(alive_.begin(),alive_.end(),std::uint8_t(1));
    std::fill(destroyed_seconds_.begin(),destroyed_seconds_.end(),-1.);
    if(index_)for(std::size_t i=0;i<hp_.size();++i)hp_[i]=index_->asteroids()[i].radius<=4.5f?3:6;
    ++revision_;
}
void SpaceCombat::emit(SpaceCombatEventKind kind,Vec3 position,Vec3 normal,std::size_t asteroid,std::uint64_t projectile,double seconds)noexcept {
    const float radius=index_&&asteroid<index_->asteroids().size()?index_->asteroids()[asteroid].radius:0;
    events_[next_event_slot_]={kind,position,normal,asteroid,projectile,++next_event_id_,seconds,radius};
    next_event_slot_=(next_event_slot_+1)%events_.size();event_count_=std::min(event_count_+1,events_.size());++event_revision_;
}
void SpaceCombat::fire(const FlightState& ship)noexcept {
    if(!state_.armed||!finite(ship.position)||!std::isfinite(ship.model_yaw)||!std::isfinite(ship.model_pitch)||
       !std::isfinite(ship.model_roll)||!std::isfinite(ship.yaw)||!std::isfinite(ship.pitch)||!std::isfinite(ship.speed))return;
    const auto* selected=active_weapon();if(!selected)return;
    const auto& definition=*selected;
    const auto& muzzle=definition.muzzles[next_muzzle_%definition.count];
    for(auto& projectile:projectiles_)if(!projectile.active&&
        (!projectile.id||projectile.lifetime<=0||clock_+1e-10>=projectile.retired_seconds+1./60.)){
        const auto position=weapon_local_to_world(muzzle.position,ship,true);
        const auto velocity=unit(weapon_local_to_world(muzzle.direction,ship,false))*assets_.projectile_speed+
            direction(ship.yaw,ship.pitch)*ship.speed;
        const float speed=length(velocity);if(!finite(position)||!finite(velocity)||!std::isfinite(speed)||speed<1e-6f)return;
        projectile={position,position,velocity,assets_.projectile_range,++next_projectile_id_,true,position,clock_,assets_.projectile_range/speed};
        ++next_muzzle_;++state_.shots;++projectile_revision_;cooldown_=assets_.cadence_ticks;return;
    }
}
void SpaceCombat::step(const FlightState& ship,bool firing,double scene_seconds)noexcept {
    constexpr float dt=FlightController::native_step;
    clock_=std::isfinite(scene_seconds)&&scene_seconds>=0?scene_seconds:clock_+1./60.;
    for(auto& projectile:projectiles_)if(projectile.active){
        projectile.previous_position=projectile.position;
        const float speed=length(projectile.velocity),distance=std::min(speed*dt,projectile.remaining_range);
        const auto end=projectile.position+unit(projectile.velocity)*distance;
        float first_time=2;Vec3 normal{1,0,0};std::size_t asteroid=no_asteroid;bool planet=false;
        if(index_&&index_->sweep_sphere(projectile.position,end,0,hits_,scratch_))for(const auto& hit:hits_){
            if(!alive_[hit.index])continue;
            first_time=hit.time;normal=hit.normal;asteroid=hit.index;break;
        }
        for(std::size_t i=0;i<planet_count_;++i){float time;Vec3 candidate_normal;
            if(planet_hit(planets_[i],projectile.position,end,time,candidate_normal)&&time<=first_time){
                first_time=time;normal=candidate_normal;asteroid=no_asteroid;planet=true;}
        }
        if(first_time<=1){
            const float remaining=std::max(0.f,projectile.remaining_range-distance*first_time);
            projectile.lifetime=std::max(0.f,projectile.lifetime-remaining/speed);
            projectile.remaining_range=remaining;projectile.retired_seconds=clock_;
            projectile.position=lerp(projectile.position,end,first_time);projectile.active=false;++projectile_revision_;
            const double impact_seconds=projectile.birth_seconds+projectile.lifetime;
            if(planet)emit(SpaceCombatEventKind::PlanetImpact,projectile.position,normal,no_asteroid,projectile.id,impact_seconds);
            else {
                auto& hp=hp_[asteroid];if(hp>0)--hp;
                if(!hp){alive_[asteroid]=0;destroyed_seconds_[asteroid]=impact_seconds;++revision_;++state_.destroyed;
                    emit(SpaceCombatEventKind::Destroyed,projectile.position,normal,asteroid,projectile.id,impact_seconds);}
                else emit(SpaceCombatEventKind::Impact,projectile.position,normal,asteroid,projectile.id,impact_seconds);
            }
        }else {
            projectile.position=end;projectile.remaining_range=std::max(0.f,projectile.remaining_range-distance);
            if(projectile.remaining_range<=1e-5f){projectile.active=false;projectile.retired_seconds=clock_;++projectile_revision_;}
        }
    }
    if(cooldown_)--cooldown_;
    if(firing&&!cooldown_)fire(ship);
}
}

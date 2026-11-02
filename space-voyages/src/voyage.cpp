// SPDX-License-Identifier: GPL-3.0-or-later
#include "voyage.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace spacevoyages {
Vec3 guide_direction_toward(Vec3 heading,Vec3 inward,float fraction) noexcept {
    heading=unit(heading);inward=unit(inward);
    const float t=std::clamp(fraction,0.f,1.f);if(t==0)return heading;if(t==1)return inward;
    const float cosine=std::clamp(dot(heading,inward),-1.f,1.f);
    if(cosine>.999999f)return unit(lerp(heading,inward,t));
    Vec3 tangent=inward-heading*cosine;
    if(length_squared(tangent)<1e-10f){
        tangent=cross({0,0,1},heading);
        if(length_squared(tangent)<1e-10f)tangent=cross({0,1,0},heading);
    }
    tangent=unit(tangent);const float angle=std::acos(cosine)*t;
    return unit(heading*std::cos(angle)+tangent*std::sin(angle));
}
bool approach_corridor_clear(Vec3 start,const Destination& target,
    const Destination* entries,std::size_t count) noexcept {
    if(!entries||!count||!finite(start)||!finite(target.position)||!std::isfinite(target.radius))return false;
    const Vec3 radial=unit(start-target.position);
    const Vec3 end=target.position+radial*(target.radius+VoyageController::keepout_margin+2.f);
    const Vec3 segment=end-start;const float l2=length_squared(segment);
    for(std::size_t i=0;i<count;++i)if(&entries[i]!=&target){
        const auto& other=entries[i];
        const float t=l2>1e-9f?std::clamp(dot(other.position-start,segment)/l2,0.f,1.f):0.f;
        const float radius=other.radius+VoyageController::keepout_margin+1.f;
        if(length_squared(start+segment*t-other.position)<=radius*radius)return false;
    }
    return true;
}
bool VoyageController::approach_corridor_clear(const Destination& target)const noexcept {
    return spacevoyages::approach_corridor_clear(flight_.state().position,target,entries_,count_);
}
DestinationRoute VoyageController::resolved_route(int level)const noexcept {
    const auto* d=find_destination(entries_,count_,level);
    return d?resolve_destination_route(d->level,coordinates_,origin_route_,route_availability_):DestinationRoute{};
}
bool VoyageController::destination_unlocked(int level)const{return resolved_route(level).valid();}
VoyageController::VoyageController():VoyageController(galaxy_catalog().data(),galaxy_catalog().size()){}
VoyageController::VoyageController(const Destination* entries,std::size_t count)
    :entries_(entries),count_(count),catalog_valid_(valid_catalog(entries,count)){
    if(catalog_valid_)combat_.set_world({},entries_,count_);
}
void VoyageController::set_asteroids(AsteroidLayout layout,bool solid){
    if(phase_!=Phase::Ground)throw std::logic_error("Cannot replace asteroid geometry during a journey");
    if(!layout.complete())throw std::invalid_argument("Incomplete asteroid field layout");
    auto contacts=solid&&!layout.asteroids().empty()?std::make_unique<AsteroidContacts>(layout.asteroids()):nullptr;
    combat_.set_world(layout.asteroids(),entries_,count_);
    if(contacts)contacts->set_alive_mask(&combat_.alive_mask());
    asteroid_layout_=std::move(layout);asteroid_contacts_=std::move(contacts);scene_seconds_=0;
    asteroid_visual_=flight_.presentation_state();
}
void VoyageController::set_asteroid_damage(bool enabled){
    if(phase_!=Phase::Ground)throw std::logic_error("Cannot change ship damage during a journey");
    asteroid_damage_enabled_=enabled;
}
void VoyageController::set_combat_assets(const SpaceCombatAssets& assets){
    if(phase_!=Phase::Ground)throw std::logic_error("Cannot replace ship weapons during a journey");
    if(!combat_.set_assets(assets))throw std::invalid_argument("Invalid ship weapon definition");
}
void VoyageController::update_asteroid_visual(){
    if(!asteroid_contacts_)return;
    asteroid_visual_=flight_.presentation_state();
    auto& camera=asteroid_visual_.camera;
    const Vec3 native_eye=camera.position,native_arm=native_eye-asteroid_visual_.position;
    camera.position=asteroid_contacts_->camera(asteroid_visual_.position,native_eye);
    const Vec3 corrected_arm=camera.position-asteroid_visual_.position;
    if(dot(native_arm,corrected_arm)<0||length_squared(cross(unit(native_arm),unit(corrected_arm)))>1e-10f){
        // A very tight obstruction may require a shoulder position instead
        // of putting the eye inside the hull. Keep the same native look-at
        // point; ordinary chase-camera orientation remains bit-identical.
        const Vec3 forward=direction(camera.yaw,camera.pitch);
        const Vec3 aim=native_eye+forward*length(native_arm);
        const Vec3 look=unit(aim-camera.position,forward);
        camera.yaw=yaw_of(look);camera.pitch=pitch_of(look);
    }
}
bool VoyageController::unlocked(const Destination& d)const {
    return resolve_destination_route(d.level,coordinates_,origin_route_,route_availability_).valid();
}
int VoyageController::target_level()const noexcept{return target_route().native_level;}
bool VoyageController::departure_pose(const Destination& from,Vec3 preferred,Vec3& position,Vec3& facing)const {
    const float start_distance=from.radius+keepout_margin+12.f;
    const float entry_distance=FlightController::cruise_speed*space_entry_duration;
    const auto clear=[&](Vec3 heading){
        const Vec3 start=from.position+heading*start_distance,end=start+heading*entry_distance;
        if(length(start)>galaxy_soft_radius||length(end)>galaxy_soft_radius)return false;
        const Vec3 segment=end-start;const float l2=length_squared(segment);
        for(std::size_t i=0;i<count_;++i)if(&entries_[i]!=&from){
            const auto& other=entries_[i];const float r=other.radius+keepout_margin+22.f;
            const float t=std::clamp(dot(other.position-start,segment)/l2,0.f,1.f);
            if(length_squared(start+segment*t-other.position)<r*r+.01f)return false;
        }
        return true;
    };
    // Pick the clear sampled heading closest to the nearest unlocked contact,
    // with deterministic tie-breaking. Pitch alternatives can leave a crowded
    // map plane without inventing an altitude for any celestial body.
    constexpr float yaw_offsets[]={0,35,-35,70,-70,105,-105,140,-140,180};
    constexpr float pitch_offsets[]={0,35,-35,70,-70};
    const float yaw=yaw_of(preferred),pitch=pitch_of(preferred);bool found=false;float best=-2;
    for(float dy:yaw_offsets)for(float dp:pitch_offsets){
        const auto candidate=direction(yaw+dy*pi/180.f,std::clamp(pitch+dp*pi/180.f,-1.3962634f,1.3962634f));
        const float score=dot(candidate,preferred);
        if((!found||score>best+.00001f)&&clear(candidate)){found=true;best=score;facing=candidate;}
    }
    if(!found){
        // A conservative fallback: refuse takeover if no clear intro exists.
        // The native world and its ordinary travel remain available.
        return false;
    }
    position=from.position+facing*start_distance;return true;
}
bool VoyageController::begin(int origin,const std::array<bool,19>& coordinates,int native_ship_class){
    return begin(DestinationRoute{origin,DestinationVariant::Campaign},coordinates,native_ship_class);
}
bool VoyageController::begin(DestinationRoute origin_route,const std::array<bool,19>& coordinates,int native_ship_class){
    const auto identity=ship_identity_from_native_class(native_ship_class);
    return identity&&begin(origin_route,coordinates,*identity);
}
bool VoyageController::begin(int origin,const std::array<bool,19>& coordinates,ShipIdentity identity){
    return begin(DestinationRoute{origin,DestinationVariant::Campaign},coordinates,identity);
}
bool VoyageController::begin(DestinationRoute origin_route,const std::array<bool,19>& coordinates,ShipIdentity identity){
    if(phase_!=Phase::Ground||!catalog_valid_)return false;
    if(!ship_profile(identity))return false;
    if(!origin_route.valid()||(origin_route.variant==DestinationVariant::VeldinGarage&&!route_availability_.veldin_garage))return false;
    const int origin=origin_route.native_level;
    const auto* from=find_destination(entries_,count_,origin);if(!from)return false;
    // DestinationRoute::valid() excludes tutorial0. Its original escape is
    // separate from both the campaign-final and added-garage program18 mounts.
    const Destination* toward=nullptr;float nearest=std::numeric_limits<float>::max();
    for(std::size_t i=0;i<count_;++i)if(&entries_[i]!=from&&resolve_destination_route(entries_[i].level,coordinates,origin_route,route_availability_).valid()){
        const float d=length_squared(entries_[i].position-from->position);
        if(d<nearest){toward=&entries_[i];nearest=d;}
    }
    const Vec3 preferred=toward?unit(toward->position-from->position):unit(-from->position);
    Vec3 start{},facing{};if(!departure_pose(*from,preferred,start,facing))return false;
    flight_.reset(start,yaw_of(facing),pitch_of(facing));
    ship_identity_=identity;combat_.reset(identity);
    respawn_position_=start;respawn_yaw_=yaw_of(facing);respawn_pitch_=pitch_of(facing);
    ship_damage_.reset();asteroid_impact_={};destruction_frame_acknowledged_=false;
    scene_seconds_=0;if(asteroid_contacts_)asteroid_contacts_->reset();update_asteroid_visual();
    coordinates_=coordinates;origin_=origin;origin_route_=origin_route;target_route_={};phase_=Phase::Departure;
    candidate_=target_=nullptr;triangle_held_=triangle_pending_=entry_active_=false;pending_=-1;approach_time_=entry_time_=0;accumulator_=0;
    return true;
}
bool VoyageController::begin_space_entry(){
    if(phase_!=Phase::Departure||entry_active_)return false;
    entry_active_=true;entry_time_=0;accumulator_=0;return true;
}
bool VoyageController::departure_complete(){
    if(phase_!=Phase::Departure)return false;entry_active_=false;entry_time_=space_entry_duration;phase_=Phase::FreeFlight;
    ship_damage_.finish_respawn_intro();select_candidate();return true;
}
bool VoyageController::acknowledge_destruction_frame()noexcept {
    if(phase_!=Phase::FreeFlight||!ship_damage_.state().destroyed||!ship_damage_.state().respawn_ready||destruction_frame_acknowledged_)return false;
    destruction_frame_acknowledged_=true;return true;
}
void VoyageController::respawn_in_space(){
    if(!ship_damage_.respawn())return;
    flight_.reset(respawn_position_,respawn_yaw_,respawn_pitch_);
    if(asteroid_contacts_)asteroid_contacts_->reset();
    combat_.clear_projectiles();
    asteroid_impact_={};destruction_frame_acknowledged_=false;
    phase_=Phase::Departure;entry_active_=true;entry_time_=0;
    candidate_=target_=nullptr;target_route_={};pending_=-1;approach_time_=0;triangle_pending_=false;
    // Keep scene_seconds_, origin_, coordinates_ and the held-button latch.
    // This is a local pose reset in the same suspended-world scene, not travel.
    update_asteroid_visual();
}
void VoyageController::set_coordinates(const std::array<bool,19>& coordinates){coordinates_=coordinates;}
const Destination* VoyageController::candidate()const{return candidate_;}
bool VoyageController::candidate_unlocked()const{return candidate_&&unlocked(*candidate_);}
void VoyageController::select_candidate(){
    const auto position=flight_.state().position;
    if(candidate_&&length(position-candidate_->position)<=candidate_->radius+prompt_margin+hysteresis&&approach_corridor_clear(*candidate_))return;
    candidate_=nullptr;float closest=std::numeric_limits<float>::max();
    for(std::size_t i=0;i<count_;++i){
        const auto& d=entries_[i];const float distance=length(position-d.position)-d.radius;
        if(distance<=prompt_margin&&distance<closest&&approach_corridor_clear(d)){closest=distance;candidate_=&d;}
    }
}
void VoyageController::boundaries(Vec3 previous,float seconds){
    Vec3 position=flight_.state().position;
    Vec3 heading=direction(flight_.state().yaw,flight_.state().pitch);
    for(std::size_t i=0;i<count_;++i){
        const auto& planet=entries_[i];const float radius=planet.radius+keepout_margin;
        const Vec3 relative=position-planet.position;
        const float distance=length(relative);const Vec3 normal=unit(relative,unit(previous-planet.position));
        if(distance<radius+22.f&&dot(heading,normal)<0){
            // Sliding is geometry policy, replacing the mission arena collision.
            Vec3 tangent=heading-normal*dot(heading,normal);
            if(length_squared(tangent)<.001f)tangent=cross({0,0,1},normal);
            if(length_squared(tangent)<.001f)tangent=cross({0,1,0},normal);
            const float factor=std::clamp((radius+22.f-distance)/22.f,0.f,1.f);
            heading=unit(lerp(heading,unit(tangent),factor*.6f));
            flight_.constrain(position,heading,1.f-factor*.12f);
        }
        // Swept segment prevents tunnelling even on the capped250ms hitch.
        const Vec3 segment=position-previous;
        const float l2=length_squared(segment);
        const float t=l2>1e-9f?std::clamp(dot(planet.position-previous,segment)/l2,0.f,1.f):0.f;
        const bool crossed=length_squared(previous+segment*t-planet.position)<radius*radius;
        if(distance<radius||crossed){
            const Vec3 hit_normal=unit(previous+segment*t-planet.position,normal);
            position=planet.position+hit_normal*(radius+.02f);
            Vec3 tangent=heading-hit_normal*dot(heading,hit_normal);
            if(length_squared(tangent)<.001f)tangent=cross({0,0,1},hit_normal);
            if(length_squared(tangent)<.001f)tangent=cross({0,1,0},hit_normal);
            heading=unit(tangent);flight_.constrain(position,heading,.7f);
        }
    }
    const float distance=length(position);
    if(distance>galaxy_soft_radius){
        const Vec3 inward=unit(-position);
        const float weight=std::clamp((distance-galaxy_soft_radius)/(galaxy_radius-galaxy_soft_radius),0.f,1.f);
        heading=guide_direction_toward(heading,inward,weight*std::clamp(seconds*3.f,0.f,1.f));
        if(distance>galaxy_radius)position=unit(position)*galaxy_radius;
        flight_.constrain(position,heading,1.f-weight*.08f);
    }
    if(phase_==Phase::FreeFlight&&asteroid_contacts_){
        const auto moved=asteroid_contacts_->move(previous,flight_.state().position,seconds);
        asteroid_impact_={moved.impact_speed,moved.touching};
        if(moved.blocked||length_squared(moved.position-flight_.state().position)>1e-12f){
            const auto& state=flight_.state();
            // Contact never steers or banks the ship. The player keeps the
            // native controls; geometry slides/brakes the attempted motion.
            flight_.constrain(moved.position,direction(state.yaw,state.pitch),moved.blocked?.9f:1.f);
        }
    }
}
void VoyageController::tick(double seconds,const FlightInput& input,bool triangle,bool paused,bool fire){
    const bool pressed=triangle&&!triangle_held_;triangle_held_=triangle;
    if(paused){triangle_pending_=false;return;}
    if(phase_!=Phase::FreeFlight&&phase_!=Phase::Approach&&!(phase_==Phase::Departure&&entry_active_)){accumulator_=0;triangle_pending_=false;return;}
    // A button held during the cinematic must be released before it can enter
    // a destination when control is handed over.
    triangle_pending_=phase_!=Phase::Departure&&(triangle_pending_||pressed);
    if(!std::isfinite(seconds)||seconds<=0)return;
    accumulator_+=std::min(seconds,.25);
    constexpr double dt=1.0/60.0;
    while(accumulator_+1e-10>=dt){
        step(input,triangle_pending_,fire);triangle_pending_=false;accumulator_-=dt;
    }
}
void VoyageController::step(const FlightInput& input,bool pressed,bool fire){
    constexpr float dt=1.f/60.f;
    scene_seconds_+=1.0/60.0;
    asteroid_impact_={};
    const auto constraints=+[](void* owner,FlightController&,Vec3 previous){static_cast<VoyageController*>(owner)->boundaries(previous,FlightController::native_step);};
    if(phase_==Phase::Departure&&entry_active_){
        ship_damage_.tick(dt,{},false);
        flight_.native_tick({},constraints,this);
        entry_time_=std::min(entry_time_+dt,space_entry_duration);
        if(entry_time_+1e-5f>=space_entry_duration)departure_complete();
    }else if(phase_==Phase::FreeFlight){
        if(ship_damage_.state().destroyed){
            // Even a long catch-up frame must leave the final destruction
            // state visible until the presentation owner acknowledges it.
            if(ship_damage_.state().respawn_ready&&destruction_frame_acknowledged_)respawn_in_space();
            else ship_damage_.tick(dt,{},false);
            return;
        }
        flight_.native_tick(input,constraints,this);
        ship_damage_.tick(dt,asteroid_impact_,asteroid_damage_enabled_);
        if(ship_damage_.state().destroyed){
            candidate_=target_=nullptr;target_route_={};pending_=-1;approach_time_=0;triangle_pending_=false;destruction_frame_acknowledged_=false;
            combat_.clear_projectiles();
            update_asteroid_visual();return;
        }
        select_candidate();
        if(pressed&&candidate_unlocked()&&approach_corridor_clear(*candidate_)){
            target_=candidate_;target_route_=resolved_route(target_->level);approach_time_=0;phase_=Phase::Approach;
            approach_start_=flight_.state().position;approach_yaw_=flight_.state().yaw;approach_pitch_=flight_.state().pitch;
            const Vec3 radial=unit(approach_start_-target_->position);
            approach_end_=target_->position+radial*(target_->radius+keepout_margin+2.f);
            const Vec3 toward=-radial;approach_target_yaw_=yaw_of(toward);approach_target_pitch_=pitch_of(toward);
            combat_.clear_projectiles();
        }
        if(phase_==Phase::FreeFlight)combat_.step(flight_.state(),fire,scene_seconds_);
    }else if(phase_==Phase::Approach){
        ship_damage_.tick(dt,{},false);
        // Coordinates can change while another mod receives an event. Do not
        // commit the now-blocked route, nor fall through into native loading.
        if(!target_||resolved_route(target_->level)!=target_route_){phase_=Phase::FreeFlight;target_=nullptr;target_route_={};pending_=-1;approach_time_=0;return;}
        approach_time_=std::min(approach_time_+dt,approach_duration);
        const float t=approach_time_/approach_duration,s=t*t*(3.f-2.f*t);
        flight_.frame(lerp(approach_start_,approach_end_,s),angle_lerp(approach_yaw_,approach_target_yaw_,s),
                      angle_lerp(approach_pitch_,approach_target_pitch_,s));
        if(approach_time_+1e-5f>=approach_duration){phase_=Phase::Arrival;pending_=target_route_.native_level;}
    }
    update_asteroid_visual();
}
int VoyageController::take_destination(){
    if(pending_>=0&&(!target_||resolved_route(target_->level)!=target_route_)){
        cancel_arrival_to_flight();return -1;
    }
    const int result=pending_;pending_=-1;return result;
}
bool VoyageController::cancel_arrival_to_flight(){
    if(phase_!=Phase::Arrival)return false;
    phase_=Phase::FreeFlight;pending_=-1;target_=nullptr;target_route_={};approach_time_=0;
    accumulator_=0;triangle_pending_=false;
    // Preserve triangle_held_: neither an event callback nor a changed unlock
    // may manufacture a new press. Candidate hysteresis and the pose survive.
    select_candidate();return true;
}
bool VoyageController::arrival_complete(){if(phase_!=Phase::Arrival)return false;cancel();return true;}
void VoyageController::cancel(){phase_=Phase::Ground;origin_=-1;origin_route_=target_route_={};pending_=-1;candidate_=target_=nullptr;triangle_held_=triangle_pending_=entry_active_=false;approach_time_=entry_time_=0;accumulator_=0;scene_seconds_=0;if(asteroid_contacts_)asteroid_contacts_->reset();combat_.clear_projectiles();asteroid_visual_=flight_.presentation_state();ship_damage_.reset();asteroid_impact_={};destruction_frame_acknowledged_=false;respawn_position_={};respawn_yaw_=respawn_pitch_=0;}
}

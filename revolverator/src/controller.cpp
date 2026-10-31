// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "controller.hpp"
#include <algorithm>
#include <cmath>
namespace revolverator {
Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float length_squared(Vec3 a){return a.x*a.x+a.y*a.y+a.z*a.z;}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
bool capturable_class(int klass){
 switch(klass){
 case 35:case 1059:case 123:case 252:case 294:case 427:case 459:case 563:
 case 573:case 1382:case 577:case 580:case 623:case 1048:return true;
 default:return false; // Never catch NPCs, scenery, turrets, vehicles or bosses.
 }
}
const Controller::Flight* Controller::displayed_flight()const{
 const Flight* latest=nullptr;
 for(const auto& flight:flights_)if(flight.target&&(!latest||flight.sequence>latest->sequence))latest=&flight;
 return latest;
}
Controller::Flight* Controller::free_flight(){
 for(auto& flight:flights_)if(!flight.target)return &flight;
 return nullptr;
}
std::size_t Controller::flight_count()const{
 std::size_t count=0;for(const auto& flight:flights_)if(flight.target)++count;return count;
}
bool Controller::flying(Target target)const{
 if(!target)return false;
 for(const auto& flight:flights_)if(flight.target&&flight.target==target)return true;
 return false;
}
Phase Controller::phase()const{
 if(phase_==Phase::Windup||phase_==Phase::Spinning)return phase_;
 if(displayed_flight()&&(phase_==Phase::Idle||release_recovery_))return Phase::Flying;
 return phase_;
}
Target Controller::target()const{
 if(target_)return target_;
 if(phase_==Phase::Windup)return {};
 const auto* flight=displayed_flight();return flight?flight->target:Target{};
}
float Controller::charge()const{
 if(phase_==Phase::Spinning)return std::clamp(spin_time_/settings_.full_charge,0.f,1.f);
 const auto* flight=displayed_flight();return phase()==Phase::Flying&&flight?flight->charge:0.f;
}
float Controller::blast_charge()const{
 if(phase_==Phase::Spinning&&std::isfinite(settings_.maximum_spin)&&settings_.maximum_spin>0)
  return std::clamp(spin_time_/settings_.maximum_spin,0.f,1.f);
 const auto* flight=displayed_flight();return phase()==Phase::Flying&&flight?flight->blast_charge:0.f;
}
float Controller::recovery_seconds()const{
 return std::isfinite(settings_.recovery)?std::clamp(settings_.recovery,0.f,.15f):.15f;
}
void Controller::cancel_attack(){
 const bool animated=phase_!=Phase::Idle||bool(target_);
 if(target_&&world_.valid(target_))world_.release(target_,true);
 else if(target_)world_.discarded(target_);
 target_={};phase_=Phase::Idle;timer_=spin_time_=angle_=0;buffered_=release_recovery_=false;
 world_.attack_cancelled();
 if(animated)world_.animate(Motion::Recover);
}
void Controller::cancel(){
 ++reset_generation_;
 cancel_attack();
 for(auto& flight:flights_){
  const auto target=flight.target;flight={};
  if(target&&world_.valid(target))world_.release(target,true);
  else if(target)world_.discarded(target);
 }
 held_=true;world_.cancelled();
}
void Controller::discard(Target target){
 if(!target)return;
 if(target_&&target_==target){cancel_attack();recover();return;}
 for(auto& flight:flights_)if(flight.target&&flight.target==target){
  flight={};
  if(world_.valid(target))world_.release(target,true);else world_.discarded(target);
  return;
 }
}
void Controller::recover(){target_={};phase_=Phase::Recovery;timer_=0;release_recovery_=false;world_.animate(Motion::Recover);}
void Controller::advance_flights(const Frame& f){
 const auto generation=reset_generation_;
 bool finished=false;
 for(auto& slot:flights_){
  if(!slot.target)continue;
  if(!world_.valid(slot.target)){const auto stale=slot.target;slot={};world_.discarded(stale);finished=true;continue;}
  slot.elapsed+=f.seconds;slot.velocity.z-=12.f*f.seconds;
  const Vec3 next=slot.point+slot.velocity*f.seconds;
  const auto hit=world_.sweep(slot.target,slot.point,next);
  if(hit||slot.elapsed>=settings_.flight_seconds){
   const Flight flight=slot;slot={}; // Reentrant native death cannot retire this flight twice.
   const Vec3 point=hit&&finite(hit->point)?hit->point:flight.point;
   world_.move(flight.target,point,flight.yaw);
   world_.release(flight.target,false);
   const ImpactContext result{hit?ImpactReason::Collision:ImpactReason::FlightTimeout,point,
    hit&&finite(hit->normal)?hit->normal:Vec3{},
    hit&&hit->other?std::optional<Target>{hit->other}:std::nullopt,flight.charge};
   world_.impact_result(flight.target,result);++counters_.impacts;finished=true;
   if(generation!=reset_generation_)return;
  }else{
   slot.point=next;world_.move(slot.target,slot.point,std::atan2(slot.velocity.y,slot.velocity.x));
  }
 }
 // Finishing an older shot must never reset the pose or timer of a new attack.
 if(finished&&!target_&&phase_==Phase::Idle&&!displayed_flight())recover();
}
void Controller::tick(const Frame& f){
 world_.observe_frame(f);
 if(!have_epoch_){epoch_=f.epoch;have_epoch_=true;}
 if(f.epoch!=epoch_){cancel();epoch_=f.epoch;return;}
 if(!f.alive||!finite(f.position)||!std::isfinite(f.yaw)){cancel();return;}
 if(!f.equipped){
  if(phase_!=Phase::Idle||target_||buffered_)cancel_attack();
  held_=f.primary;
 }
 if(f.paused)return;
 if(!f.playing){cancel();return;}
 if(!std::isfinite(f.seconds)||f.seconds<=0||f.seconds>.1f){cancel();return;}
 const float dt=f.seconds;
 timer_+=dt;
 if(phase_==Phase::Recovery&&timer_+.000001f>=recovery_seconds()){
  phase_=Phase::Idle;timer_=0;release_recovery_=false;
 }
 const auto generation=reset_generation_;advance_flights(f);
 if(generation!=reset_generation_)return;
 if(!f.equipped)return;
 if(target_&&!world_.valid(target_)){const auto stale=target_;target_={};world_.discarded(stale);world_.attack_cancelled();recover();}
 const bool pressed=f.primary&&!held_;held_=f.primary;
 if(pressed&&phase_!=Phase::Spinning&&phase_!=Phase::Windup)buffered_=true;
 if(phase_==Phase::Idle&&buffered_&&f.grounded&&free_flight()&&world_.can_launch()){
  buffered_=false;
  if(world_.begin_attack()){phase_=Phase::Windup;timer_=0;world_.animate(Motion::Strike);}
  else world_.animate(Motion::DryFire);
  return;
 }
 switch(phase_){
 case Phase::Idle:case Phase::Recovery:break;
 case Phase::Windup:
  // A buffered tap can perform melee, but capture still requires a fresh,
  // uninterrupted minimum hold after the gesture has actually started.
  if(!f.primary){recover();break;}
  if(timer_>=std::max(settings_.windup,settings_.minimum_hold)){
   const auto found=world_.find_target(f.position,f.yaw,settings_.reach);
   if(found&&!flying(*found)&&world_.valid(*found)&&world_.capture(*found)){
    target_=*found;phase_=Phase::Spinning;spin_time_=timer_=0;angle_=f.yaw;
    point_=world_.carry_anchor(angle_).value_or(f.position+Vec3{0,0,settings_.orbit_height});
    world_.move(target_,point_,angle_);++counters_.captures;world_.animate(Motion::Raise);
   }else if(timer_>=settings_.capture_window)recover();
  }
  break;
 case Phase::Spinning:{
  spin_time_+=dt;
  const float power=charge();angle_=std::remainder(angle_+dt*(24.f+power*18.f),6.283185307f);
  point_=world_.carry_anchor(angle_).value_or(f.position+Vec3{0,0,settings_.orbit_height});
  world_.move(target_,point_,angle_);
  if((!f.primary&&spin_time_>=settings_.minimum_spin)||spin_time_>=settings_.maximum_spin){
   auto* flight=free_flight();if(!flight||!world_.can_launch())break;
   const float blast=blast_charge(),speed=13.f+power*9.f;
   *flight={target_,point_,{std::cos(f.yaw)*speed,std::sin(f.yaw)*speed,2.f+power*1.5f},0,power,blast,f.yaw,++sequence_};
   const auto victim=target_;target_={};phase_=Phase::Recovery;timer_=0;release_recovery_=true;
   ++counters_.throws;world_.launched(victim,power,blast);world_.animate(Motion::Release);
  }
  break;}
 case Phase::Flying:break; // Flying is synthesized by phase(); gestures never enter it.
 }
}
}

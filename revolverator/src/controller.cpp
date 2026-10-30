// SPDX-License-Identifier: MIT
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
float Controller::charge()const{
 if(phase_==Phase::Flying)return flight_charge_;
 return phase_==Phase::Spinning?std::clamp(spin_time_/settings_.full_charge,0.f,1.f):0.f;
}
void Controller::cancel(){
 if(target_&&world_.valid(target_))world_.release(target_,true);
 const bool animated=phase_!=Phase::Idle;
 target_={};phase_=Phase::Idle;timer_=spin_time_=angle_=0;held_=true;
 if(animated)world_.animate(Motion::Recover);
}
void Controller::recover(){target_={};phase_=Phase::Recovery;timer_=0;world_.animate(Motion::Recover);}
void Controller::tick(const Frame& f){
 if(!have_epoch_){epoch_=f.epoch;have_epoch_=true;}
 if(f.epoch!=epoch_){cancel();epoch_=f.epoch;return;}
 if(!f.equipped||!f.alive||!finite(f.position)||!std::isfinite(f.yaw)){cancel();return;}
 if(f.paused)return; // A paused tick neither advances physics nor loses its input latch.
 if(!f.playing){cancel();return;}
 if(!std::isfinite(f.seconds)||f.seconds<=0||f.seconds>.1f){cancel();return;}
 const float dt=f.seconds;
 if(target_&&!world_.valid(target_)){target_={};recover();}
 const bool pressed=f.primary&&!held_;held_=f.primary;
 timer_+=dt;
 switch(phase_){
 case Phase::Idle:
  if(pressed&&f.grounded){
   if(world_.begin_attack()){phase_=Phase::Windup;timer_=0;world_.animate(Motion::Strike);}
   else world_.animate(Motion::DryFire);
  }
  break;
 case Phase::Windup:
  // A tap is an ordinary melee strike. Capturing requires one uninterrupted
  // hold, including the frame that actually acquires the target.
  if(!f.primary){recover();break;}
  if(timer_>=std::max(settings_.windup,settings_.minimum_hold)){
   const auto found=world_.find_target(f.position,f.yaw,settings_.reach);
   if(found&&world_.valid(*found)&&world_.capture(*found)){
    target_=*found;phase_=Phase::Spinning;spin_time_=timer_=0;angle_=f.yaw;
    point_=world_.carry_anchor(angle_).value_or(f.position+Vec3{0,0,settings_.orbit_height});
    world_.move(target_,point_,angle_);++counters_.captures;world_.animate(Motion::Raise);
   }else if(timer_>=settings_.capture_window)recover();
  }
  break;
 case Phase::Spinning:{
  spin_time_+=dt;
  const float power=charge();angle_=std::remainder(angle_+dt*(8.f+power*10.f),6.283185307f);
  point_=world_.carry_anchor(angle_).value_or(f.position+Vec3{0,0,settings_.orbit_height});
  world_.move(target_,point_,angle_);
  if((!f.primary&&spin_time_>=settings_.minimum_spin)||spin_time_>=settings_.maximum_spin){
   flight_charge_=power;const float speed=13.f+power*9.f;
   velocity_={std::cos(f.yaw)*speed,std::sin(f.yaw)*speed,2.0f+power*1.5f};
   phase_=Phase::Flying;timer_=0;++counters_.throws;world_.animate(Motion::Release);
  }
  break;}
 case Phase::Flying:{
  velocity_.z-=12.f*dt;const Vec3 next=point_+velocity_*dt;
  const auto hit=world_.sweep(target_,point_,next);
  if(hit||timer_>=settings_.flight_seconds){
   if(hit&&finite(hit->point))point_=hit->point;
   world_.move(target_,point_,f.yaw);
   const auto victim=target_;
   world_.release(victim,false); // Restore AI/collision BEFORE posting the native damage.
   world_.impact(victim,hit&&hit->other?std::optional<Target>{hit->other}:std::nullopt,flight_charge_);
   ++counters_.impacts;recover();
  }else{point_=next;world_.move(target_,point_,std::atan2(velocity_.y,velocity_.x));}
  break;}
 case Phase::Recovery:if(timer_>=settings_.recovery){phase_=Phase::Idle;timer_=0;}break;
 }
}
}

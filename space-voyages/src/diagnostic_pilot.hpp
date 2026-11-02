// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "voyage.hpp"
#include <charconv>
#include <cstdlib>
#include <limits>
#include <string>
#include <sstream>
#include <vector>
namespace spacevoyages {
// Explicit, process-local QA input. Never grants coordinates or writes a pose.
// Without either QA environment variable all input comes from the player's pad.
class DiagnosticPilot {
 static constexpr std::size_t maximum_nodes=194;
 struct Obstacle {Vec3 center{};float radius{};};
 std::vector<int> destinations_;std::size_t next_{};
 bool asteroid_requested_{},asteroid_index_valid_{};std::size_t asteroid_index_{};
 std::uint64_t handoff_tick_{};
 bool handoff_completed_{},repeat_impacts_{},repeat_seen_{},retreating_{};
 std::uint32_t observed_damage_events_{},observed_respawns_{};Vec3 retreat_point_{};
 std::array<Obstacle,64> planets_{};std::size_t planet_count_{};
 std::array<Vec3,maximum_nodes> route_{};std::size_t route_count_{},waypoint_{};
 bool planned_{};int planned_origin_=-1,planned_target_=-1;std::uint64_t last_tick_{};
 static std::string environment(const char* name){
  std::string value;
#ifdef _WIN32
  char* raw=nullptr;std::size_t size=0;if(!_dupenv_s(&raw,&size,name)&&raw)value=raw;std::free(raw);
#else
  if(const auto* raw=std::getenv(name))value=raw;
#endif
  return value;
 }
 static float segment_distance_squared(Vec3 a,Vec3 b,Vec3 point){
  const auto delta=b-a;const float l2=length_squared(delta);
  const float t=l2>1e-9f?std::clamp(dot(point-a,delta)/l2,0.f,1.f):0.f;
  return length_squared(a+delta*t-point);
 }
 bool point_clear(Vec3 point,const AsteroidLayout* layout)const{
  if(!finite(point)||length(point)>VoyageController::galaxy_soft_radius-5.f)return false;
  for(std::size_t i=0;i<planet_count_;++i)if(length_squared(point-planets_[i].center)<planets_[i].radius*planets_[i].radius)return false;
  if(layout)for(const auto& rock:layout->asteroids()){
   const float radius=rock.radius+AsteroidContacts::ship_radius+16.f;
   if(length_squared(point-rock.position)<radius*radius)return false;
  }
  return true;
 }
 bool segment_clear(Vec3 a,Vec3 b,const AsteroidLayout* layout)const{
  for(std::size_t i=0;i<planet_count_;++i){
   const auto& planet=planets_[i];
   // A synthetic start may be inside the enlarged planning margin, but not
   // the native hard keepout. Permit a monotonic exit without hiding a body.
   if(length_squared(a-planet.center)<planet.radius*planet.radius&&dot(b-a,a-planet.center)>=0)continue;
   if(segment_distance_squared(a,b,planet.center)<planet.radius*planet.radius)return false;
  }
  if(layout)for(const auto& field:layout->fields()){
   const float bound=field.radius+AsteroidContacts::ship_radius+16.f;
   if(segment_distance_squared(a,b,field.center)>bound*bound)continue;
   for(std::size_t i=field.begin;i<field.begin+field.count;++i){
    const auto& rock=layout->asteroids()[i];const float radius=rock.radius+AsteroidContacts::ship_radius+16.f;
    if(segment_distance_squared(a,b,rock.position)<radius*radius)return false;
   }
  }
  return true;
 }
 void plan(const VoyageController& voyage,const Destination& target){
  planned_=true;planned_origin_=voyage.origin();planned_target_=target.level;route_count_=waypoint_=0;
  const auto start=voyage.flight().position;const auto* layout=voyage.asteroid_layout();
  planet_count_=std::min(voyage.catalog_count(),planets_.size());
  for(std::size_t i=0;i<planet_count_;++i){
   const auto& body=voyage.catalog_data()[i];
   const bool destination=celestial_body_for_level(body.level)==celestial_body_for_level(target.level);
   const bool origin=celestial_body_for_level(body.level)==celestial_body_for_level(voyage.origin());
   planets_[i]={body.position,body.radius+VoyageController::keepout_margin+(destination?4.f:origin?8.f:25.f)};
  }
  std::array<Vec3,maximum_nodes> nodes{};std::size_t count=2;nodes[0]=start;
  const float arrival_radius=target.radius+VoyageController::prompt_margin-8.f;
  nodes[1]=target.position+unit(start-target.position)*arrival_radius;
  if(!point_clear(nodes[1],layout))return;
  constexpr std::array<Vec3,6> axes{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
  const auto add=[&](Vec3 point){if(count<nodes.size()&&point_clear(point,layout))nodes[count++]=point;};
  // Field envelopes generate detour choices, never solid obstacles: their
  // empty authored passages and planet exclusion holes remain navigable.
  if(layout)for(const auto& field:layout->fields()){
   for(auto axis:axes)add(field.center+axis*(field.radius*1.4f+80.f));
   for(const auto& authored:asteroid_field_catalog())if(authored.id==field.id&&length_squared(authored.center-field.center)<.001f){
    const auto along=unit(authored.passage_axis)*(field.radius+80.f);add(field.center+along);add(field.center-along);
   }
  }
  for(std::size_t i=0;i<planet_count_;++i)for(auto axis:axes)add(planets_[i].center+axis*(planets_[i].radius*1.6f+35.f));
  std::array<float,maximum_nodes> distance;distance.fill(std::numeric_limits<float>::infinity());distance[0]=0;
  std::array<std::size_t,maximum_nodes> previous;previous.fill(maximum_nodes);
  std::array<bool,maximum_nodes> visited{};
  for(std::size_t pass=0;pass<count;++pass){
   std::size_t best=maximum_nodes;float nearest=std::numeric_limits<float>::infinity();
   for(std::size_t i=0;i<count;++i)if(!visited[i]&&distance[i]<nearest){best=i;nearest=distance[i];}
   if(best==maximum_nodes||best==1)break;
   visited[best]=true;
   for(std::size_t i=1;i<count;++i)if(!visited[i]){
    const float candidate=nearest+length(nodes[i]-nodes[best]);
    if(candidate<distance[i]&&segment_clear(nodes[best],nodes[i],layout)){distance[i]=candidate;previous[i]=best;}
   }
  }
  if(previous[1]==maximum_nodes)return;
  std::size_t node=1;
  while(node!=0&&node<maximum_nodes&&route_count_<route_.size()){route_[route_count_++]=nodes[node];node=previous[node];}
  if(node!=0){route_count_=0;return;}
  std::reverse(route_.begin(),route_.begin()+std::ptrdiff_t(route_count_));
 }
 static void aim(const FlightState& ship,Vec3 point,FlightInput& input){
  const auto delta=point-ship.position;
  input.horizontal=std::clamp(-wrap_angle(yaw_of(delta)-ship.yaw)*2.3f,-1.f,1.f);
  input.vertical=std::clamp(-(pitch_of(delta)-ship.pitch)*2.3f,-1.f,1.f);input.boost=true;
 }
public:
 DiagnosticPilot(){
  repeat_impacts_=environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS")=="1";
  const auto handoff=environment("SPACE_VOYAGES_QA_HANDOFF_TICK");
  if(!handoff.empty()){
   std::uint64_t tick{};const auto parsed=std::from_chars(handoff.data(),handoff.data()+handoff.size(),tick);
   if(parsed.ec==std::errc{}&&parsed.ptr==handoff.data()+handoff.size()&&tick>0)handoff_tick_=tick;
  }
  const auto asteroid=environment("SPACE_VOYAGES_QA_ASTEROID");asteroid_requested_=!asteroid.empty();
  if(asteroid_requested_){
   const auto parsed=std::from_chars(asteroid.data(),asteroid.data()+asteroid.size(),asteroid_index_);
   asteroid_index_valid_=parsed.ec==std::errc{}&&parsed.ptr==asteroid.data()+asteroid.size()&&asteroid_index_<16384;
  }
  auto value=environment("SPACE_VOYAGES_QA_TARGET");
  for(auto& c:value)if(c==',')c=' ';std::istringstream input(value);int target;
  while(input>>target)if(target>=0&&target<19)destinations_.push_back(target);
 }
 bool active()const{return asteroid_requested_?asteroid_index_valid_:!destinations_.empty();}
 void completed(){if(next_+1<destinations_.size())++next_;planned_=false;route_count_=waypoint_=0;repeat_seen_=retreating_=false;}
 void apply(const VoyageController& voyage,FlightInput& input,bool& triangle){
  // The benchmark then receives the untouched raw pad for every later tick;
  // no steering, boost or button edge survives from the diagnostic pilot.
  if(handoff_completed_)return;
  if(active()&&handoff_tick_&&voyage.flight().ticks>=handoff_tick_){handoff_completed_=true;return;}
  if(asteroid_requested_){
   const auto* layout=voyage.asteroid_layout();
   // An invalid explicit impact request never silently becomes a planet
   // autopilot. In particular it must not consume the player's Triangle.
   if(!asteroid_index_valid_||!layout||asteroid_index_>=layout->asteroids().size())return;
   const auto& rock=layout->asteroids()[asteroid_index_];const auto& damage=voyage.ship_damage();
   triangle=false;
   if(damage.destroyed){input={};return;}
   if(repeat_impacts_){
    if(!repeat_seen_||damage.respawns!=observed_respawns_||damage.damage_events<observed_damage_events_){
     repeat_seen_=true;retreating_=false;observed_respawns_=damage.respawns;observed_damage_events_=damage.damage_events;
    }
    if(damage.damage_events>observed_damage_events_){
     const auto outward=unit(voyage.flight().position-rock.position,-direction(voyage.flight().yaw,voyage.flight().pitch));
     retreat_point_=rock.position+outward*(rock.radius+AsteroidContacts::ship_radius+65.f);retreating_=true;
    }
    observed_damage_events_=damage.damage_events;
    if(retreating_&&length(voyage.flight().position-rock.position)>rock.radius+AsteroidContacts::ship_radius+50.f)retreating_=false;
   }
   aim(voyage.flight(),repeat_impacts_&&retreating_?retreat_point_:rock.position,input);return;
  }
  if(destinations_.empty())return;
  const int selected=destinations_[next_];const auto* target=find_destination(voyage.catalog_data(),voyage.catalog_count(),selected);if(!target)return;
  triangle=false;if(voyage.phase()!=Phase::FreeFlight)return;
  const auto& ship=voyage.flight();
  if(!planned_||planned_origin_!=voyage.origin()||planned_target_!=target->level||ship.ticks<last_tick_)plan(voyage,*target);
  last_tick_=ship.ticks;
  if(route_count_){
   // Shortcuts are accepted only against actual rock/planet volumes. Looking
   // ahead to a clear following segment starts turns before a waypoint can
   // produce a tight orbit under the unchanged native aircraft coefficients.
   for(std::size_t i=route_count_;i>waypoint_+1;--i)if(segment_clear(ship.position,route_[i-1],voyage.asteroid_layout())){waypoint_=i-1;break;}
   aim(ship,route_[waypoint_],input);
  }else aim(ship,target->position,input);
  triangle=voyage.candidate()&&voyage.candidate_unlocked()&&celestial_body_for_level(voyage.candidate()->level)==celestial_body_for_level(selected);
 }
};
}

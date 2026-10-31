// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "controller.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <array>
#include <functional>
#include <vector>
using namespace revolverator;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fake:World {
 int ammo=20,attempts=0,queries=0;float queried_reach=0;
 bool begin_attack()override{++attempts;if(ammo<=0)return false;--ammo;return true;}
 Target enemy{0x123400,0x456700,459,7}; bool exists=true,available=true,leased=false,collision=false;
 int takes=0,restores=0,hits=0;bool interrupted=false;Vec3 point{};std::vector<Motion> motions;
 std::optional<Target> find_target(Vec3,float,float reach)override{++queries;queried_reach=reach;return available?std::optional<Target>{enemy}:std::nullopt;}
 bool valid(Target t)const override{return exists&&t==enemy;}
 bool capture(Target)override{CHECK(!leased);leased=true;++takes;return true;}
 std::optional<Vec3> carry_anchor(float)override{return Vec3{1,2,3};}
 void move(Target,Vec3 p,float)override{CHECK(leased);CHECK(finite(p));point=p;}
 void release(Target,bool cancelled)override{CHECK(leased);leased=false;interrupted=cancelled;++restores;}
 std::optional<Contact> sweep(Target,Vec3,Vec3 to)override{return collision?std::optional<Contact>{{to,{}}}:std::nullopt;}
 void impact(Target,std::optional<Target>,float q)override{CHECK(!leased);CHECK(q>=0&&q<=1);++hits;}
 void animate(Motion m)override{motions.push_back(m);}
};
Frame frame(){Frame f;f.epoch=1;f.equipped=f.playing=f.alive=f.grounded=true;return f;}
void steps(Controller& c,Frame f,int count){while(count--)c.tick(f);}
void spin(Controller& c,Frame& f){f.primary=true;steps(c,f,15);CHECK(c.phase()==Phase::Spinning);}
struct MultiFake:World {
 struct Actor {Target target{};bool exists=true,leased=false,flying=false,collision=false;unsigned moves{};Vec3 position{};};
 std::array<Actor,16> actors{};
 struct Hit {Target target{};float charge{},blast{};ImpactReason reason{};};
 std::vector<Hit> impacts;std::vector<Motion> motions;
 std::function<void()> on_impact;
 int ammunition=64,attempts=0,captures=0,restores=0,all_cancels=0,gesture_cancels=0;
 std::array<float,16> charge{},blast{};
 Target force_target{};
 MultiFake(){for(unsigned i=0;i<actors.size();++i)actors[i].target={0x20000+i*256,0x40000+i*128,459,i+1};}
 Actor* actor(Target target){for(auto& a:actors)if(a.target==target)return &a;return nullptr;}
 const Actor* actor(Target target)const{for(const auto& a:actors)if(a.target==target)return &a;return nullptr;}
 bool begin_attack()override{++attempts;if(ammunition<=0)return false;--ammunition;return true;}
 std::optional<Target> find_target(Vec3,float,float)override{
  if(force_target)return force_target;
  for(const auto& a:actors)if(a.exists&&!a.leased&&!a.flying)return a.target;
  return {};
 }
 bool valid(Target target)const override{const auto* a=actor(target);return a&&a->exists;}
 bool capture(Target target)override{auto* a=actor(target);CHECK(a&&!a->leased&&!a->flying);a->leased=true;++captures;return true;}
 std::optional<Vec3> carry_anchor(float angle)override{return Vec3{std::cos(angle),std::sin(angle),2};}
 void move(Target target,Vec3 position,float)override{auto* a=actor(target);CHECK(a&&a->leased);CHECK(finite(position));a->position=position;++a->moves;}
 void release(Target target,bool)override{auto* a=actor(target);CHECK(a&&a->leased);a->leased=a->flying=false;++restores;}
 std::optional<Contact> sweep(Target target,Vec3,Vec3 to)override{const auto* a=actor(target);return a&&a->collision?std::optional<Contact>{{to,{},{0,0,1}}}:std::nullopt;}
 void launched(Target target,float q,float b)override{auto* a=actor(target);CHECK(a&&a->leased);a->flying=true;const auto i=std::size_t(a-actors.data());charge[i]=q;blast[i]=b;}
 void impact(Target,std::optional<Target>,float)override{}
 void impact_result(Target target,const ImpactContext& result)override{const auto* a=actor(target);CHECK(a&&!a->leased);const auto i=std::size_t(a-actors.data());impacts.push_back({target,result.charge,blast[i],result.reason});if(on_impact)on_impact();}
 void discarded(Target target)override{if(auto* a=actor(target))a->leased=a->flying=false;}
 void cancelled()override{++all_cancels;for(auto& a:actors)a.leased=a.flying=false;}
 void attack_cancelled()override{++gesture_cancels;}
 void animate(Motion motion)override{motions.push_back(motion);}
};
void launch(Controller& c,Frame& f){
 const auto before=c.counters().throws;f.primary=true;
 for(int i=0;i<60&&c.attack_phase()!=Phase::Spinning;++i)c.tick(f);
 CHECK(c.attack_phase()==Phase::Spinning);f.primary=false;
 for(int i=0;i<60&&c.counters().throws==before;++i)c.tick(f);
 CHECK(c.counters().throws==before+1);
}
int main(){
 {Fake w;Controller c(w);auto f=frame();spin(c,f);CHECK(w.takes==1);steps(c,f,90);CHECK(c.charge()>.9f);f.primary=false;c.tick(f);CHECK(c.phase()==Phase::Flying);w.collision=true;c.tick(f);CHECK(w.hits==1&&w.restores==1);steps(c,f,30);CHECK(c.phase()==Phase::Idle);CHECK(c.counters().throws==1);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);f.primary=false;steps(c,f,25);CHECK(c.phase()==Phase::Flying);CHECK(c.charge()<.5f);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);steps(c,f,200);CHECK(c.counters().throws==1);steps(c,f,200);CHECK(w.takes==1);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);float q=c.charge();f.paused=true;f.primary=false;steps(c,f,120);CHECK(c.phase()==Phase::Spinning&&c.charge()==q&&w.restores==0);f.paused=false;steps(c,f,25);CHECK(c.phase()==Phase::Flying);}
 for(int scenario=0;scenario<5;++scenario){Fake w;Controller c(w);auto f=frame();spin(c,f);if(scenario==0)f.equipped=false;if(scenario==1)f.alive=false;if(scenario==2)f.epoch++;if(scenario==3)f.playing=false;if(scenario==4)f.seconds=std::numeric_limits<float>::quiet_NaN();c.tick(f);CHECK(!w.leased&&w.restores==1&&w.interrupted);c.cancel();CHECK(w.restores==1);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);w.exists=false;c.tick(f);CHECK(w.restores==0);CHECK(c.phase()==Phase::Recovery);}
 {Fake w;Controller c(w);auto f=frame();w.available=false;f.primary=true;steps(c,f,100);CHECK(w.takes==0&&w.hits==0);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);f.primary=false;steps(c,f,120);CHECK(w.restores==1&&w.hits==1);}
 CHECK(capturable_class(459));CHECK(!capturable_class(1422));CHECK(!capturable_class(11));CHECK(!capturable_class(1106));CHECK(!capturable_class(-1));
 {Fake w;w.ammo=0;Controller c(w);auto f=frame();f.primary=true;steps(c,f,100);CHECK(c.phase()==Phase::Idle&&w.motions.size()==1&&w.motions[0]==Motion::DryFire&&w.takes==0&&w.queries==0&&w.ammo==0&&w.attempts==1);
  f.primary=false;c.tick(f);f.primary=true;steps(c,f,100);CHECK(w.motions.size()==2&&w.motions[1]==Motion::DryFire&&w.attempts==2&&w.takes==0&&w.queries==0);}
 {Fake w;w.ammo=1;Controller c(w);auto f=frame();spin(c,f);CHECK(w.ammo==0);steps(c,f,400);f.primary=false;steps(c,f,30);f.primary=true;steps(c,f,30);CHECK(w.takes==1&&w.ammo==0);}
 {Fake w;Controller c(w);auto f=frame();spin(c,f);CHECK(w.point.x==1&&w.point.y==2&&w.point.z==3);f.position={10,20,30};f.yaw=2;steps(c,f,30);CHECK(w.point.x==1&&w.point.y==2&&w.point.z==3);}
 {Fake w;Controller c(w);auto f=frame();f.primary=true;c.tick(f);
  CHECK(c.phase()==Phase::Windup&&w.ammo==19&&w.attempts==1&&w.motions.size()==1&&w.motions[0]==Motion::Strike&&w.queries==0);
  f.primary=false;steps(c,f,120);
  CHECK(c.phase()==Phase::Idle&&w.takes==0&&w.queries==0&&w.hits==0&&c.counters().throws==0&&w.ammo==19);}
 {Fake w;Controller c(w);auto f=frame();f.seconds=.08995f;f.primary=true;steps(c,f,3);
  CHECK(c.phase()==Phase::Windup&&w.queries==0&&w.takes==0);
  f.primary=false;c.tick(f);CHECK(c.phase()==Phase::Recovery&&w.queries==0&&w.takes==0);
  steps(c,f,40);CHECK(c.phase()==Phase::Idle&&c.counters().captures==0&&c.counters().throws==0);}
 {Fake w;Controller c(w);auto f=frame();f.seconds=.09f;f.primary=true;steps(c,f,2);CHECK(w.queries==0&&w.takes==0);
  c.tick(f);CHECK(c.phase()==Phase::Spinning&&w.queries==1&&w.takes==1&&w.ammo==19&&w.attempts==1);
  CHECK(std::abs(w.queried_reach-2.8f)<1.e-5f&&w.motions.back()==Motion::Raise);
  steps(c,f,100);CHECK(c.phase()==Phase::Idle&&w.attempts==1&&w.takes==1&&c.counters().throws==1);}
 {Fake w;Controller c(w);auto f=frame();f.primary=true;steps(c,f,6);f.primary=false;c.tick(f);
  f.primary=true;steps(c,f,100);CHECK(w.takes==1&&w.attempts==2&&w.ammo==18&&c.attack_phase()==Phase::Spinning);}
 {Fake w;Controller c(w);auto f=frame();w.available=false;f.seconds=.05f;f.primary=true;steps(c,f,8);
  CHECK(c.phase()==Phase::Windup&&w.queries>0&&w.takes==0);
  w.available=true;c.tick(f);CHECK(c.phase()==Phase::Spinning&&w.takes==1&&w.ammo==19);}
 {MultiFake w;Controller c(w);auto f=frame();launch(c,f);const auto first=w.actors[0].target;
  CHECK(c.phase()==Phase::Flying&&c.target()==first&&c.attack_phase()==Phase::Recovery&&c.flight_count()==1);
  const auto original_charge=c.charge(),original_blast=c.blast_charge();f.primary=true;c.tick(f);
  CHECK(w.attempts==1&&c.attack_phase()==Phase::Recovery);steps(c,f,7);CHECK(w.attempts==1);
  c.tick(f);CHECK(w.attempts==2&&c.attack_phase()==Phase::Windup&&c.phase()==Phase::Windup);
  CHECK(c.flight_count()==1&&c.flying(first)&&c.owns(first));
  steps(c,f,12);CHECK(c.attack_phase()==Phase::Spinning&&c.target()==w.actors[1].target);
  const auto moved=w.actors[0].moves;steps(c,f,5);CHECK(w.actors[0].moves>moved&&c.flight_count()==1);
  f.primary=false;for(int i=0;i<30&&c.counters().throws<2;++i)c.tick(f);
  CHECK(c.counters().throws==2&&c.flight_count()==2&&c.phase()==Phase::Flying&&c.target()==w.actors[1].target);
  CHECK(w.charge[0]==original_charge&&w.blast[0]==original_blast);
  w.actors[0].collision=true;const auto animations=w.motions.size();c.tick(f);
  CHECK(c.flight_count()==1&&w.impacts.size()==1&&w.impacts[0].target==first&&w.motions.size()==animations);
  CHECK(w.impacts[0].charge==original_charge&&w.impacts[0].blast==original_blast);
  c.tick(f);CHECK(w.impacts.size()==1);
  w.actors[1].collision=true;c.tick(f);CHECK(w.impacts.size()==2&&c.flight_count()==0&&w.restores==2);
 }
 {MultiFake w;Controller c(w);auto f=frame();launch(c,f);f.primary=true;c.tick(f);f.primary=false;
  steps(c,f,8);CHECK(w.attempts==2&&c.attack_phase()==Phase::Windup);
  c.tick(f);CHECK(c.attack_phase()==Phase::Recovery&&w.captures==1&&w.ammunition==62);
  steps(c,f,20);CHECK(w.captures==1&&c.counters().throws==1);}
 {MultiFake w;Controller c(w);auto f=frame();launch(c,f);steps(c,f,9);f.primary=true;c.tick(f);
  CHECK(w.attempts==2&&c.attack_phase()==Phase::Windup);f.equipped=false;c.tick(f);
  CHECK(c.flight_count()==1&&c.attack_phase()==Phase::Idle&&w.all_cancels==0&&w.actors[0].leased);
  w.actors[0].collision=true;c.tick(f);CHECK(w.impacts.size()==1&&c.flight_count()==0&&w.all_cancels==0);}
 {MultiFake w;Controller c(w);auto f=frame();launch(c,f);f.primary=true;c.tick(f);f.paused=true;
  const auto point=w.actors[0].position;const auto moves=w.actors[0].moves;steps(c,f,120);
  CHECK(c.flight_count()==1&&w.actors[0].moves==moves&&length_squared(point-w.actors[0].position)==0&&w.attempts==1);
  f.paused=false;steps(c,f,8);CHECK(w.attempts==2&&c.attack_phase()==Phase::Windup);}
 {MultiFake w;Settings settings;settings.flight_seconds=100;Controller c(w,settings);auto f=frame();launch(c,f);launch(c,f);
  const auto first=w.actors[0].target,second=w.actors[1].target;CHECK(c.flight_count()==2);
  c.discard(first);CHECK(c.flight_count()==1&&!w.actors[0].leased&&w.actors[1].leased&&w.impacts.empty());
  c.discard(first);CHECK(w.restores==1);w.actors[1].collision=true;c.tick(f);
  CHECK(w.impacts.size()==1&&w.impacts[0].target==second&&w.restores==2);}
 {MultiFake w;Settings settings;settings.flight_seconds=100;Controller c(w,settings);auto f=frame();launch(c,f);launch(c,f);
  w.actors[0].exists=false;c.tick(f);CHECK(c.flight_count()==1&&w.impacts.empty()&&!w.actors[0].leased&&w.actors[1].leased);
  c.cancel();CHECK(c.flight_count()==0&&!w.actors[1].leased&&w.impacts.empty()&&w.all_cancels==1);}
 {MultiFake w;Settings settings;settings.windup=settings.minimum_hold=settings.minimum_spin=.001f;settings.recovery=0;settings.flight_seconds=100;
  Controller c(w,settings);auto f=frame();for(unsigned i=0;i<maximum_flights;++i)launch(c,f);
  CHECK(c.flight_count()==maximum_flights&&w.attempts==int(maximum_flights));
  f.primary=true;steps(c,f,20);CHECK(c.flight_count()==maximum_flights&&w.attempts==int(maximum_flights));
  w.actors[0].collision=true;c.tick(f);CHECK(c.flight_count()==maximum_flights-1&&w.attempts==int(maximum_flights)+1&&c.attack_phase()==Phase::Windup);
  CHECK(w.ammunition==64-int(maximum_flights)-1);c.cancel();CHECK(c.flight_count()==0&&w.restores==int(maximum_flights));}
 {MultiFake w;Settings settings;settings.flight_seconds=100;Controller c(w,settings);auto f=frame();launch(c,f);const auto first=w.actors[0].target;
  w.force_target=first;f.primary=true;steps(c,f,60);CHECK(w.captures==1&&c.flight_count()==1&&w.attempts==2);}
 for(unsigned event=0;event<3;++event){MultiFake w;Settings settings;settings.flight_seconds=100;Controller c(w,settings);auto f=frame();launch(c,f);launch(c,f);
  if(event==0)f.alive=false;
  if(event==1)f.playing=false;
  if(event==2)++f.epoch;
  c.tick(f);CHECK(c.flight_count()==0&&w.restores==2&&w.impacts.empty());c.cancel();CHECK(w.restores==2);}
 {MultiFake w;Settings settings;settings.flight_seconds=100;Controller c(w,settings);auto f=frame();launch(c,f);launch(c,f);
  w.on_impact=[&]{c.cancel();};w.actors[0].collision=w.actors[1].collision=true;c.tick(f);
  CHECK(c.phase()==Phase::Idle&&c.flight_count()==0&&w.impacts.size()==1&&w.restores==2&&w.all_cancels==1);}
 std::puts("controller: held/tap capture, buffered rapid reuse, eight independent flights, per-target discard, provenance, pause, equip switch, teardown and legacy regressions passed");
}

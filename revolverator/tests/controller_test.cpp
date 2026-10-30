// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "controller.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
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
  f.primary=true;steps(c,f,100);CHECK(w.takes==0&&w.queries==0&&w.attempts==1&&w.ammo==19&&c.phase()==Phase::Idle);
  f.primary=false;c.tick(f);spin(c,f);CHECK(w.takes==1&&w.attempts==2&&w.ammo==18);}
 {Fake w;Controller c(w);auto f=frame();w.available=false;f.seconds=.05f;f.primary=true;steps(c,f,8);
  CHECK(c.phase()==Phase::Windup&&w.queries>0&&w.takes==0);
  w.available=true;c.tick(f);CHECK(c.phase()==Phase::Spinning&&w.takes==1&&w.ammo==19);}
 std::puts("controller: held capture, melee taps, dry fire, charge, release, impact, timeout, pause, interruption, stale targets and boss exclusions passed");
}

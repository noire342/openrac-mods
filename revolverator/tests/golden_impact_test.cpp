// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "golden_impact.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* what){
 ++checks;if(!condition){std::fprintf(stderr,"golden impact: %s\n",what);std::exit(1);}
}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
constexpr Target projectile{0x20000,0x30000,459,7};
struct WorldFake final:World {
 bool exists=true,leased=false,collision=false;unsigned throws=0,impacts=0,cancellations=0;
 float release_charge{},blast_charge{};ImpactContext result{};Vec3 point{},release_point{};
 bool begin_attack()override{return true;}
 std::optional<Target> find_target(Vec3,float,float)override{return projectile;}
 bool valid(Target target)const override{return exists&&target==projectile;}
 bool capture(Target)override{leased=true;return true;}
 std::optional<Vec3> carry_anchor(float)override{return Vec3{0,0,2};}
 void move(Target,Vec3 position,float)override{point=position;}
 void release(Target,bool cancelled)override{leased=false;if(cancelled)++cancellations;}
 std::optional<Contact> sweep(Target,Vec3,Vec3 to)override{return collision?std::optional<Contact>{{to,{},{0,0,1}}}:std::nullopt;}
 void impact(Target,std::optional<Target>,float)override{}
 void impact_result(Target,const ImpactContext& hit)override{require(!leased,"lease restored before impact dispatch");++impacts;result=hit;}
 void launched(Target,float charge,float blast)override{++throws;release_charge=charge;blast_charge=blast;release_point=point;}
 void animate(Motion)override{}
};
Frame frame(){Frame f;f.equipped=f.alive=f.playing=f.grounded=true;f.epoch=1;return f;}
void tick(Controller& controller,Frame f,unsigned count){while(count--)controller.tick(f);}
void begin_spin(Controller& controller,Frame& f){f.primary=true;tick(controller,f,15);require(controller.phase()==Phase::Spinning,"fixture reaches capture");}
}
int main(){
 GoldenImpact flight;
 flight.launch(projectile,true,0,1);auto minimum=flight.consume(projectile,ImpactReason::Collision);
 require(minimum&&near(minimum->radius,2.5f)&&near(minimum->damage,8)&&near(minimum->visual_scale,1.2f),"minimum blast profile");
 require(!flight.consume(projectile,ImpactReason::Collision),"duplicate collision cannot re-arm blast");
 flight.launch(projectile,true,1,1);auto maximum=flight.consume(projectile,ImpactReason::Collision);
 require(maximum&&near(maximum->radius,6)&&near(maximum->damage,16)&&near(maximum->visual_scale,3),"maximum blast profile");
 require(maximum->radius>minimum->radius*2&&maximum->visual_scale>minimum->visual_scale*2,"charge changes visible and damage radius substantially");
 require(near(maximum->damage_at(0),16)&&near(maximum->damage_at(3),10.8f)&&maximum->damage_at(5.999f)>5.6f,"radial falloff preserves centre and soft edge");
 require(maximum->damage_at(6)==0&&maximum->damage_at(-1)==0&&maximum->damage_at(std::numeric_limits<float>::quiet_NaN())==0,"invalid/outside distances never cause damage");
 for(unsigned i=0;i<=300;++i){
  flight.launch(projectile,true,float(i)/300,1);const auto profile=flight.consume(projectile,ImpactReason::Collision);
  require(profile&&profile->radius>=2.5f&&profile->radius<=6&&profile->damage>=8&&profile->damage<=16,"all charge values remain bounded");
  if(i){flight.launch(projectile,true,float(i-1)/300,1);const auto previous=flight.consume(projectile,ImpactReason::Collision);
   require(profile->radius>previous->radius&&profile->visual_scale>previous->visual_scale,"radius and FX grow through the last hold interval");}
 }
 for(auto reason:{ImpactReason::Collision,ImpactReason::FlightTimeout}){
  flight.launch(projectile,false,1,1);require(!flight.consume(projectile,reason),"base shot has no Golden effect");
 }
 flight.launch(projectile,true,1,1);require(!flight.consume(projectile,ImpactReason::FlightTimeout)&&!flight.pending(projectile),"flight timeout consumes without explosion");
 flight.launch(projectile,true,1,1);flight.cancel();require(!flight.consume(projectile,ImpactReason::Collision),"cancel consumes without explosion");
 auto recycled=projectile;++recycled.stamp;
 flight.launch(projectile,true,.5f,2);require(!flight.consume(recycled,ImpactReason::Collision)&&flight.pending(projectile),"recycled identity cannot consume a shot");
 const auto boosted=flight.consume(projectile,ImpactReason::Collision);require(boosted&&near(boosted->damage,24),"damage multiplier is latched into profile");
 for(float q:{-10.f,10.f}){flight.launch(projectile,true,q,100);auto p=flight.consume(projectile,ImpactReason::Collision);require(p&&p->radius>=2.5f&&p->radius<=6&&p->damage<=256,"untrusted charge and multiplier bounded");}
 flight.launch(projectile,true,std::numeric_limits<float>::infinity(),1);require(!flight.pending(projectile),"nonfinite launch rejected");
 require(golden_damage_class(1106)&&golden_damage_class(1422)&&!capturable_class(1106)&&!capturable_class(1422),"bosses damageable but never capturable");
 for(unsigned c:{0u,13u,71u,180u,2017u,32000u,531u,532u,533u,9999u})require(!golden_damage_class(c),"hero/NPC/weapon/pickup/scenery excluded");
 for(unsigned c:{459u,1051u,1106u,1422u,500u,501u,502u,505u,507u,508u,509u,510u,511u})require(golden_damage_class(c),"verified combat/destructible actors admitted");
 {
  GoldenBlastPlan plan(*maximum,{0,0,0});
  require(plan.consider(projectile,{0,0,0},0,true,true),"projectile admitted once");
  require(!plan.consider(projectile,{0,0,0},0,true,true),"duplicate target omitted");
  require(!plan.consider(recycled,{0,0,0},0,true,true),"same address cannot receive two native hits");
  require(!plan.consider({0x21000,1,459,1},{1,0,0},0,false),"occluded enemy omitted");
  require(!plan.consider({0x22000,1,13,1},{1,0,0},0,true),"visible NPC omitted");
  require(plan.consider({0x23000,1,1106,1},{7,0,0},2,true),"large boss surface inside radius admitted");
  require(!plan.consider({0x24000,1,459,1},{7,0,0},0,true),"actor outside blast omitted");
  require(plan.size()==2&&finite(plan.hits()[0].direction)&&near(plan.hits()[0].damage,16),"zero-distance target produces finite full-strength packet");
 }
 {
  GoldenBlastPlan plan(*maximum,{0,0,0});
  for(unsigned i=0;i<200;++i)plan.consider({0x40000+i*256,1,459,i+1},{5.f-float(i)*.02f,0,0},0,true);
  require(plan.size()==GoldenBlastPlan::maximum_hits,"single burst fits bounded native hit ring");
  require(plan.consider(projectile,{5.9f,0,0},0,true,true),"primary actor retains priority in crowded scene");
  require(plan.hits()[0].target==projectile,"primary actor retained before nearest crowd");
  for(std::size_t i=2;i<plan.size();++i)require(plan.hits()[i].distance>=plan.hits()[i-1].distance,"crowd uses nearest visible actors");
 }
 {
  WorldFake world;Controller controller(world);auto f=frame();begin_spin(controller,f);
  tick(controller,f,104);require(controller.charge()==1&&controller.blast_charge()<.7f,"throw speed reaches its original cap before blast");
  const float earlier=controller.blast_charge();tick(controller,f,30);
  require(controller.charge()==1&&controller.blast_charge()>earlier+.15f,"Golden blast keeps growing after original speed cap");
  tick(controller,f,80);require(world.throws==1&&near(world.release_charge,1)&&near(world.blast_charge,1),"automatic release latches both charges once");
  f.equipped=false;world.collision=true;controller.tick(f);
  require(world.impacts==1&&world.cancellations==0&&world.result.reason==ImpactReason::Collision,"already launched projectile survives equip switch");
 }
 {
  WorldFake world;Controller controller(world);auto f=frame();begin_spin(controller,f);
  f.primary=false;tick(controller,f,20);require(world.throws==1,"manual release starts immediately at minimum spin");
  f.paused=true;const auto point=world.point;tick(controller,f,100);
  require(world.impacts==0&&length_squared(world.point-point)==0,"pause freezes projectile lifetime");
  f.paused=false;f.equipped=false;tick(controller,f,120);
  require(world.impacts==1&&world.result.reason==ImpactReason::FlightTimeout,"timeout distinguished from terrain contact");
 }
 for(unsigned event=0;event<3;++event){
  WorldFake world;Controller controller(world);auto f=frame();begin_spin(controller,f);f.primary=false;tick(controller,f,20);
  if(event==0)f.alive=false;else if(event==1)f.playing=false;else ++f.epoch;
  controller.tick(f);require(world.impacts==0&&world.cancellations==1&&!world.leased,"death/scene/travel cancel flight without impact");
 }
 std::printf("golden impact: %u checks passed\n",checks);
}

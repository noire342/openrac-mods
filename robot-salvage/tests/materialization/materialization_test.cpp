// SPDX-License-Identifier: GPL-3.0-or-later
#include "materialization_policy.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace salvage;
namespace {
unsigned checks{};
void require(bool yes,const char* message){++checks;if(!yes){std::fprintf(stderr,"materialization: %s\n",message);std::exit(1);}}
void thresholds(){
 World world;world.begin_visit(1);MaterializationPolicy policy;
 const auto near=world.spawn(1,{{45,0,0}}),border=world.spawn(1,{{45.0005f,0,0}}),retained=world.spawn(1,{{55,0,0}}),far=world.spawn(1,{{55.0005f,0,0}});
 require(policy.select(world,{},{} )==std::vector<InstanceId>{near},"new materialization includes45 and excludes points beyond45");
 require(policy.select(world,{}, {far,retained,border,border,0,999999})==std::vector<InstanceId>({near,border,retained}),"existing actors retain through55; duplicates and stale handles cannot consume slots");
 require(world.all().size()==4,"threshold selection never deletes logical records");
 WorldLease claim;require(world.claim(near,world.visit(),claim)==Status::Ok,"claim fixture obtains custody");
 require(policy.select(world,{}, {near,retained})==std::vector<InstanceId>{retained},"claimed instances are excluded even if their prior actor was materialized");
 require(world.owns(claim)&&world.find(near),"policy cannot cancel custody or remove a claimed record");
 world.release(claim);require(policy.select(world,{},{} )==std::vector<InstanceId>{near},"released instance becomes eligible again");
 auto* part=world.find(near);part->sleeping=true;part->suspended=true;
 require(policy.select(world,{},{} )==std::vector<InstanceId>{near},"sleeping and stale-suspended flags do not hide a nearby piece");
 world.begin_visit(1);require(policy.select(world,{},{} )==std::vector<InstanceId>{near},"same-planet respawn retains the logical candidates");
 world.begin_visit(2,true);const auto next=world.spawn(1,{{50,0,0}});
 require(policy.select(world,{}, {near}).empty()&&next!=near,"old visit materializations cannot give a new instance retention distance");
}
void priorities(){
 World world;world.begin_visit(1);MaterializationPolicy one({45,55,1});
 const auto first=world.spawn(1,{{1,0,0}}),second=world.spawn(2,{{-1,0,0}}),old_far=world.spawn(1,{{50,0,0}});
 require(one.select(world,{}, {old_far,second})==std::vector<InstanceId>{first},"nearest pieces displace farther retained actors, with lower-ID tie breaking");
 require(one.select(world,{}, {second,old_far,second})==one.select(world,{}, {old_far,second}),"current actor enumeration order never changes stable ties");
 require(one.select(world,{}, {},[](const Fragment& f){return f.part==2;})==std::vector<InstanceId>{second},"unavailable models are excluded before assigning budget");
 const auto ids=one.select(world,{}, {old_far},[](const Fragment&){return false;});require(ids.empty()&&world.all().size()==3,"zero eligible resources retain every logical component");
 bool threw=false;try{one.select(world,{}, {},[](const Fragment&)->bool{throw std::runtime_error("resource query failed");});}catch(const std::runtime_error&){threw=true;}
 require(threw&&world.all().size()==3&&world.find(old_far),"a failed adapter query cannot partially mutate World");
}
void large_accumulation(){
 World world;world.begin_visit(7);std::vector<InstanceId> old;std::vector<std::pair<double,InstanceId>> oracle;
 for(unsigned i=0;i<10000;++i){const float x=float(int(i%100)-50),y=float(int(i/100)-50);const auto id=world.spawn(i%2+1,{{x,y,0}});require(id!=0,"ten-thousand logical records are created independently of native pool");
  auto* part=world.find(id);part->sleeping=(i%2)!=0;part->suspended=(i%3)==0;if(i<150)old.push_back(id);
  if(double(x)*x+double(y)*y<=45.*45.)oracle.emplace_back(double(x)*x+double(y)*y,id);
 }
 std::sort(oracle.begin(),oracle.end());MaterializationPolicy policy;const auto before=world.all().size();
 const auto desired=policy.select(world,{},{});require(desired.size()==96,"ten-thousand records use at most96 materializations");
 for(unsigned i=0;i<desired.size();++i)require(desired[i]==oracle[i].second,"bounded selector agrees with full-sort nearest-ID oracle");
 const auto again=policy.select(world,{},old);require(again==desired,"farther old actors never evict the nearest96");
 std::vector<WorldLease> leases;for(unsigned i=0;i<16;++i){WorldLease lease;require(world.claim(desired[i],world.visit(),lease)==Status::Ok,"nearby claimed fixture");leases.push_back(lease);}
 const auto claimed=policy.select(world,{},desired);require(claimed.size()==96,"claimed components do not exhaust native materialization capacity");
 for(unsigned i=0;i<claimed.size();++i)require(claimed[i]==oracle[i+16].second,"nearest unclaimed candidates replace leased parts in deterministic order");
 require(policy.select(world,{1000,1000,0},desired).empty(),"leaving an accumulation dematerializes its native actors");
 require(world.all().size()==before&&world.find(desired.front())&&world.owns(leases.front()),"leaving preserves every piece and its custody metadata");
 for(const auto& [id,part]:world.all()){require(part.sleeping==((id-1)%2!=0)&&part.suspended==((id-1)%3==0),"selection never rewrites sleeping/suspension state");}
 require(policy.select(world,{},{}).size()==96,"returning can rematerialize the retained logical accumulation");
}
void invalid_inputs(){
 World world;world.begin_visit(1);world.spawn(1,{{0,0,0}});MaterializationPolicy disabled({45,55,0});
 require(disabled.select(world,{},{}).empty()&&world.all().size()==1,"budget0 disables actors without deleting records");
 MaterializationPolicy policy;require(policy.select(world,{std::numeric_limits<float>::quiet_NaN(),0,0},{}).empty(),"invalid player position cannot enter spatial ordering");
 world.find(1)->position.x=std::numeric_limits<float>::infinity();require(policy.select(world,{},{}).empty()&&world.find(1),"invalid logical positions are skipped without record destruction");
 for(const MaterializationSettings settings: {MaterializationSettings{0,55,96},{45,44,96},{45,std::numeric_limits<float>::infinity(),96},{std::numeric_limits<float>::quiet_NaN(),55,96}}){bool threw=false;try{MaterializationPolicy wrong(settings);}catch(const std::invalid_argument&){threw=true;}require(threw,"invalid hysteresis settings are rejected before adapter work");}
}
}
int main(){thresholds();priorities();large_accumulation();invalid_inputs();std::printf("robot salvage materialization: %u checks passed\n",checks);}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "materialization_policy.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace salvage {
MaterializationPolicy::MaterializationPolicy(MaterializationSettings settings):settings_(settings){
 if(!std::isfinite(settings_.active_distance)||settings_.active_distance<=0||
    !std::isfinite(settings_.retain_distance)||settings_.retain_distance<settings_.active_distance)
  throw std::invalid_argument("Invalid component materialization distances");
}
std::vector<InstanceId> MaterializationPolicy::select(const World& world,Vec3 player,
 const std::vector<InstanceId>& currently_materialized,const Eligible& eligible)const{
 if(!settings_.budget||!finite(player)||world.all().empty())return {};
 auto current=currently_materialized;
 std::sort(current.begin(),current.end());current.erase(std::unique(current.begin(),current.end()),current.end());
 struct Candidate {double distance;InstanceId id;};
 const auto better=[](const Candidate& a,const Candidate& b){return a.distance<b.distance||(a.distance==b.distance&&a.id<b.id);};
 const auto budget=std::min(settings_.budget,world.all().size());
 std::vector<Candidate> nearest;nearest.reserve(budget);
 const double active2=double(settings_.active_distance)*settings_.active_distance,
              retain2=double(settings_.retain_distance)*settings_.retain_distance;
 for(const auto& [id,part]:world.all()){
  if(world.claimed(id)||!finite(part.position))continue;
  const double dx=double(part.position.x)-player.x,dy=double(part.position.y)-player.y,dz=double(part.position.z)-player.z;
  const double distance=dx*dx+dy*dy+dz*dz;
  const bool retained=std::binary_search(current.begin(),current.end(),id);
  if(distance>(retained?retain2:active2)||(eligible&&!eligible(part)))continue;
  Candidate candidate{distance,id};
  // A bounded max-heap keeps only the budget's nearest candidates. Its front
  // is the worst retained one, so logical accumulations do not allocate/sort a
  // second full-sized World snapshot every frame.
  if(nearest.size()<budget){nearest.push_back(candidate);std::push_heap(nearest.begin(),nearest.end(),better);}
  else if(better(candidate,nearest.front())){std::pop_heap(nearest.begin(),nearest.end(),better);nearest.back()=candidate;std::push_heap(nearest.begin(),nearest.end(),better);}
 }
 std::sort(nearest.begin(),nearest.end(),better);
 std::vector<InstanceId> desired;desired.reserve(nearest.size());for(const auto& part:nearest)desired.push_back(part.id);
 return desired;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "builtin_catalog.hpp"
#include <array>
#include <cstdio>
namespace salvage {
namespace {
// Canonical class identity comes from the owned-disc PAL format4 manifests.
// Enemy role is checked against the corresponding PAL update/death routines.
// These are sources, never a blanket whitelist of similarly named debris.
constexpr MechanicalSource sources[]{
 {8,"heavy-interceptor","Intercettatore Blarg","Blarg heavy interceptor"},
 {28,"plasmabot","Plasmabot","Plasmabot"},
 {29,"blarg-turret","Torretta Blarg","Blarg turret"},
 {30,"machinegun-turret","Torretta mitragliatrice","Machinegun turret"},
 {31,"minibomber","Minibombardiere","Minibomber"},
 {44,"sentrybot","Robot sentinella","Sentrybot"},
 {69,"fighter-jet","Caccia Blarg","Blarg fighter jet"},
 {123,"spiderbot","Robot ragno","Spiderbot"},
 {211,"plasmabot","Plasmabot","Plasmabot"},
 {217,"big-axebot","Robot taglialegna pesante","Big Axebot"},
 {233,"helicopter","Elicottero da combattimento","Combat helicopter"},
 {252,"scoutbot","Robot esploratore","Scoutbot"},
 {253,"bomber-tank","Carro bombardiere","Bomber tank"},
 {326,"blarg-helicopter","Elicottero Blarg","Blarg helicopter"},
 {336,"helicopter","Elicottero da combattimento","Combat helicopter"},
 {347,"fleet-turret","Torretta della flotta","Fleet turret"},
 {459,"birdbot","Birdbot","Birdbot"},
 {480,"grab-helicopter","Elicottero raccoglitore","Grab helicopter"},
 {491,"ultramech","Ultramech","Ultramech"},
 {493,"tank-ship","Trasporto corazzato","Tank ship"},
 {563,"woodcut-bot","Robot boscaiolo","Woodcut Bot"},
 {573,"robot-dog","Cane robot","Robot dog"},
 {577,"peckbot","Peckbot","Peckbot"},
 {578,"mine-layer","Robot posamine","Mine layer"},
 {612,"constructorbot","Robot da costruzione","Constructorbot"},
 {623,"extermibot","Robot disinfestatore","Extermibot"},
 {625,"extermitank","Carro disinfestatore","Extermitank"},
 {631,"blarg-helicopter","Elicottero Blarg","Blarg helicopter"},
 {688,"bomber-jet","Bombardiere Blarg","Blarg bomber jet"},
 {1126,"machinegun-turret","Torretta mitragliatrice","Machinegun turret"},
 {1379,"fighter-jet","Caccia Blarg","Blarg fighter jet"},
 {1382,"robot-dog","Cane robot","Robot dog"},
 {1422,"drek-robot","Robot di Drek","Drek's robot"},
 {1440,"veldin-robot","Robot di Veldin","Veldin robot"},
 {1454,"rocket-tank","Carro lanciarazzi","Rocket tank"},
};
struct Pair {unsigned source,fragment;};
struct Label {unsigned source,fragment;const char* name_it;const char* name_en;};
constexpr Label labels[]{
#include "builtin_part_labels.inc"
};
// Direct calls in PAL elevator_and_birdbot.c, train_locomotive.c and
// vendor_orb_and_robots.c. Source snippets are recorded by census_parts.py.
constexpr Pair known[]{
 {459,0x6c8},{459,0x6c9},{459,0x6ca},{459,0x6ea},
 {577,0x6d3},{577,0x6d4},{577,0x6d5},
 {573,0x6ce},{573,0x6cf},{573,0x77d},
 {1440,0x78d},{1440,0x78e},{1440,0x78f},{1440,0x7ab},
};
}
const MechanicalSource* mechanical_source(std::uint32_t klass)noexcept{
 for(const auto& source:sources)if(source.native_class==klass)return &source;
 return nullptr;
}
std::optional<PartDescriptor> builtin_part(std::uint32_t source,std::uint32_t fragment,float radius,float half_height,float mass){
 const auto* origin=mechanical_source(source);if(!origin||!fragment)return {};
 const Label* label=nullptr;
 for(const auto& candidate:labels)if(candidate.source==source&&candidate.fragment==fragment){label=&candidate;break;}
 if(!label)return {}; // Unknown particle/decorative models remain native.
 PartDescriptor p;
 p.stable_id="rac1.robot."+std::string(origin->stable_key)+"-"+std::to_string(source)+".part."+std::to_string(fragment);
 p.id=stable_part_id(p.stable_id);
 p.name_it=label->name_it;p.name_en=label->name_en;
 p.source_it=origin->name_it;p.source_en=origin->name_en;p.source_class=source;p.fragment_class=fragment;
 p.radius=radius;p.capsule_half_height=half_height;p.mass=mass;
 p.model_key="rac1.moby."+std::to_string(fragment);return p;
}
Status register_builtin_catalog(Catalog& catalog){
 for(const auto& pair:known){
  auto p=builtin_part(pair.source,pair.fragment);if(!p)return Status::Invalid;
  if(const auto* existing=catalog.find(p->id)){
   if(existing->stable_id!=p->stable_id)return Status::Duplicate;
   continue;
  }
  const auto status=catalog.register_part(std::move(*p));if(status!=Status::Ok)return status;
 }
 return Status::Ok;
}
}

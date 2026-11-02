// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.hpp"
#include "core_api.hpp"
#include "builtin_catalog.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>
using namespace salvage;
extern "C" int robot_salvage_c_header_test(void);
namespace {
unsigned checks{};
void require(bool ok,const char* what){++checks;if(!ok){std::fprintf(stderr,"salvage core: %s\n",what);std::exit(1);}}
bool near(float a,float b,float epsilon=.002f){return std::abs(a-b)<=epsilon;}
PartDescriptor part(const char* name="rac1.birdbot.head"){
 PartDescriptor p;p.stable_id=name;p.name_it="Testa Birdbot";p.name_en="Birdbot head";p.source_it="Birdbot";p.source_en="Birdbot";
 p.source_class=459;p.fragment_class=700;p.radius=.2f;p.mass=2;return p;
}
struct Fixture {
 Catalog catalog;Inventory inventory;World world;Events events;Collector collector;
 PartId head=stable_part_id("rac1.birdbot.head"),leg=stable_part_id("rac1.birdbot.leg");
 Fixture():collector(catalog,inventory,world,events){require(catalog.register_part(part())==Status::Ok,"register known part");
  require(catalog.register_part(part("rac1.birdbot.leg"))==Status::Ok,"register second known part");world.begin_visit(1);}
};
struct Terrain:CollisionPort {
 float wall=10000;unsigned calls{};
 std::optional<Contact> sweep_sphere(Vec3 from,Vec3 to,float radius,InstanceId)override{
  ++calls;require(finite(from)&&finite(to)&&std::isfinite(radius)&&radius>0,"physics queries are finite spheres");
  std::optional<Contact> hit;
  if(to.z<radius&&to.z<=from.z){const float denominator=from.z-to.z,u=denominator>0?std::clamp((from.z-radius)/denominator,0.f,1.f):0;
   const auto point=from+(to-from)*u;hit=Contact{{point.x,point.y,radius},{0,0,1},{},u};}
  if(to.x+radius>=wall&&to.x>from.x){const float u=std::clamp((wall-radius-from.x)/(to.x-from.x),0.f,1.f);
   if(!hit||u<hit->fraction){auto point=from+(to-from)*u;point.x=wall-radius;hit=Contact{point,{-1,0,0},{},u};}}
  return hit;
 }
};
void catalog_inventory(){
 Fixture f;const auto known=f.catalog.find(f.head);require(known&&known->id==f.head&&known->stable_id=="rac1.birdbot.head","stable deterministic identity");
 require(f.catalog.register_part(part())==Status::Duplicate,"duplicate catalog registration does not replace identity");
 auto invalid=part("bad name");require(f.catalog.register_part(invalid)==Status::Invalid,"stable ids reject ambiguous identifiers");
 invalid=part("rac1.invalid");invalid.radius=std::numeric_limits<float>::quiet_NaN();require(f.catalog.register_part(invalid)==Status::Invalid,"invalid collider cannot enter catalog");
 require(f.inventory.apply(f.catalog,{{f.head,3,Operation::Add},{f.leg,2,Operation::Add}})==Status::Ok,"batch grants two component types");
 const auto before=f.inventory.snapshot();const auto revision=f.inventory.revision();
 require(f.inventory.apply(f.catalog,{{f.head,2,Operation::Remove},{f.leg,3,Operation::Remove}})==Status::Insufficient,"recipe-like batch rejects insufficient second component");
 require(f.inventory.snapshot()==before&&f.inventory.revision()==revision,"failed batch cannot partially consume first component");
 require(f.inventory.apply(f.catalog,{{f.head,2,Operation::Remove},{f.leg,2,Operation::Remove}})==Status::Ok&&f.inventory.count(f.head)==1&&!f.inventory.count(f.leg),"successful batch consumes exact quantities");
 const auto maximum=std::numeric_limits<Quantity>::max();require(f.inventory.restore({{f.head,maximum}})==Status::Ok,"full uint64 quantities preserved");
 require(f.inventory.apply(f.catalog,{{f.head,1,Operation::Add}})==Status::Overflow&&f.inventory.count(f.head)==maximum,"quantity overflow preserves previous inventory");
 require(f.inventory.restore({{987654321,9007199254740993ull}})==Status::Ok&&f.inventory.count(987654321)==9007199254740993ull,"unknown IDs and counts above double precision survive restore");
 const auto unknown=f.inventory.snapshot();require(f.inventory.restore({{0,1}})==Status::Invalid&&f.inventory.snapshot()==unknown,"invalid restore is atomic");
 require(f.inventory.apply(f.catalog,{{123,1,Operation::Add}})==Status::UnknownPart,"unknown grants fail without inventing a catalog entry");
 require(f.inventory.restore({})==Status::Ok,"empty campaign inventory can be restored");
 unsigned random=0x12345678;Quantity expected=0;
 for(unsigned i=0;i<2000;++i){random=random*1664525u+1013904223u;const Quantity amount=1+(random%13);const bool add=(random&0x100)!=0;
  const auto status=f.inventory.apply(f.catalog,{{f.head,amount,add?Operation::Add:Operation::Remove}});
  if(add){expected+=amount;require(status==Status::Ok,"random valid addition succeeds");}
  else if(expected>=amount){expected-=amount;require(status==Status::Ok,"random affordable removal succeeds");}
  else require(status==Status::Insufficient,"random unaffordable removal fails");
  require(f.inventory.count(f.head)==expected,"transaction sequence retains exact expected total");
 }
}
void builtins(){
 Catalog catalog;
 require(register_builtin_catalog(catalog)==Status::Ok&&catalog.entries().size()==14,"directly proven mechanical source/fragment pairs register");
 require(register_builtin_catalog(catalog)==Status::Ok&&catalog.entries().size()==14,"repeated startup catalog registration is idempotent");
 require(mechanical_source(459)&&mechanical_source(577)&&mechanical_source(1440),"mechanical enemy sources recognized");
 for(unsigned excluded:{0u,11u,35u,186u,302u,750u,857u,914u,1208u,1428u,1906u})
  require(!mechanical_source(excluded),"player, vendors, organic enemies, friendly bots, scenery and debris classes are not source robots");
 const auto head=builtin_part(459,0x6c8),arm=builtin_part(459,0x6ca);
 require(head&&arm&&head->id!=arm->id&&head->name_it=="Testa di Birdbot"&&arm->name_it=="Mano di Birdbot","reviewed anatomical labels describe each original model without changing identity");
 require(!builtin_part(459,9999),"unknown decorative or particle classes are not made collectible");
 require(!mechanical_source(1446)&&!mechanical_source(1417),"scientist NPC and ambient flying-car aliases are not enemy sources");
 require(!builtin_part(750,0x6c8)&&!builtin_part(459,0),"fragment identity never makes an NPC source eligible");
}
void collection_visits(){
 Fixture f;const auto a=f.world.spawn(f.head,{{1,0,.1f}}),b=f.world.spawn(f.leg,{{1.2f,0,.1f}});
 CollectionInput input;input.player={0,0,0};auto visible=[](const Fragment&){return true;};
 require(f.collector.target(input,visible)==a,"nearest reachable component selected");
 input.native_interaction=true;input.triangle=true;require(f.collector.update(input,visible).status==Status::NotFound,"shop/NPC interaction takes Triangle precedence");
 input.native_interaction=false;require(f.collector.update(input,visible).status==Status::NotFound,"holding consumed Triangle does not collect after leaving native interaction");
 input.triangle=false;f.collector.update(input,visible);input.triangle=true;
 auto result=f.collector.update(input,visible);require(result.status==Status::Ok&&result.instance==a&&f.inventory.count(f.head)==1&&!f.world.find(a),"one press commits inventory then retires one world component");
 for(unsigned i=0;i<60;++i)require(f.collector.update(input,visible).status==Status::NotFound,"held Triangle never drains the pile");
 input.triangle=false;f.collector.update(input,visible);input.triangle=true;
 require(f.collector.update(input,[](const Fragment&){return false;}).status==Status::NotFound&&f.world.find(b),"wall blocks collection without deleting component");
 input.triangle=false;f.collector.update(input,visible);input.triangle=true;input.playing=false;
 require(f.collector.update(input,visible).status==Status::NotFound,"pause-menu Triangle does not collect");
 input.playing=true;require(f.collector.update(input,visible).status==Status::NotFound,"closing pause while holding Triangle does not collect");
 input.triangle=false;f.collector.update(input,visible);input.triangle=true;
 require(f.collector.update(input,visible).status==Status::Ok&&f.inventory.count(f.leg)==1,"second fresh press collects next part");
 const auto overflow=f.world.spawn(f.head,{{0,0,0}});f.inventory.restore({{f.head,std::numeric_limits<Quantity>::max()}});
 input.triangle=false;f.collector.update(input,visible);input.triangle=true;
 require(f.collector.update(input,visible).status==Status::Overflow&&f.world.find(overflow),"failed inventory transfer leaves physical fragment available");
 require(!f.world.claimed(overflow),"failed inventory transfer also releases custody for another destination");
 const auto visit=f.world.visit();require(!f.world.begin_visit(1)&&f.world.visit()==visit&&f.world.find(overflow),"same planet respawn retains visit/world records");
 require(f.world.begin_visit(2)&&f.world.all().empty()&&f.inventory.count(f.head)==std::numeric_limits<Quantity>::max(),"travel clears floor pieces without clearing collected inventory");
 const auto newer=f.world.spawn(f.leg,{{0,0,0}});require(newer>overflow,"instance IDs never alias old visit records");
 require(f.world.begin_visit(2,true)&&f.world.all().empty(),"explicit same-planet load starts a fresh visit");
 f.world.end_visit();require(!f.world.spawn(f.leg,{{0,0,0}}),"inactive world cannot accept physical spawns");
}
void collection_surface_reach(){
 const auto visible=[](const Fragment&){return true;};CollectionInput input;
 Fixture large;auto shell=part("qa.large-grounded-shell");shell.radius=4;
 require(large.catalog.register_part(shell)==Status::Ok,"large physical shell is a valid catalog component");
 const auto large_id=large.world.spawn(stable_part_id(shell.stable_id),{{0,0,4}});
 input.player={-4.45f,0,1.05f};
 require(length(large.world.find(large_id)->position-input.player)>shell.radius,
  "large-part fixture places the interaction origin outside the physical collider");
 require(large.collector.target(input,visible)==large_id,
  "grounded radius4 component is reachable from a character torso beside its physical surface");
 require(!large.collector.target(input,[](const Fragment&){return false;}),
  "large surface reach still requires unobstructed native line of sight");
 input.native_interaction=true;require(!large.collector.target(input,visible),"large components preserve native interaction priority");
 input.native_interaction=false;
 Fixture boundary;const auto edge=boundary.world.spawn(boundary.head,{{2.f,0,1.05f}});input.player={0,0,1.05f};
 require(boundary.collector.target(input,visible)==edge,"exactly1.8 from a sphere surface is within pickup reach");
 boundary.world.find(edge)->position.x=2.0005f;
 require(!boundary.collector.target(input,visible),"a sphere surface beyond1.8 is outside pickup reach");
 Fixture capsule;auto rail=part("qa.long-grounded-rail");rail.capsule_half_height=3;
 require(capsule.catalog.register_part(rail)==Status::Ok,"elongated capsule component registered");
 const auto rail_id=capsule.world.spawn(stable_part_id(rail.stable_id),{{0,0,.2f},{},{0,1.57079632679f,0}});
 input.player={5.f,0,.2f};
 require(capsule.collector.target(input,visible)==rail_id,
  "rotated capsule endpoint surface is reachable even when its center is five units away");
 input.player.x=5.0005f;require(!capsule.collector.target(input,visible),"capsule endpoint also enforces the1.8 surface boundary");
 input.player={0,1.9f,.2f};require(capsule.collector.target(input,visible)==rail_id,"capsule cylindrical side uses the closest axis point");
 input.player.y=2.0005f;require(!capsule.collector.target(input,visible),"capsule side rejects positions beyond its surface reach");
}
void event_cursors(){
 Events events(3);events.publish({0,EventKind::Collected,1,1,1,1});events.publish({0,EventKind::Collected,2,2,1,1});
 std::uint64_t a=0,b=0;std::vector<Event> out;
 require(events.read(a,1,out)==Status::Ok&&out.size()==1&&a==1,"consumer one can paginate");
 require(events.read(b,10,out)==Status::Ok&&out.size()==2&&b==2,"consumer two receives complete independent stream");
 events.publish({});events.publish({});events.publish({});
 require(events.read(a,10,out)==Status::Stale&&out.empty()&&a==1,"bounded event history reports a gap rather than silently losing events");
 a=events.newest_sequence();require(events.read(a,10,out)==Status::Ok&&out.empty(),"consumer can resnapshot and resume at newest sequence");
}
void exclusive_transfers(){
 Fixture f;const auto id=f.world.spawn(f.head,{{1,0,1},{2,0,0}});const auto original=*f.world.find(id);
 WorldLease first,second;require(f.world.claim(id,f.world.visit(),first)==Status::Ok&&f.world.owns(first),"a transfer obtains one generation-checked world lease");
 require(f.world.claim(id,f.world.visit(),second)==Status::Duplicate&&!second.generation,"another collector cannot claim the same physical part");
 require(!f.world.erase(id),"unconditional erase cannot steal an in-progress transfer");
 CollectionInput input;input.triangle=true;const auto visible=[](const Fragment&){return true;};
 require(!f.collector.target(input,visible)&&f.collector.update(input,visible).status==Status::NotFound&&f.inventory.count(f.head)==0,"Triangle cannot collect a part being pulled by another adapter");
 Terrain terrain;Physics physics;PlayerBody player;player.enabled=false;physics.step(f.world,f.catalog,terrain,1.f/60,player);
 require(near(f.world.find(id)->position.x,original.position.x)&&near(f.world.find(id)->position.z,original.position.z)&&terrain.calls==0,"world physics leaves the externally controlled part untouched");
 require(f.world.release(first)&&!f.world.owns(first)&&f.world.find(id),"cancelled destination leaves the original world record available");
 require(f.world.claim(id,f.world.visit(),second)==Status::Ok&&second.generation!=first.generation&&!f.world.release(first),"old cancellation cannot release a newer transfer");
 const auto taken=f.world.take(second);require(taken&&taken->id==id&&!f.world.find(id)&&!f.world.take(second),"acknowledged destination takes the original instance exactly once");
 const auto next=f.world.spawn(f.leg,{{0,0,0}});WorldLease stale;require(f.world.claim(next,f.world.visit(),stale)==Status::Ok,"visit cancellation fixture obtains custody");
 f.world.begin_visit(2,true);require(!f.world.owns(stale)&&!f.world.take(stale)&&!f.world.release(stale),"a load invalidates every old transfer without resurrecting pieces");
}
void physics(){
 Fixture f;Terrain floor;Physics physics;PlayerBody player;player.enabled=false;
 const auto id=f.world.spawn(f.head,{{0,0,3},{1,0,0},{},{0,0,1}});
 for(unsigned i=0;i<900;++i)physics.step(f.world,f.catalog,floor,1.f/60,player);
 auto* body=f.world.find(id);require(body&&body->sleeping&&near(body->position.z,.201f,.005f),"real sphere settles on terrain and sleeps instead of expiring");
 const auto asleep=body->position;const auto query_count=floor.calls;
 for(unsigned i=0;i<180;++i)physics.step(f.world,f.catalog,floor,1.f/60,player);
 require(near(body->position.x,asleep.x)&&near(body->position.z,asleep.z)&&floor.calls==query_count,"sleeping fragments remain present without redundant terrain simulation");
 body->position={100,0,.201f};body->sleeping=false;body->velocity={4,0,0};physics.step(f.world,f.catalog,floor,1.f/60,player);
 require(body->suspended&&near(body->position.x,100)&&f.world.all().size()==1,"distant fragment suspends without despawning or consuming its lifetime");
 player.position={100,0,0};physics.step(f.world,f.catalog,floor,1.f/60,player);require(!body->suspended&&body->position.x>100,"approaching resumes suspended motion");
 body->position={0,0,.201f};body->velocity={};body->sleeping=true;player.enabled=true;player.position={-.5f,0,0};player.velocity={3,0,0};
 physics.step(f.world,f.catalog,floor,1.f/60,player);require(!body->sleeping&&body->velocity.x>0&&body->position.x>0,"Ratchet capsule wakes and pushes a sleeping component");
 const auto frozen=*body;physics.step(f.world,f.catalog,floor,std::numeric_limits<float>::quiet_NaN(),player);
 require(near(body->position.x,frozen.position.x)&&near(body->position.z,frozen.position.z),"invalid delta time does not corrupt world motion");
 Fixture wall_case;Terrain wall;wall.wall=1;player.enabled=false;player.position={};
 const auto fast=wall_case.world.spawn(wall_case.head,{{0,0,2},{100,0,0}});
 physics.step(wall_case.world,wall_case.catalog,wall,.02f,player);
 require(wall_case.world.find(fast)->position.x<.801f&&wall_case.world.find(fast)->velocity.x<0,"swept terrain query prevents fast pieces tunneling through walls");
 Fixture pair;Terrain terrain;
 const auto lower=pair.world.spawn(pair.head,{{0,0,.201f}}),upper=pair.world.spawn(pair.leg,{{0,0,.45f}});
 for(unsigned i=0;i<300;++i)physics.step(pair.world,pair.catalog,terrain,1.f/60,player);
 require(pair.world.find(lower)->position.z>=.195f&&pair.world.find(upper)->position.z>.56f,"piece pile resolves upward without pushing bottom part through terrain");
 auto capsule_part=part("rac1.long-part");capsule_part.capsule_half_height=.4f;pair.catalog.register_part(capsule_part);
 const auto long_id=pair.world.spawn(stable_part_id(capsule_part.stable_id),{{2,0,2}});
 for(unsigned i=0;i<300;++i)physics.step(pair.world,pair.catalog,terrain,1.f/60,player);
 require(pair.world.find(long_id)->position.z>=.59f,"upright capsule lower sample rests at correct fitted height");
}
void public_api(){
 require(robot_salvage_c_header_test()!=0,"public header is usable as C");
 require(ApiService::query(1)==nullptr,"inactive API refuses callers");Fixture f;
 const robot_salvage_api_v1* api{};
 {
  ApiService service(f.catalog,f.inventory,f.world,f.events);api=ApiService::query(1);
  require(api&&api->version==1&&api->size==sizeof(*api)&&!ApiService::query(2),"versioned public query and table sizes");
  std::uint64_t count{},revision{};require(api->catalog_count(&count)==0&&count==2,"public catalog enumeration");
  robot_salvage_part_v1 descriptor{};descriptor.size=sizeof(descriptor);
  require(api->get_part(f.head,&descriptor)==0&&descriptor.id==f.head&&std::strcmp(descriptor.name_it,"Testa Birdbot")==0,"public descriptor is caller-owned copied data");
  descriptor.size=4;require(api->get_part(f.head,&descriptor)==ROBOT_SALVAGE_INVALID,"undersized output rejected");
  robot_salvage_change_v1 changes[2]{{sizeof(robot_salvage_change_v1),ROBOT_SALVAGE_ADD,f.head,5},{sizeof(robot_salvage_change_v1),ROBOT_SALVAGE_REMOVE,f.leg,1}};
  require(api->apply_changes(changes,2)==ROBOT_SALVAGE_INSUFFICIENT&&f.inventory.snapshot().empty(),"API mutation preserves transaction atomicity");
  require(api->apply_changes(changes,1)==0&&api->inventory_count(&count,&revision)==0&&count==1,"API successful grant updates inventory");
  robot_salvage_spawn_v1 spawn{};spawn.size=sizeof(spawn);spawn.part=f.head;spawn.position={1,2,3};std::uint64_t instance{};
  require(api->spawn_part(&spawn,&instance)==0&&f.world.find(instance),"custom-level public spawn creates logical physical instance");
  robot_salvage_fragment_v1 record{};record.size=sizeof(record);
  require(api->world_entry(0,&record)==0&&record.instance==instance&&record.position.z==3,"world API exports only copies and stable IDs");
  std::uint64_t cursor=0,newest=0;std::uint32_t received=0;robot_salvage_event_v1 event[4]{};
  for(auto& e:event)e.size=sizeof(e);
  require(api->read_events(&cursor,event,4,&received,&newest)==0&&received==2&&event[0].kind==ROBOT_SALVAGE_INVENTORY_CHANGED&&event[1].kind==ROBOT_SALVAGE_SPAWNED,"API consumers see grant and spawn events");
  int result=0;std::thread other([&]{std::uint64_t untouched=123;result=api->catalog_count(&untouched);if(untouched!=123)result=100;});other.join();
  require(result==ROBOT_SALVAGE_WRONG_THREAD,"foreign thread mutation/read does not enter game-owned state");
 }
 std::uint64_t ignored{};require(!ApiService::query(1)&&api->catalog_count(&ignored)==ROBOT_SALVAGE_NOT_READY,"retained table safely refuses calls after session unload");
}
}
int main(){catalog_inventory();builtins();collection_visits();collection_surface_reach();event_cursors();exclusive_transfers();physics();public_api();std::printf("robot salvage core: %u checks passed\n",checks);}

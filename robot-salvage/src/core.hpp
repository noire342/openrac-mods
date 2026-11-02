// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace salvage {
using PartId=std::uint64_t;
using InstanceId=std::uint64_t;
using Quantity=std::uint64_t;
struct Vec3 {float x{},y{},z{};};
Vec3 operator+(Vec3,Vec3);
Vec3 operator-(Vec3,Vec3);
Vec3 operator*(Vec3,float);
float dot(Vec3,Vec3);
float length(Vec3);
bool finite(Vec3);

enum class Status {Ok=0,Invalid=-1,UnknownPart=-2,Duplicate=-3,Overflow=-4,Insufficient=-5,NotFound=-6,Stale=-7,WrongThread=-8,NotReady=-9};
// IDs are independent of native classes, region and current language. A caller
// can supply its own nonzero ID; id0 asks for the deterministic stable-id hash.
PartId stable_part_id(const std::string&) noexcept;
struct PartDescriptor {
 PartId id{};
 std::string stable_id,name_it,name_en,source_it,source_en;
 std::uint32_t source_class{},fragment_class{};
 float radius=.2f,capsule_half_height=0.f,mass=1.f;
 std::string model_key;
};
class Catalog final {
public:
 Status register_part(PartDescriptor);
 const PartDescriptor* find(PartId)const noexcept;
 const std::map<PartId,PartDescriptor>& entries()const noexcept{return entries_;}
 const std::map<PartId,PartDescriptor>& all()const noexcept{return entries_;}
private:
 std::map<PartId,PartDescriptor> entries_;
};

enum class Operation {Add,Remove};
struct Change {PartId part{};Quantity quantity{};Operation operation=Operation::Add;};
using InventorySnapshot=std::map<PartId,Quantity>;
// No game/SDK dependency. Unknown IDs are preserved by restore(), so loading a
// campaign with an unavailable optional catalog never silently deletes items.
class Inventory final {
public:
 Status apply(const Catalog&,const std::vector<Change>&);
 Quantity count(PartId)const noexcept;
 const InventorySnapshot& snapshot()const noexcept{return items_;}
 Status restore(const InventorySnapshot&);
 std::uint64_t revision()const noexcept{return revision_;}
private:
 InventorySnapshot items_;
 std::uint64_t revision_{};
};

struct SpawnState {Vec3 position{},velocity{},rotation{},angular_velocity{};};
struct Fragment {
 InstanceId id{};PartId part{};std::uint64_t visit{};
 Vec3 position{},velocity{},rotation{},angular_velocity{};
 float quiet_seconds{};
 bool sleeping=false,suspended=false;
};
// Exclusive custody for transfers to any game-system adapter. Leases contain
// no native addresses; visit and monotonic generation reject stale owners.
struct WorldLease {InstanceId instance{};std::uint64_t visit{},generation{};};
// A same-planet respawn retains records. Explicit load starts a new visit even
// on the same planet. Native pointers/generations belong to the native adapter.
class World final {
public:
 bool begin_visit(int planet,bool explicit_load=false);
 void end_visit();
 int planet()const noexcept{return planet_;}
 std::uint64_t visit()const noexcept{return visit_;}
 InstanceId spawn(PartId,const SpawnState&);
 Fragment* find(InstanceId)noexcept;
 const Fragment* find(InstanceId)const noexcept;
 bool erase(InstanceId)noexcept;
 Status claim(InstanceId,std::uint64_t visit,WorldLease&);
 bool owns(const WorldLease&)const noexcept;
 bool claimed(InstanceId)const noexcept;
 bool release(const WorldLease&)noexcept;
 std::optional<Fragment> take(const WorldLease&)noexcept;
 const std::map<InstanceId,Fragment>& all()const noexcept{return fragments_;}
private:
 std::map<InstanceId,Fragment> fragments_;
 int planet_=-1;
 std::uint64_t visit_{};
 InstanceId next_id_=1;
 std::uint64_t next_lease_=1;
 std::map<InstanceId,std::uint64_t> claims_;
};

enum class EventKind:std::uint32_t {Collected=1,InventoryChanged=2,Spawned=3,VisitChanged=4,InventoryRestored=5,CatalogChanged=6};
struct Event {
 std::uint64_t sequence{};
 EventKind kind=EventKind::Collected;
 PartId part{};InstanceId instance{};Quantity quantity{},total{};
 std::uint64_t revision{},visit{};
};
// Every consumer owns its cursor. Overflow is explicit: resnapshot and continue
// at newest_sequence(), rather than consume another mod's events.
class Events final {
public:
 explicit Events(std::size_t capacity=4096):entries_(capacity?capacity:1),capacity_(capacity?capacity:1){}
 bool publish(Event)noexcept;
 Status read(std::uint64_t& cursor,std::size_t maximum,std::vector<Event>& out)const;
 std::uint64_t newest_sequence()const noexcept{return sequence_;}
private:
 std::vector<Event> entries_;
 std::size_t capacity_;
 std::size_t head_{},count_{};
 std::uint64_t sequence_{};
};
struct CollectionInput {
 bool triangle=false,playing=true,alive=true,native_interaction=false;
 Vec3 player{}; // adapter-supplied interaction origin; reach is to collider surface
};
struct CollectionResult {Status status=Status::NotFound;InstanceId instance{};PartId part{};Quantity total{};};
using Visibility=std::function<bool(const Fragment&)>;
class Collector final {
public:
 static constexpr float reach=1.8f;
 Collector(const Catalog& catalog,Inventory& inventory,World& world,Events& events)
  :catalog_(catalog),inventory_(inventory),world_(world),events_(events){}
 std::optional<InstanceId> target(const CollectionInput&,const Visibility&)const;
 CollectionResult update(const CollectionInput&,const Visibility&);
 void reset_input(bool triangle=false)noexcept{triangle_=triangle;}
private:
 const Catalog& catalog_;Inventory& inventory_;World& world_;Events& events_;
 bool triangle_{};
};

struct Contact {Vec3 point{},normal{0,0,1},surface_velocity{};float fraction=1.f;};
class CollisionPort {
public:
 virtual ~CollisionPort()=default;
 // Native adapters can suspend virtualized bodies without deleting World
 // records. Portable/core-only callers simulate every eligible body by default.
 virtual bool simulation_enabled(InstanceId)const{return true;}
 // Contact point is the corrected CENTER of the swept sphere, in world units.
 virtual std::optional<Contact> sweep_sphere(Vec3 from,Vec3 to,float radius,InstanceId ignore)=0;
};
struct PlayerBody {
 Vec3 position{},velocity{};
 float radius=.4f,half_height=.65f;
 bool enabled=true;
};
struct PhysicsSettings {
 Vec3 gravity{0,0,-9.8f};
 float restitution=.25f,friction=.55f,sleep_speed=.08f,sleep_seconds=.75f;
 float activation_distance=45.f,fixed_step=1.f/120.f;
 unsigned maximum_steps=12;
};
struct PhysicsResult {Vec3 player_correction{};std::size_t active{},sleeping{},suspended{};};
class Physics final {
public:
 explicit Physics(PhysicsSettings settings={}):settings_(settings){}
 PhysicsResult step(World&,const Catalog&,CollisionPort&,float seconds,const PlayerBody&);
 const PhysicsSettings& settings()const noexcept{return settings_;}
private:
 PhysicsSettings settings_;
};
}

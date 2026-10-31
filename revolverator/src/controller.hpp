// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace revolverator {
struct Vec3 { float x{},y{},z{}; };
Vec3 operator+(Vec3 a,Vec3 b);
Vec3 operator-(Vec3 a,Vec3 b);
Vec3 operator*(Vec3 a,float b);
float length_squared(Vec3 a);
bool finite(Vec3 a);
struct Target {
 std::uint32_t address{},pvars{},klass{},stamp{};
 bool operator==(const Target& b)const{return address==b.address&&pvars==b.pvars&&klass==b.klass&&stamp==b.stamp;}
 explicit operator bool()const{return address!=0;}
};
inline constexpr std::size_t maximum_flights=8;
enum class Phase { Idle,Windup,Spinning,Flying,Recovery };
enum class Motion { Strike,Raise,Release,Recover,DryFire };
struct Settings {
 float reach=2.8f, windup=.04f, minimum_hold=.18f, capture_window=.45f, minimum_spin=.28f, full_charge=1.6f, maximum_spin=3.f;
 float orbit_radius=1.05f, orbit_height=2.05f, flight_seconds=1.4f, recovery=.3f;
};
struct Frame {
 float seconds=1.f/60.f; std::uint32_t epoch{};
 bool equipped{},playing{},paused{},alive{},primary{},grounded{};
 Vec3 position{}; float yaw{};
};
struct Contact { Vec3 point{}; Target other{}; Vec3 normal{}; };
enum class ImpactReason { Collision,FlightTimeout };
struct ImpactContext {
 ImpactReason reason=ImpactReason::Collision;
 Vec3 point{},normal{};
 std::optional<Target> other{};
 float charge{};
};
// The controller never reads game memory or calls the SDK. The adapter owns all
// native handles, collision queries, damage routing, pose changes and restoration.
class World {
public:
 virtual ~World()=default;
 virtual bool begin_attack()=0;
 virtual std::optional<Target> find_target(Vec3 origin,float yaw,float reach)=0;
 virtual bool valid(Target target)const=0;
 virtual bool capture(Target target)=0;
 virtual std::optional<Vec3> carry_anchor(float angle)=0;
 virtual void move(Target target,Vec3 position,float yaw)=0;
 virtual void release(Target target,bool cancelled)=0;
 virtual std::optional<Contact> sweep(Target target,Vec3 from,Vec3 to)=0;
 virtual void impact(Target target,std::optional<Target> other,float charge)=0;
 // Default seams retain compatibility with adapters which only implement the
 // original controller. NativeWorld latches provenance and owns release recoil.
 virtual void observe_frame(const Frame&){}
 virtual bool can_launch()const{return true;}
 virtual void launched(Target,float /* throw_charge */,float /* blast_charge */){}
 virtual void impact_result(Target target,const ImpactContext& result){impact(target,result.other,result.charge);}
 virtual void cancelled(){}
 // Abandon one stale projectile, or the current gesture, without destroying
 // unrelated flights. Full scene/death/reset teardown still uses cancelled().
 virtual void discarded(Target){}
 virtual void attack_cancelled(){}
 virtual void animate(Motion motion)=0;
};
struct Counters { std::uint64_t captures{},throws{},impacts{}; };
class Controller {
public:
 explicit Controller(World& world,Settings settings={}):world_(world),settings_(settings){}
 void tick(const Frame& frame);
 void cancel();
 void discard(Target target);
 Phase phase()const;
 Phase attack_phase()const{return phase_;}
 Target target()const;
 bool owns(Target target)const{return (target_&&target==target_)||flying(target);}
 bool flying(Target target)const;
 std::size_t flight_count()const;
 float charge()const;
 float blast_charge()const;
 float spin_angle()const{return angle_;}
 const Counters& counters()const{return counters_;}
private:
 World& world_; Settings settings_; Phase phase_=Phase::Idle;
 Target target_{}; Vec3 point_{};
 float timer_{},spin_time_{},angle_{};
 struct Flight {
  Target target{};Vec3 point{},velocity{};float elapsed{},charge{},blast_charge{},yaw{};
  std::uint64_t sequence{};
 };
 std::array<Flight,maximum_flights> flights_{};
 std::uint64_t sequence_{},reset_generation_{};
 std::uint32_t epoch_{}; bool have_epoch_{},held_{},buffered_{},release_recovery_{};
 Counters counters_{};
 void recover();
 void cancel_attack();
 void advance_flights(const Frame& frame);
 float recovery_seconds()const;
 Flight* free_flight();
 const Flight* displayed_flight()const;
};
bool capturable_class(int klass);
}

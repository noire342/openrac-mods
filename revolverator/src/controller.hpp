// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
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
struct Contact { Vec3 point{}; Target other{}; };
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
 virtual void animate(Motion motion)=0;
};
struct Counters { std::uint64_t captures{},throws{},impacts{}; };
class Controller {
public:
 explicit Controller(World& world,Settings settings={}):world_(world),settings_(settings){}
 void tick(const Frame& frame);
 void cancel();
 Phase phase()const{return phase_;}
 Target target()const{return target_;}
 bool owns(Target target)const{return target_&&target==target_;}
 float charge()const;
 float spin_angle()const{return angle_;}
 const Counters& counters()const{return counters_;}
private:
 World& world_; Settings settings_; Phase phase_=Phase::Idle;
 Target target_{}; Vec3 point_{},velocity_{};
 float timer_{},spin_time_{},angle_{},flight_charge_{};
 std::uint32_t epoch_{}; bool have_epoch_{},held_{};
 Counters counters_{};
 void recover();
};
bool capturable_class(int klass);
}

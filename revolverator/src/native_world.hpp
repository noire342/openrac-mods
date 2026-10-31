// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include "memory.hpp"
#include "native_arm.hpp"
#include "attachment.hpp"
#include "native_ammunition.hpp"
#include "native_rotor.hpp"
#include "golden_impact.hpp"
#include "release_recoil.hpp"
#include <array>
namespace revolverator {
class NativeWorld final:public World,public Memory {
public:
 NativeWorld(const openrac_api* api,openrac_mod* mod,NativeAmmunition& ammunition,NativeRotor& rotor,GuestArena& arena);
 ~NativeWorld()override;
 bool begin_attack()override{return ammunition_.begin_attack();}
 std::optional<Target> find_target(Vec3 origin,float yaw,float reach)override;
 bool valid(Target target)const override;
 bool capture(Target target)override;
 std::optional<Vec3> carry_anchor(float angle)override;
 void move(Target target,Vec3 position,float yaw)override;
 void release(Target target,bool cancelled)override;
 std::optional<Contact> sweep(Target target,Vec3 from,Vec3 to)override;
 void impact(Target target,std::optional<Target> other,float charge)override;
 void observe_frame(const Frame& frame)override;
 bool can_launch()const override;
 void launched(Target target,float throw_charge,float blast_charge)override;
 void impact_result(Target target,const ImpactContext& result)override;
 void cancelled()override;
 void attack_cancelled()override;
 void discarded(Target target)override;
 void animate(Motion motion)override;
 Frame snapshot(bool primary,std::uint32_t epoch)const;
 void set_active(bool active){active_=active;if(!active)recoil_.cancel();}
 void set_golden(bool golden){golden_=golden;}
 void set_damage_multiplier(float value);
 void set_drill_head_scale(float radial,float axial){arm_.set_head_scale(radial,axial);}
 bool release_recoil_active()const;
 // Call after native Hero_UpdateMotionInput and Hero_SetPlanarVelocity. Only
 // Ratchet's horizontal motion is clamped; gravity, world and camera keep ticking.
 void enforce_release_recoil();
 std::uint64_t golden_explosions()const{return golden_explosions_;}
 std::uint64_t golden_hits()const{return golden_hits_;}
 bool frozen(std::uint32_t moby)const;
 bool blocks_native_hit(std::uint32_t hit)const;
 bool lease_valid()const;
 bool flying(Target target)const;
 std::size_t flight_count()const;
 void pose(Phase phase,float angle);
 void align_weapon(float angle);
 void align_release(){if(arm_.releasing())arm_.align_weapon(weapon());}
 void reset();
 unsigned hero()const{return global(0x13f350);}
 unsigned body()const{return read<unsigned>(hero()+0x2080);}
 unsigned weapon()const{return read<unsigned>(hero()+0x1090);}
 unsigned mode()const{return level(0x15f5c4);}
 Target token(unsigned moby)const;
 void log(const char* text)const;
private:
 NativeArm arm_;
 NativeAmmunition& ammunition_;
 NativeRotor& rotor_;
 using Allocate=unsigned(*)(unsigned);
 using Hit=void(*)(unsigned,unsigned);
 using Ray=int(*)(unsigned,unsigned,int,unsigned,int);
 using Basis=void(*)(unsigned,unsigned);
 using State=int(*)(int,int);
 using Animation=void(*)(int,int,float);
 using Burst=void(*)(unsigned,unsigned,int,float,float);
 Allocate allocate_; Hit post_hit_;Ray ray_;Basis basis_;State state_;Animation animation_;Burst burst_;
 openrac_mod* mod_; bool trace_;
 bool active_=true;
 bool golden_=false;
 float damage_multiplier_=1.f;
 ReleaseRecoil recoil_;
 Frame last_frame_{};
 struct Shot {Target weapon{},body{};float multiplier=1.f,yaw{};};
 Target recoil_body_{};
 std::uint64_t golden_explosions_{},golden_hits_{};
 int level_=-1;
 struct Lease {
  Target target{};unsigned collision{},disabled{};Vec3 origin{},angles{},local_sphere{};
  CaptureGrip grip{};float hold_time{},spin_angle{};bool flying{};
 } lease_;
 struct Flight {Target target{};Lease lease{};Shot shot{};GoldenImpact impact{};};
 std::array<Flight,maximum_flights> flights_{};
 struct DeferredHit {Target target{};Shot shot{};float damage{};Vec3 direction{};bool explosive{};};
 std::array<DeferredHit,maximum_flights*GoldenBlastPlan::maximum_hits> deferred_{};
 std::size_t deferred_head_{},deferred_size_{},frame_hits_{};
 std::uint64_t cancellation_generation_{};
 struct Site {Target target{};Vec3 point{},approach{};} site_;
 void move_origin(const Lease& lease,Vec3 position,Vec3 angles);
 void restore_lease(Lease& lease,bool cancelled);
 Flight* flight(Target target);
 const Flight* flight(Target target)const;
 Lease* owned_lease(Target target);
 bool identity_valid(Target target)const;
 void each_enemy(const void* context,void(*fn)(const void*,Target),bool include_crates=false)const;
 void each_damageable(const void* context,void(*fn)(const void*,Target))const;
 Vec3 damage_centre(Target target)const;
 float collision_radius(Target target)const;
 bool visible_from(Vec3 origin,Target target,unsigned ignore)const;
 std::optional<Vec3> ray(Vec3 from,Vec3 to,unsigned ignore)const;
 bool hit(Target target,float damage,Vec3 direction,unsigned attacker,bool explosive=false);
 bool submit_hit(Target target,float damage,Vec3 direction,const Shot& shot,bool explosive=false);
 void drain_hits();
 unsigned shot_attacker(const Shot& shot)const;
 void base_impact(Target target,std::optional<Target> other,float charge,const Shot& shot);
 void explode(Target target,const ImpactContext& result,const GoldenBlast& blast,const Shot& shot);
};
}

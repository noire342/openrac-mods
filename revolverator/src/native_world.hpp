// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller.hpp"
#include "memory.hpp"
#include "native_arm.hpp"
#include "attachment.hpp"
#include "native_ammunition.hpp"
#include "native_rotor.hpp"
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
 void animate(Motion motion)override;
 Frame snapshot(bool primary,std::uint32_t epoch)const;
 bool frozen(std::uint32_t moby)const;
 bool blocks_native_hit(std::uint32_t hit)const;
 bool lease_valid()const;
 void pose(Phase phase,float angle);
 void align_weapon(float angle);
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
 Allocate allocate_; Hit post_hit_;Ray ray_;Basis basis_;State state_;Animation animation_;
 openrac_mod* mod_; bool trace_;
 int level_=-1;
 struct Lease {
  Target target{};unsigned collision{},disabled{};Vec3 origin{},angles{},local_sphere{};
  CaptureGrip grip{};float hold_time{},spin_angle{};bool flying{};
 } lease_;
 struct Site {Target target{};Vec3 point{},approach{};} site_;
 void move_origin(Target target,Vec3 position,Vec3 angles);
 bool identity_valid(Target target)const;
 void each_enemy(const void* context,void(*fn)(const void*,Target),bool include_crates=false)const;
 std::optional<Vec3> ray(Vec3 from,Vec3 to,unsigned ignore)const;
 void hit(Target target,float damage,Vec3 direction);
};
}

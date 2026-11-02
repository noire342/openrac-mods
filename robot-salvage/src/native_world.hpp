// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include "native_suck_cannon.hpp"
#include "native_fragment_sources.hpp"
#include "materialization_policy.hpp"
#include <map>
#include <vector>

namespace salvage {
class NativeWorld final:public NativeMemory,public CollisionPort,public CannonWorldPort {
public:
 NativeWorld(const openrac_api*,openrac_mod*,GuestArena&,Catalog&,World&,Events* events=nullptr);
 ~NativeWorld(){detach(true);}
 void before_load(int next_level);
 void after_load(int level);
 void explicit_load();
 bool adopt(unsigned source,unsigned fragment);
 bool adopt(FragmentOrigin,unsigned fragment,const FragmentMotion&);
 void set_materialized_callback(std::function<void(unsigned)> callback){materialized_=std::move(callback);}
 bool owns(unsigned moby)const;
 bool owns_callback(unsigned moby)const;
 void frame();
 bool visible(const Fragment&)const;
 CollectionInput input(bool triangle)const;
 void retire_collected();
 std::optional<Contact> sweep_sphere(Vec3,Vec3,float,InstanceId)override;
 bool simulation_enabled(InstanceId id)const override{return native_actor(id)!=0;}
 unsigned native_actor(InstanceId)const;
 std::uint64_t adoptions()const{return adoptions_;}
 std::uint64_t pushes()const{return pushes_;}
 std::size_t materialized()const{return actors_.size();}
 std::optional<CannonNativeBody> cannon_describe(unsigned)const override;
 bool cannon_replace_class(unsigned,unsigned,unsigned)override;
 std::optional<CannonNativeBody> cannon_claim(unsigned)override;
 bool cannon_cancel(const CannonNativeBody&,bool)override;
 bool cannon_capture(const CannonNativeBody&)override;
 void cannon_retire(const CannonNativeBody&)override;
private:
 struct Actor {unsigned address{},pclass{},pvars{},collision{};float scale{};Vec3 local_offset{};};
 struct Visual {float scale{};Vec3 local_offset{};};
 struct Detached {WorldLease lease{};Actor actor{};};
 openrac_mod* mod_;GuestArena& arena_;Catalog& catalog_;World& world_;Physics physics_;
 MaterializationPolicy materialization_;
 Events* events_{};
 std::function<void(unsigned)> materialized_;
 std::map<InstanceId,Actor> actors_;
 std::map<unsigned,Actor> retired_callbacks_;
 std::map<std::uint64_t,Detached> detached_;
 std::map<InstanceId,Visual> visuals_;
 std::vector<unsigned> free_colliders_;
 int level_=-1;unsigned relay_{};
 bool collision_suppressed_=false;
 std::uint64_t adoptions_{},pushes_{};
 using Ray=int(*)(unsigned,unsigned,int,unsigned,int);
 using Sphere=int(*)(float,unsigned,int,unsigned);
 using Capsule=int(*)(float,float,unsigned,int,unsigned);
 Ray ray_{};Sphere sphere_{};Capsule capsule_{};
 void(*matrix_)(unsigned){};void(*basis_)(unsigned,unsigned){};void(*remove_)(unsigned){};unsigned(*create_)(int){};
 bool actor_valid(const Actor&)const;
 bool callback_valid(const Actor&)const;
 bool resources(unsigned)const;
 void detach(bool remove);
 void sync(Actor&,const Fragment&,const PartDescriptor&);
 Actor attach(unsigned,const Fragment&,const PartDescriptor&,Vec3 offset={});
 unsigned collider();
 void release_actor(Actor&,bool remove);
 PlayerBody player()const;
 void correct_player(Vec3);
 void log(const char*)const;
};
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include <map>
#include <optional>
#include <vector>
namespace salvage {
// Value transfer from NativeWorld. Native addresses remain inside native
// adapters; the public salvage C API never exposes this structure.
struct CannonNativeBody {
 WorldLease lease{};
 Fragment fragment{};
 unsigned actor{},pclass{},pvars{},collision{},update{};
 float scale{};
 Vec3 offset{};
};
class CannonWorldPort {
public:
 virtual ~CannonWorldPort()=default;
 // Read a managed materialized world actor without claiming it. lease is empty.
 virtual std::optional<CannonNativeBody> cannon_describe(unsigned actor)const=0;
 // Compare generation/pvars/current class and update NativeWorld's expected
 // class together with the actor's class. No native actor is recreated.
 virtual bool cannon_replace_class(unsigned actor,unsigned expected,unsigned replacement)=0;
 // Claim World::claim(id,visit), then detach ONLY the native actor map record.
 // Keep collision storage leased; no DeleteMoby, collider free or world erase.
 // Failure leaves the world and actor map unchanged.
 virtual std::optional<CannonNativeBody> cannon_claim(unsigned actor)=0;
 // Restore the detached native actor map and release its world lease. When
 // native_valid is true adopt the actor's current native transform; otherwise
 // retain the copied Fragment transform and rematerialize safely if needed.
 virtual bool cannon_cancel(const CannonNativeBody&,bool native_valid)=0;
 // Destination already acknowledged hero slot+stored count. World::take(lease)
 // atomically removes exactly that original record. The adapter keeps actor
 // and collision storage until projectile cleanup; NEVER DeleteMoby here.
 virtual bool cannon_capture(const CannonNativeBody&)=0;
 // Return the detached collider budget after native finalization/removal.
 // Do not touch a recycled native actor and do not add inventory/world items.
 virtual void cannon_retire(const CannonNativeBody&)=0;
};
// PAL-only native adapter. Register the source-audited typed hooks listed in
// docs/NATIVE_SUCK_CANNON.md. Route owned actors through update() before the
// ordinary World relay. offer() is called after adopting/rematerializing a part.
class NativeSuckCannon final:private NativeMemory {
public:
 using Launch=int(*)(unsigned,unsigned,unsigned,float);
 using Finalize=void(*)(unsigned,unsigned,int);
 NativeSuckCannon(const openrac_api*,GuestArena&,CannonWorldPort&);
 ~NativeSuckCannon();
 bool offer(unsigned actor);
 bool owns(unsigned actor)const;
 // Broader than live ownership: known private class data must never recurse
 // through the native getter when a generation is stale or already retired.
 bool handles_record(unsigned actor)const;
 unsigned record(unsigned actor)const;
 // Exact class callback order: actor, muzzle-position address, cannon actor.
 int grab(unsigned actor,unsigned muzzle,unsigned cannon);
 int candidate(unsigned cannon,unsigned data,unsigned actor,unsigned record,unsigned muzzle);
 int store(unsigned actor,unsigned muzzle);
 int cancel(unsigned actor);
 int launch(unsigned actor,unsigned velocity,unsigned target,float height,Launch original);
 void finalize(unsigned actor,unsigned record,int explode,Finalize original);
 bool update(unsigned actor);
 void frame();
 void reset();
 std::size_t tracked()const noexcept;
private:
 struct Actor {
  CannonNativeBody body{};unsigned original_class{},storage{},bytes{},record{},clone{},table{},anims{};
  unsigned short original_flags{},native_class{};bool claimed=false,captured=false,abort_pending=false,retired=false;
 };
 struct Storage{unsigned address{},bytes{};};
 GuestArena& arena_;CannonWorldPort& world_;std::map<unsigned,Actor> actors_;std::vector<Storage> free_;
 int level_=-1;unsigned dispatch_depth_{};
 int(*carried_)(unsigned,unsigned){};void(*matrix_)(unsigned){};void(*remove_)(unsigned){};
 int(*ticks_)(int){};
 bool same_actor(const Actor&)const;
 bool storage_referenced(const Actor&)const;
 bool valid_actor(const Actor&)const;
 unsigned capacity()const;
 int slot(unsigned actor)const;
 void clear_slot(Actor&,bool stored);
 void restore(Actor&);
 bool rollback(Actor&,bool stored);
 unsigned allocate(unsigned bytes,unsigned& allocated);
};
}

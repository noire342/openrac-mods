// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "core.hpp"
#include <cstddef>
#include <functional>
#include <vector>

namespace salvage {
struct MaterializationSettings {
 float active_distance=45.f;
 float retain_distance=55.f;
 std::size_t budget=96;
};

// Pure selection, independent from native actor handles, model classes and SDK.
// The caller applies the completed result to its adapters. World records and
// their physics/sleep/visit state are never changed by this policy.
class MaterializationPolicy final {
public:
 using Eligible=std::function<bool(const Fragment&)>;
 explicit MaterializationPolicy(MaterializationSettings={});
 // Current IDs receive only distance hysteresis, not priority over nearer new
 // pieces. Claimed parts are in another destination's custody and never chosen.
 // Optional eligibility can exclude unavailable models before spending budget.
 // The result is ordered nearest first, then by increasing stable instance ID.
 // Budget0 or an invalid player position yields no desired materializations.
 std::vector<InstanceId> select(const World&,Vec3 player,
  const std::vector<InstanceId>& currently_materialized,
  const Eligible& eligible={})const;
 const MaterializationSettings& settings()const noexcept{return settings_;}
private:
 MaterializationSettings settings_;
};
}

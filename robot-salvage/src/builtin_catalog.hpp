// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "core.hpp"
namespace salvage {
struct MechanicalSource {
 std::uint32_t native_class;
 const char* stable_key;
 const char* name_it;
 const char* name_en;
};
// Deliberately excludes friendly Gadgebots, Infobots, weapon drones, NPCs,
// vegetation, organic enemies and decorative scenery with similar fragments.
const MechanicalSource* mechanical_source(std::uint32_t native_class)noexcept;
std::optional<PartDescriptor> builtin_part(std::uint32_t source,std::uint32_t fragment,
 float radius=.2f,float capsule_half_height=0.f,float mass=1.f);
// Only directly proven source/part pairs are eagerly registered. Other actual
// factory results from whitelisted robots use builtin_part at first creation.
Status register_builtin_catalog(Catalog&);
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "asteroid_field.hpp"
#include "flight.hpp"
#include "ship_identity.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace spacevoyages {
// Authored native-model local coordinates: +X forward, +Y left, +Z up.
// The asset reader supplies actual mounts; the core never invents a gun.
struct WeaponMuzzle {Vec3 position{},direction{1,0,0};};
struct ShipWeaponDefinition {
    std::array<WeaponMuzzle,8> muzzles{};
    std::size_t count{};
    bool armed{};
};
struct SpaceCombatAssets {
    std::array<ShipWeaponDefinition,ship_profile_count> ships{}; // stock0..2, Original3
    float projectile_speed=400.f,projectile_range=200.f;
    std::uint32_t cadence_ticks=4; // one native60Hz tick, four ticks=15Hz
};
struct SpaceProjectile {
    Vec3 previous_position{},position{},velocity{};
    float remaining_range{};
    std::uint64_t id{};
    bool active{};
    Vec3 birth_position{};
    double birth_seconds{};
    float lifetime{};
    double retired_seconds=-1.; // pool reuse waits until the next native tick
};
enum class SpaceCombatEventKind : std::uint8_t {Impact,Destroyed,PlanetImpact};
struct SpaceCombatEvent {
    SpaceCombatEventKind kind{};
    Vec3 position{},normal{1,0,0};
    std::size_t asteroid_id=static_cast<std::size_t>(-1);
    std::uint64_t projectile_id{};
    std::uint64_t id{};
    double scene_seconds{};
    float radius{}; // asteroid collision radius;0 for indestructible planets
};
struct SpaceCombatState {
    bool armed{};
    std::uint64_t shots{},destroyed{};
};
// Allocation happens only in set_world(), before travel. Fixed projectile and
// event pools plus reserved spatial scratch keep every simulation step free
// from allocations. This owns no native state, save data or reward mechanism.
class SpaceCombat final {
public:
    static constexpr std::size_t projectile_capacity=128,event_capacity=32;
    bool set_assets(const SpaceCombatAssets&)noexcept;
    void set_world(const std::vector<Asteroid>&,const Destination*,std::size_t);
    void reset(int native_ship_class=531)noexcept;
    void reset(ShipIdentity)noexcept;
    void clear_projectiles()noexcept;
    void step(const FlightState& ship,bool fire,double scene_seconds=-1.)noexcept;
    const std::array<SpaceProjectile,projectile_capacity>& projectiles()const noexcept{return projectiles_;}
    // Persistent last32 events, ring order after wrap. ids/timestamps let the
    // renderer consume catch-up impacts without per-tick polling or mutation.
    const std::array<SpaceCombatEvent,event_capacity>& events()const noexcept{return events_;}
    std::size_t event_count()const noexcept{return event_count_;}
    const std::vector<std::uint8_t>& alive_mask()const noexcept{return alive_;}
    // Exact continuous contact time, or-1 while alive. Presentation can defer
    // disappearance until its interpolated clock reaches the physical hit.
    const std::vector<double>& destroyed_seconds()const noexcept{return destroyed_seconds_;}
    const std::vector<std::uint8_t>& asteroid_hp()const noexcept{return hp_;}
    std::uint64_t revision()const noexcept{return revision_;}
    std::uint64_t projectile_revision()const noexcept{return projectile_revision_;}
    std::uint64_t event_revision()const noexcept{return event_revision_;}
    const SpaceCombatState& state()const noexcept{return state_;}
    const SpaceCombatAssets& assets()const noexcept{return assets_;}
    const ShipWeaponDefinition* active_weapon()const noexcept;
private:
    SpaceCombatAssets assets_{};
    SpaceCombatState state_{};
    std::unique_ptr<AsteroidSpatialIndex> index_;
    const Destination* planets_{};
    std::size_t planet_count_{};
    std::vector<std::uint8_t> alive_,hp_;
    std::vector<double> destroyed_seconds_;
    AsteroidQueryScratch scratch_;
    std::vector<AsteroidSweepHit> hits_;
    std::array<SpaceProjectile,projectile_capacity> projectiles_{};
    std::array<SpaceCombatEvent,event_capacity> events_{};
    std::size_t event_count_{},next_event_slot_{},next_muzzle_{};
    std::uint32_t cooldown_{};
    const ShipProfile* ship_profile_=ship_profile(ShipIdentity::Green);
    std::uint64_t revision_{},projectile_revision_{},next_projectile_id_{};
    std::uint64_t event_revision_{},next_event_id_{};
    double clock_{};
    void emit(SpaceCombatEventKind,Vec3,Vec3,std::size_t,std::uint64_t,double)noexcept;
    void fire(const FlightState&)noexcept;
    void reset_profile(const ShipProfile*)noexcept;
};
// The same native Euler basis as the existing hull renderer (Rz*Ry*Rx),
// including cosmetic bank and pitch. Translation applies only to positions.
Vec3 weapon_local_to_world(Vec3 local,const FlightState&,bool position)noexcept;
}

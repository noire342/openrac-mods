// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "catalog.hpp"
#include "flight.hpp"
#include "asteroid_contacts.hpp"
#include "ship_damage.hpp"
#include "space_combat.hpp"
#include "destination_route.hpp"
#include <array>
#include <cstddef>
#include <memory>

namespace spacevoyages {
enum class Phase {Ground,Departure,FreeFlight,Approach,Arrival};
// Geometric visibility for the one-second radial approach, independent of
// coordinates and target selection. Padding is outside native hard keepouts.
bool approach_corridor_clear(Vec3 start,const Destination& target,
    const Destination* entries,std::size_t count) noexcept;
// Geometry-only steering along a great circle. Handles opposite headings
// without normalized-linear interpolation's stationary antipodal case.
Vec3 guide_direction_toward(Vec3 heading,Vec3 inward,float fraction) noexcept;
class VoyageController {
public:
    VoyageController();
    VoyageController(const Destination* entries,std::size_t count);
    bool begin(int origin,const std::array<bool,19>& coordinates,int native_ship_class=531);
    bool begin(DestinationRoute origin,const std::array<bool,19>& coordinates,int native_ship_class=531);
    bool begin(int origin,const std::array<bool,19>& coordinates,ShipIdentity);
    bool begin(DestinationRoute origin,const std::array<bool,19>& coordinates,ShipIdentity);
    ShipIdentity ship_identity()const noexcept{return ship_identity_;}
    bool begin_space_entry();
    bool departure_complete();
    float space_entry_progress()const noexcept{return std::clamp(entry_time_/space_entry_duration,0.f,1.f);}
    float approach_progress()const noexcept{return phase_==Phase::Arrival?1.f:std::clamp(approach_time_/approach_duration,0.f,1.f);}
    void tick(double seconds,const FlightInput& input,bool triangle,bool paused=false,bool fire=false);
    void set_coordinates(const std::array<bool,19>& coordinates);
    void set_route_availability(RouteAvailability available) noexcept{route_availability_=available;}
    // Scene geometry is installed once at Ground, independently of unlocks.
    // Old resource packs leave this empty and retain the original flight.
    void set_asteroids(AsteroidLayout layout,bool solid=true);
    // Configuration is immutable while the world is suspended. This affects
    // only the scene's ship hull; native Ratchet health is never read/written.
    void set_asteroid_damage(bool enabled);
    void set_combat_assets(const SpaceCombatAssets& assets);
    const SpaceCombat& combat()const noexcept{return combat_;}
    bool asteroid_damage_enabled()const noexcept{return asteroid_damage_enabled_;}
    const ShipDamageState& ship_damage()const noexcept{return ship_damage_.state();}
    // Acknowledge only after rendering a final fully-black destruction frame.
    // The next non-paused simulation tick performs the local-space respawn.
    bool acknowledge_destruction_frame()noexcept;
    const AsteroidLayout* asteroid_layout()const noexcept{return asteroid_layout_.asteroids().empty()?nullptr:&asteroid_layout_;}
    const AsteroidContactStats& asteroid_contacts()const noexcept{return asteroid_contacts_?asteroid_contacts_->stats():empty_contacts_;}
    double scene_seconds()const noexcept{return scene_seconds_;}
    const Destination* candidate()const;
    bool candidate_unlocked()const;
    bool destination_unlocked(int level)const;
    DestinationRoute resolved_route(int level)const noexcept;
    DestinationRoute origin_route()const noexcept{return origin_route_;}
    DestinationRoute target_route()const noexcept{return target_?target_route_:DestinationRoute{};}
    const Destination* catalog_data()const noexcept{return entries_;}
    std::size_t catalog_count()const noexcept{return count_;}
    int take_destination();
    // A phase listener may invalidate coordinates between reaching Arrival and
    // handing its destination to the native loader. Stay in the same scene.
    bool cancel_arrival_to_flight();
    Phase phase()const noexcept{return phase_;}
    int origin()const noexcept{return origin_;}
    const FlightState& flight()const noexcept{return flight_.state();}
    const FlightState& presentation_flight()const noexcept{return asteroid_contacts_?asteroid_visual_:flight_.presentation_state();}
    int target_level()const noexcept;
    bool arrival_complete();
    void cancel();
    static constexpr float approach_duration=1.f;
    static constexpr float space_entry_duration=1.75f;
    static constexpr float keepout_margin=10.f,prompt_margin=50.f,hysteresis=10.f;
    static constexpr float galaxy_soft_radius=galaxy_bounds::soft_radius,galaxy_radius=galaxy_bounds::hard_radius;
private:
    const Destination* entries_{};std::size_t count_{};bool catalog_valid_{};
    std::array<bool,19> coordinates_{};
    RouteAvailability route_availability_{};
    DestinationRoute origin_route_{},target_route_{};
    ShipIdentity ship_identity_=ShipIdentity::Green;
    FlightController flight_;
    AsteroidLayout asteroid_layout_;
    SpaceCombat combat_;
    std::unique_ptr<AsteroidContacts> asteroid_contacts_;
    AsteroidContactStats empty_contacts_{};
    FlightState asteroid_visual_{};
    ShipDamage ship_damage_;
    bool asteroid_damage_enabled_=true,destruction_frame_acknowledged_{};
    ShipImpactSample asteroid_impact_{};
    Vec3 respawn_position_{};
    float respawn_yaw_{},respawn_pitch_{};
    double scene_seconds_{};
    Phase phase_=Phase::Ground;int origin_=-1,pending_=-1;
    const Destination* candidate_{},*target_{};
    bool triangle_held_{};
    bool entry_active_{};float entry_time_{};
    bool triangle_pending_{};double accumulator_{};
    float approach_time_{};Vec3 approach_start_{},approach_end_{};
    float approach_yaw_{},approach_pitch_{},approach_target_yaw_{},approach_target_pitch_{};
    bool unlocked(const Destination& d)const;
    bool approach_corridor_clear(const Destination& target)const noexcept;
    bool departure_pose(const Destination& from,Vec3 preferred,Vec3& position,Vec3& facing)const;
    void boundaries(Vec3 previous,float seconds);
    void select_candidate();
    void update_asteroid_visual();
    void respawn_in_space();
    void step(const FlightInput& input,bool pressed,bool fire);
};
}

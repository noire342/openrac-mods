// SPDX-License-Identifier: GPL-3.0-or-later
#include "voyage.hpp"
#include "travel_policy.hpp"
#include "input_adapter.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spacevoyages;
namespace {
int checks;
void require(bool test,const char* message){++checks;if(!test){std::fprintf(stderr,"voyage: %s\n",message);std::exit(1);}}
constexpr std::array<Destination,2> route{{{1,"Origin",{-250,0,0},20},{2,"Destination",{250,0,0},25}}};
std::array<bool,19> coordinates(){std::array<bool,19> c{};c[1]=c[2]=true;return c;}
void fly_until_candidate(VoyageController& v){for(int i=0;i<1800&&(!v.candidate()||celestial_body_for_level(v.candidate()->level)==celestial_body_for_level(v.origin()));++i)v.tick(1./60,{0,0,true},false);}
}
int main(){
    require(valid_catalog(prototype_catalog().data(),prototype_catalog().size()),"prototype catalogue is geometricallyvalid");
    require(celestial_body_for_level(0)==celestial_body_for_level(18),"Veldin revisit shares one celestial body");
    require(celestial_body_for_level(10)!=celestial_body_for_level(14),"Orxon10 and Oltanis14 are distinct celestial bodies");
    require(celestial_body_for_level(98)==-1,"menus never become celestialdestinations");
    auto revisit_coordinates=coordinates();revisit_coordinates[18]=true;revisit_coordinates[14]=true;
    require(coordinates_for_body(revisit_coordinates,0)&&coordinates_for_body(revisit_coordinates,14)&&!coordinates_for_body(revisit_coordinates,10),"late-story coordinates never grant a different planet");
    require(!coordinates_for_body(revisit_coordinates,-1),"invalid body cannot borrow invalid coordinate");
    {
        std::array<bool,19> early{};early[0]=true;
        require(!coordinates_for_body(early,18)&&!coordinates_for_body(early,0),"initial Veldin coordinates cannot unlock its final native map marker");
        const auto& full=galaxy_catalog();VoyageController defaults;
        require(full.size()==18&&valid_catalog(full.data(),full.size())&&defaults.catalog_data()==full.data()&&defaults.catalog_count()==18,"default flight uses one validated complete galaxy");
        require(find_destination(full.data(),full.size(),0)==find_destination(full.data(),full.size(),18)&&find_destination(full.data(),full.size(),0)->level==18,"one Veldin mesh resolves both physical aliases to native landing18");
        // Literal retail table values, independently read from SCUS_971.99 at
        // ELF offset0x000DEDA8. This verifies ordering, handedness and scale.
        constexpr int native_xy[18][2]={{35,80},{80,95},{85,130},{90,65},{108,110},{160,110},{120,85},{150,150},{180,130},{200,110},{220,140},{280,100},{230,80},{210,40},{200,50},{160,40},{130,50},{45,90}};
        constexpr int designed_depth[18]={-130,75,-95,145,-35,120,15,-165,35,-100,135,-25,-150,155,70,-40,-125,-50};
        for(std::size_t i=0;i<full.size();++i){
            require(std::abs(full[i].position.x-(native_xy[i][0]-157.5f)*8.75f)<.0001f&&std::abs(full[i].position.y-(95.f-native_xy[i][1])*8.75f)<.0001f,"full catalogue physically expands native-map XY2.5x to scale8.75");
            require(full[i].position.z==designed_depth[i]*2.5f,"authored 3D depths expand by the same factor without altering native-map provenance");
            require(full[i].asset_id&&*full[i].asset_id,"each full destination identifies its renderer asset independently of catalogue order");
        }
        const auto* kerwan=find_destination(full.data(),full.size(),3);const auto* novalis=find_destination(full.data(),full.size(),1);
        const auto* gemlik=find_destination(full.data(),full.size(),13);const auto* oltanis=find_destination(full.data(),full.size(),14);
        require(kerwan->position.y>novalis->position.y&&gemlik->position.x>oltanis->position.x&&gemlik->position.y>oltanis->position.y,"map north and relative planet ordering retain native handedness");
        float shortest=10000,shortest_planets=10000,longest=0;for(std::size_t i=0;i<full.size();++i)for(std::size_t j=0;j<i;++j){const float d=length(full[i].position-full[j].position);shortest=std::min(shortest,d);longest=std::max(longest,d);if(full[i].kind==DestinationKind::Planet&&full[j].kind==DestinationKind::Planet)shortest_planets=std::min(shortest_planets,d);}
        require(shortest>235&&shortest<236&&shortest_planets>305&&shortest_planets<307,"expanded orbital neighbours exceed235units and the closest planet pair is about306units apart");
        require(longest>2166&&longest<2167&&longest/FlightController::boost_speed>72&&longest/FlightController::boost_speed<=72.3f,"the real expanded maximum distance takes about72.23seconds at unchanged native boost speed");
        require(galaxy_bounds::catalog_limit==galaxy_bounds::hard_radius-40&&VoyageController::galaxy_radius==galaxy_bounds::hard_radius&&VoyageController::galaxy_soft_radius==galaxy_bounds::soft_radius,"catalogue validation and flight boundaries share one bounds definition");
        // Every origin, all-unlocked, no-other-unlock, and every single flag
        // (including initialVeldin0) must have a deterministic safe intro.
        for(const auto& from:full)for(int pattern=-2;pattern<19;++pattern){
            std::array<bool,19> available{};if(pattern==-2)available.fill(true);else if(pattern>=0)available[unsigned(pattern)]=true;
            VoyageController departure,duplicate;
            require(departure.begin(from.level,available)&&duplicate.begin(from.level,available),"all18 origins have a safe departure for every tested unlock pattern");
            const auto start=departure.flight().position;const auto heading=direction(departure.flight().yaw,departure.flight().pitch);
            require(length(start-duplicate.flight().position)<.0001f&&departure.flight().yaw==duplicate.flight().yaw&&departure.flight().pitch==duplicate.flight().pitch,"departure heading search is deterministic");
            require(departure.begin_space_entry(),"safe full-galaxy departure starts its entry cinematic");
            for(int frame=0;frame<=105;++frame){
                const auto p=departure.flight().position;
                require(length(p)<=VoyageController::galaxy_soft_radius+.001f,"full intro remains inside the outer guidance band");
                for(const auto& body:full){const float margin=body.level==from.level?VoyageController::keepout_margin:VoyageController::keepout_margin+22.f;
                    require(length(p-body.position)>=body.radius+margin-.002f,"spawn and complete intro avoid every other body's soft boundary and all hard keepouts");}
                if(frame<105)departure.tick(1./60,{1,-1,true},true);
            }
            const float entry_error=length(departure.flight().position-(start+heading*42.f));
            // At expanded positions above1000, repeated float additions have
            // larger ULPs. Bound that numeric error without changing physics.
            const float entry_tolerance=(length(start)+42.f)*std::numeric_limits<float>::epsilon()*105.f+.001f;
            require(departure.phase()==Phase::FreeFlight&&entry_error<entry_tolerance,"safe entry flies the same42unit native cruise ray within world-position float precision");
        }
        require(!defaults.begin(0,early)&&defaults.phase()==Phase::Ground,"initial Veldin escape cannot resolve to the final landing program");
    }
    {
        const auto& full=galaxy_catalog();
        for(const auto& target:full){
            unsigned clear=0;
            for(int angle=0;angle<72;++angle)for(float pitch:{-.4f,0.f,.4f}){
                const Vec3 start=target.position+direction(float(angle)*pi/36.f,pitch)*(target.radius+VoyageController::prompt_margin-.5f);
                if(!approach_corridor_clear(start,target,full.data(),full.size()))continue;
                ++clear;
                const Vec3 end=target.position+(start-target.position)*((target.radius+VoyageController::keepout_margin+2.f)/length(start-target.position));
                // Independent dense point samples validate the continuous
                // analytic swept-sphere policy used by the production helper.
                for(int step=0;step<=60;++step){const Vec3 p=start+(end-start)*(float(step)/60);
                    for(const auto& other:full)if(&other!=&target)
                        require(length(p-other.position)>other.radius+VoyageController::keepout_margin+1.f-.001f,"every accepted full-galaxy entry corridor avoids all other padded hard keepouts");
                }
            }
            require(clear>0,"all18 destinations retain reachable clear entry corridors");
        }
        for(const auto pair:{std::array<int,2>{18,17},std::array<int,2>{14,13}}){
            // Retain the old compact planar pairs as regression fixtures even
            // though the production galaxy now separates Z and expands2.5x.
            std::array<Destination,2> compact{{*find_destination(full.data(),full.size(),pair[0]),*find_destination(full.data(),full.size(),pair[1])}};
            for(auto& body:compact){body.position=body.position/2.5f;body.position.z=0;}
            const auto& target=compact[0];const auto& blocker=compact[1];
            const Vec3 outward=unit(blocker.position-target.position);
            const Vec3 blocked_start=blocker.position+outward*(blocker.radius+VoyageController::keepout_margin+.5f);
            const Vec3 clear_start=target.position-outward*(target.radius+VoyageController::keepout_margin+5.f);
            require(length(blocked_start-target.position)<target.radius+VoyageController::prompt_margin+VoyageController::hysteresis,"compact native pair reproduces an occluded target still inside its hysteresis range");
            require(!approach_corridor_clear(blocked_start,target,compact.data(),compact.size())&&approach_corridor_clear(blocked_start,blocker,compact.data(),compact.size()),"compact Veldin/Fleet and Oltanis/Gemlik regression fixtures cannot be approached through the intervening body");
            std::array<bool,19> all{};all.fill(true);VoyageController select(compact.data(),compact.size());select.begin(target.level,all);select.departure_complete();
            // Geometry fixture only: place the pure controller on each side of
            // the compact pair. No guest RAM, native world, or runtime pose is
            // changed. This isolates hysteresis from a piloting route.
            auto& fixture_pose=const_cast<FlightState&>(select.flight());
            fixture_pose.position=clear_start;fixture_pose.yaw=yaw_of(-outward);fixture_pose.pitch=pitch_of(-outward);
            select.tick(1./60,{},false);
            require(select.candidate()&&select.candidate()->level==target.level,"an unobstructed nearby body becomes the stable candidate");
            const Vec3 side=unit(cross(outward,{0,0,1}));
            const Vec3 overlap=target.position+unit(outward+side)*(target.radius+36.f);
            require(length(overlap-blocker.position)-blocker.radius<length(overlap-target.position)-target.radius&&approach_corridor_clear(overlap,target,compact.data(),compact.size()),"compact pair provides a closer alternative with an unobstructed existing target");
            fixture_pose.position=overlap;fixture_pose.yaw=yaw_of(unit(outward+side));fixture_pose.pitch=0;
            select.tick(1./60,{},false);
            require(select.candidate()&&select.candidate()->level==target.level,"valid corridor preserves hysteresis even when another visible body becomes closer");
            fixture_pose.position=blocked_start;fixture_pose.yaw=yaw_of(outward);fixture_pose.pitch=pitch_of(outward);
            select.tick(1./60,{},true);
            require(select.candidate()&&select.candidate()->level==blocker.level&&select.phase()==Phase::Approach,"an occluded hysteresis candidate yields to the visible body before Triangle can start approach");
            const auto from=select.flight().position;
            for(int frame=0;frame<61;++frame){select.tick(1./60,{1,1,true},true);for(const auto& other:compact)if(other.level!=blocker.level)
                require(length(select.flight().position-other.position)>=other.radius+VoyageController::keepout_margin-.001f,"the selected compact-pair approach never clips an unrelated body");}
            require(length(select.flight().position-from)>0&&select.take_destination()==blocker.level,"the visible-body approach still completes its single native handoff");
            std::array<bool,19> locked{};VoyageController unknown(compact.data(),compact.size());unknown.begin(target.level,locked);unknown.departure_complete();
            auto& unknown_pose=const_cast<FlightState&>(unknown.flight());unknown_pose.position=blocked_start;unknown_pose.yaw=yaw_of(outward);unknown_pose.pitch=pitch_of(outward);
            unknown.tick(1./60,{},true);
            require(unknown.candidate()&&unknown.candidate()->level==blocker.level&&!unknown.candidate_unlocked()&&unknown.phase()==Phase::FreeFlight&&unknown.take_destination()==-1,"an unknown visible occluder can be selected without granting entry or exposing an obstructed target");
        }
    }
    {
        std::array<bool,19> available{};std::array<std::uint8_t,19> visits{};
        for(int level=0;level<19;++level){const bool expected=level!=0&&level!=7&&level!=13&&level!=14&&level!=17;
            require(ordinary_departure_allowed(level,available,visits)==expected,"ordinary departure policy conservatively retains first mandatory story routes");}
        require(!ordinary_departure_allowed(-1,available,visits)&&!ordinary_departure_allowed(19,available,visits),"menus and invalid levels cannot become departures");
        for(int level:{7,14,17}){visits[unsigned(level)]=1;require(!ordinary_departure_allowed(level,available,visits),"a first visit alone cannot bypass a mandatory departure");visits[unsigned(level)]=2;require(ordinary_departure_allowed(level,available,visits),"completed story departure enables ordinary revisits");}
        visits[13]=2;require(!ordinary_departure_allowed(13,available,visits),"Gemlik's first fighter mission remains native even with a visit flag");
        available[14]=true;require(ordinary_departure_allowed(13,available,visits),"Oltanis coordinates allow ordinary later Gemlik departures");
        const auto before_coordinates=available;const auto before_visits=visits;
        require(!ordinary_departure_allowed(0,available,visits)&&available==before_coordinates&&visits==before_visits,"policy never grants coordinates, visits or an initial Veldin departure");
    }
    {
        constexpr std::array<Destination,7> closed_intro{{{1,"Origin",{0,0,0},12},{2,"East",{80,0,0},35},{3,"West",{-80,0,0},35},{4,"North",{0,80,0},35},{5,"South",{0,-80,0},35},{6,"Up",{0,0,80},35},{7,"Down",{0,0,-80},35}}};
        std::array<bool,19> all{};all.fill(true);VoyageController refuse(closed_intro.data(),closed_intro.size());
        require(valid_catalog(closed_intro.data(),closed_intro.size())&&!refuse.begin(1,all)&&refuse.phase()==Phase::Ground,"a valid but intro-blocked catalogue safely declines takeover instead of spawning in a soft boundary");
    }
    {
        constexpr std::array<Destination,2> late_route{{{1,"Origin",{-250,0,0},20},{18,"Veldin",{250,0,0},12}}};
        std::array<bool,19> early{};early[0]=early[1]=true;VoyageController late(late_route.data(),late_route.size());
        require(late.begin(1,early)&&late.departure_complete(),"late-Veldin fixture departs an ordinary origin");fly_until_candidate(late);
        require(late.candidate()&&late.candidate()->level==18&&!late.candidate_unlocked(),"early Veldin flag does not identify final Veldin as an unlocked destination");
        late.tick(1./60,{},true);require(late.phase()==Phase::FreeFlight,"early story coordinates cannot enter final Veldin");
        early[18]=true;late.set_coordinates(early);late.tick(1./60,{},false);late.tick(1./60,{},true);
        require(late.phase()==Phase::Approach&&late.target_level()==18,"late coordinates resolve the Veldin body to native destination18");
        for(int i=0;i<61;++i)late.tick(1./60,{},true);
        require(late.take_destination()==18&&late.take_destination()==-1,"Veldin emits one final native landing18 and never initial0");
        constexpr Destination home{18,"Veldin",{0,0,0},12};VoyageController returning(&home,1);std::array<bool,19> absent{};
        require(returning.begin(18,absent)&&returning.departure_complete()&&returning.candidate_unlocked(),"origin18 remains a valid route home without granting its coordinate flag");
        returning.tick(1./60,{},true);for(int i=0;i<61;++i)returning.tick(1./60,{},true);
        require(returning.target_level()==18&&returning.take_destination()==18&&!absent[18],"return to origin18 never loads initialVeldin0 or modifies campaign coordinates");
    }
    {
        constexpr DestinationRoute home{18,DestinationVariant::VeldinGarage},final{18,DestinationVariant::Campaign};
        constexpr std::array<Destination,2> route_home{{{1,"Origin",{-250,0,0},20},{18,"Veldin",{250,0,0},12}}};
        std::array<bool,19> early{};early[1]=true;const auto before=early;
        VoyageController supported(route_home.data(),route_home.size());supported.set_route_availability({true});
        require(supported.begin(1,early)&&supported.origin_route()==DestinationRoute{1}&&supported.departure_complete(),"host-verified availability begins an ordinary campaign-origin journey");fly_until_candidate(supported);
        require(supported.candidate()&&supported.candidate()->level==18&&supported.candidate_unlocked()&&supported.resolved_route(0)==home&&supported.resolved_route(18)==home,"one physical Veldin marker advertises the registered pre-final home route");
        supported.tick(1./60,{},true);
        require(supported.phase()==Phase::Approach&&supported.target_route()==home&&supported.target_level()==18,"entry captures a garage route while legacy API reports native18");
        for(int i=0;i<61;++i)supported.tick(1./60,{},true);
        require(supported.take_destination()==18&&supported.take_destination()==-1&&supported.target_route()==home&&early==before,"home handoff emits native18 once and keeps separate logical identity without coordinate writes");
        const auto fallback=supported.origin_route();supported.cancel();
        require(fallback==DestinationRoute{1}&&!supported.origin_route().valid()&&!supported.target_route().valid(),"cancel clears current route state while the host can retain an explicit fallback value");
        VoyageController unavailable(route_home.data(),route_home.size());
        require(!unavailable.begin(home,early)&&unavailable.phase()==Phase::Ground&&!unavailable.origin_route().valid(),"a garage origin without actual entry support cannot be reinterpreted as campaign18");
        unavailable.set_route_availability({true});
        require(!unavailable.begin(DestinationRoute{1,DestinationVariant::VeldinGarage},early)&&!unavailable.begin(0,early)&&!unavailable.begin(DestinationRoute{0},early),"invalid garage program and tutorial0 stay rejected even with home capability");
        constexpr Destination single_home{18,"Veldin",{0,0,0},12};VoyageController departing_home(&single_home,1);departing_home.set_route_availability({true});
        require(departing_home.begin(home,early)&&departing_home.origin()==18&&departing_home.origin_route()==home&&departing_home.departure_complete(),"home origin captures logical mount independently from native program18");
        departing_home.tick(1./60,{},true);for(int i=0;i<61;++i)departing_home.tick(1./60,{},true);
        require(departing_home.target_route()==home&&departing_home.take_destination()==18&&departing_home.origin_route()!=final,"return home never silently converts its origin or target to final campaign Veldin");
        for(bool initially_final:{false,true}){
            auto flags=early;flags[18]=initially_final;
            VoyageController changed(route_home.data(),route_home.size());changed.set_route_availability({true});changed.begin(1,flags);changed.departure_complete();fly_until_candidate(changed);changed.tick(1./60,{},true);
            const auto captured=changed.target_route();flags[18]=!initially_final;changed.set_coordinates(flags);
            require(captured==(initially_final?final:home)&&changed.target_route()==captured&&changed.resolved_route(18)!=captured&&changed.target_level()==18,"story change is detectable without changing an already captured native18 route");
            changed.tick(1./60,{},true);
            require(changed.phase()==Phase::FreeFlight&&!changed.target_route().valid()&&changed.take_destination()==-1&&changed.candidate_unlocked(),"home/final switch cancels approach rather than silently redirecting");
            changed.tick(1./60,{},true);require(changed.phase()==Phase::FreeFlight,"held Triangle cannot restart a changed logical route");
            changed.tick(1./60,{},false);changed.tick(1./60,{},true);
            const auto replacement=initially_final?home:final;
            require(changed.phase()==Phase::Approach&&changed.target_route()==replacement,"a fresh press confirms the newly resolved logical route");
            for(int i=0;i<61;++i)changed.tick(1./60,{},true);
            flags[18]=initially_final;changed.set_coordinates(flags);
            require(changed.phase()==Phase::Arrival&&changed.target_route()==replacement&&changed.take_destination()==-1&&changed.phase()==Phase::FreeFlight,"an Arrival listener changing the logical variant cannot leak a stale native18 handoff");
        }
        VoyageController revoked(route_home.data(),route_home.size());revoked.set_route_availability({true});revoked.begin(1,early);revoked.departure_complete();fly_until_candidate(revoked);revoked.tick(1./60,{},true);
        revoked.set_route_availability({false});revoked.tick(1./60,{},true,true);
        require(revoked.phase()==Phase::Approach&&revoked.target_route()==home,"paused simulation keeps the captured approach until a real step");
        revoked.tick(1./60,{},true);
        require(revoked.phase()==Phase::FreeFlight&&!revoked.candidate_unlocked()&&revoked.take_destination()==-1,"revoked host entry capability cancels home without unlocking campaign18");
        revoked.set_route_availability({true});revoked.tick(1./60,{},true);
        require(revoked.phase()==Phase::FreeFlight,"restored provider does not manufacture a Triangle press");
        revoked.tick(1./60,{},false);revoked.tick(1./60,{},true);for(int i=0;i<61;++i)revoked.tick(1./60,{},true);
        const auto entered_route=revoked.target_route();const int entered_level=revoked.take_destination();
        auto unlocked=early;unlocked[18]=true;revoked.set_coordinates(unlocked);
        require(entered_level==18&&revoked.destination_unlocked(entered_level)&&entered_route==home&&revoked.resolved_route(entered_level)!=entered_route,"cached final handoff must compare logical route, because body availability and native18 alone still match");
        require(revoked.cancel_arrival_to_flight(),"host can revoke a cached handoff after final presentation");revoked.tick(1./60,{},true);
        require(revoked.phase()==Phase::FreeFlight&&early==before,"final recovery requires fresh input and preserves original campaign coordinates");
        const auto& full=galaxy_catalog();
        for(const auto& from:full){
            VoyageController intro;intro.set_route_availability({true});std::array<bool,19> only_home{};
            require(intro.begin(from.level,only_home)&&intro.begin_space_entry(),"new home eligibility retains a valid departure from every campaign body");
            for(int frame=0;frame<=105;++frame){
                const auto position=intro.flight().position;
                for(const auto& body:full)require(length(position-body.position)>=body.radius+VoyageController::keepout_margin-.002f,"home-directed departure and complete intro never cross a planet keepout");
                if(frame<105)intro.tick(1./60,{},false);
            }
            require(intro.phase()==Phase::FreeFlight&&intro.origin_route()==DestinationRoute{from.level},"home capability never changes the captured campaign origin or native entry duration");
        }
    }
    auto duplicate=route;duplicate[1].level=1;
    require(!valid_catalog(duplicate.data(),duplicate.size()),"duplicate bodies rejectedbeforeflight");
    auto overlap=route;overlap[1].position=overlap[0].position;
    require(!valid_catalog(overlap.data(),overlap.size()),"overlapping keepout volumes rejected");
    auto invalid=route;invalid[1].position.z=std::numeric_limits<float>::infinity();
    require(!valid_catalog(invalid.data(),invalid.size()),"nonfinite catalogue rejected");
    VoyageController v(route.data(),route.size());const auto c=coordinates();
    require(!v.begin(13,c)&&v.phase()==Phase::Ground,"unsupportedorigin leavesgameplayuntouched");
    require(v.begin(1,c)&&v.phase()==Phase::Departure,"voyage begins only for cataloguedorigin");
    require(!v.begin(1,c),"second launch cannot replace anactivevoyage");
    const auto start=v.flight().position;
    v.tick(.2,{0,0,true},false);require(length(v.flight().position-start)==0,"native departure owns its movement untilcomplete");
    require(v.departure_complete()&&!v.departure_complete(),"departure notification isidempotent");
    fly_until_candidate(v);
    require(v.candidate()&&v.candidate()->level==2&&v.candidate_unlocked(),"flying forward reaches eligible destinationwithnopremenu");
    v.tick(1./60,{},true);require(v.phase()==Phase::Approach,"triangle startsapproach onlyneardestination");
    const auto approach_start=v.flight().position;
    for(int i=0;i<120;++i)v.tick(1./60,{},true,true);
    require(v.phase()==Phase::Approach&&length(v.flight().position-approach_start)==0,"pause freezes approach while consumingheldinput");
    for(int i=0;i<61;++i)v.tick(1./60,{},true);
    require(v.phase()==Phase::Arrival&&v.take_destination()==2&&v.take_destination()==-1,"one press produces exactly one destinationcommit");
    for(int i=0;i<120;++i)v.tick(1./60,{},true);
    require(v.take_destination()==-1,"heldtriangle cannot duplicate native travel");
    require(v.arrival_complete()&&v.phase()==Phase::Ground&&!v.arrival_complete(),"arrival releases transient stateonce");
    {
        VoyageController cinematic(route.data(),route.size()),neutral(route.data(),route.size());
        cinematic.begin(1,c);neutral.begin(1,c);
        require(cinematic.begin_space_entry()&&!cinematic.begin_space_entry()&&neutral.begin_space_entry(),"space entry is acquired once after the native departure");
        const auto pose=cinematic.flight().position;
        for(int i=0;i<120;++i)cinematic.tick(1./60,{1,1,true},true,true);
        require(cinematic.space_entry_progress()==0&&length(cinematic.flight().position-pose)==0,"pause freezes both the entry animation and the ship");
        for(int i=0;i<104;++i){cinematic.tick(1./60,{1,1,true},true);neutral.tick(1./60,{},false);}
        require(cinematic.phase()==Phase::Departure&&cinematic.space_entry_progress()>.98f,"the establishing shot keeps control until its full duration");
        require(length(cinematic.flight().position-neutral.flight().position)<.0001f&&cinematic.flight().speed==FlightController::cruise_speed,"steering, turbo and Triangle cannot interrupt cinematic ship entry");
        cinematic.tick(1./60,{1,1,true},true);neutral.tick(1./60,{},false);
        require(cinematic.phase()==Phase::FreeFlight&&cinematic.space_entry_progress()==1&&cinematic.take_destination()==-1,"the native chase view receives control after the space-entry animation");
        require(length(cinematic.flight().position-neutral.flight().position)<.0001f&&std::abs(cinematic.flight().model_roll)<.0001f,"control handoff has no bank or pose discontinuity");
        cinematic.tick(1./60,{1,0,true},false);
        require(cinematic.flight().model_roll>0&&cinematic.flight().speed>FlightController::cruise_speed,"player steering and boost start after the cinematic finishes");
    }
    {
        auto locked=c;locked[2]=false;VoyageController unknown(route.data(),route.size());
        require(unknown.begin(1,locked)&&unknown.departure_complete(),"unknown bodies can coexist with validorigin");
        fly_until_candidate(unknown);
        require(unknown.candidate()&&unknown.candidate()->level==2&&!unknown.candidate_unlocked(),"unknown contact is visible but has no entrypermission");
        unknown.tick(1./60,{},true);require(unknown.phase()==Phase::FreeFlight&&unknown.take_destination()==-1,"locked contact ignoresentry");
        unknown.set_coordinates(c);unknown.tick(1./60,{},true);
        require(unknown.phase()==Phase::FreeFlight,"unlocking beneath heldbutton doesnotautoland");
        unknown.tick(1./60,{},false);unknown.tick(1./60,{},true);
        require(unknown.phase()==Phase::Approach,"newpress afterunlock enablesentry");
        unknown.set_coordinates(locked);unknown.tick(1./60,{},false);
        require(unknown.phase()==Phase::FreeFlight&&unknown.take_destination()==-1,"coordinate revocation cancels pendingapproach");
    }
    {
        VoyageController handoff(route.data(),route.size());
        require(!handoff.cancel_arrival_to_flight(),"arrival recovery cannot start a grounded journey");
        handoff.begin(1,c);handoff.departure_complete();fly_until_candidate(handoff);
        handoff.tick(1./60,{},true);
        require(!handoff.cancel_arrival_to_flight()&&handoff.phase()==Phase::Approach,"arrival recovery cannot interrupt a valid approach");
        for(int i=0;i<61;++i)handoff.tick(1./60,{},true);
        require(handoff.phase()==Phase::Arrival,"handoff fixture reaches Arrival before native destination consumption");
        // Simulate an Arrival listener revoking coordinates after the last
        // approach check but before the runtime consumes take_destination().
        auto revoked=c;revoked[2]=false;handoff.set_coordinates(revoked);
        const auto pose=handoff.flight().position;
        const auto* selected=handoff.candidate();
        require(handoff.cancel_arrival_to_flight()&&handoff.phase()==Phase::FreeFlight,"revoked final handoff can recover inside the existing scene");
        require(handoff.take_destination()==-1&&handoff.target_level()==-1,"recovery discards the invalid native loading request");
        require(handoff.origin()==1&&handoff.candidate()==selected&&!handoff.candidate_unlocked()&&length(handoff.flight().position-pose)==0,"recovery preserves origin, pose and stable unknown contact");
        require(!handoff.cancel_arrival_to_flight(),"arrival recovery is one-shot");
        handoff.set_coordinates(c);handoff.tick(1./60,{},true);
        require(handoff.phase()==Phase::FreeFlight,"regranting coordinates with Triangle still held cannot restart entry");
        handoff.tick(1./60,{},false);handoff.tick(1./60,{},true);
        require(handoff.phase()==Phase::Approach,"entry after recovery requires a fresh Triangle press");
        for(int i=0;i<61;++i)handoff.tick(1./60,{},true);
        require(handoff.take_destination()==2&&handoff.take_destination()==-1,"a later valid approach emits the chosen destination exactly once");
    }
    {
        VoyageController boundary(route.data(),route.size());boundary.begin(1,c);boundary.departure_complete();
        bool near=false;
        for(int i=0;i<60*180;++i){
            boundary.tick(1./60,{0,0,true},false);
            const Vec3 p=boundary.flight().position;
            for(const auto& body:route){const float d=length(p-body.position);require(d>=body.radius+VoyageController::keepout_margin-.001f,"boost cannot cross a celestial keepoutvolume");if(d<body.radius+45)near=true;}
            require(length(p)<=VoyageController::galaxy_radius+.01f&&finite(p),"long flight remains inside finitegalaxy");
        }
        require(near&&boundary.phase()==Phase::FreeFlight,"close flyby neverlandsautomatically");
    }
    {
        VoyageController low(route.data(),route.size()),high(route.data(),route.size());
        low.begin(1,c);low.departure_complete();high.begin(1,c);high.departure_complete();
        for(int i=0;i<30*30;++i)low.tick(1./30,{0,0,true},false);
        for(int i=0;i<144*30;++i)high.tick(1./144,{0,0,true},false);
        require(length(low.flight().position-high.flight().position)<.001f&&std::abs(wrap_angle(low.flight().yaw-high.flight().yaw))<.0001f,"planet avoidance is render-rate independent");
    }
    {
        for(Vec3 outward:{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1},Vec3{0,0,-1}}){
            const auto turned=guide_direction_toward(outward,-outward,.05f);
            require(finite(turned)&&std::abs(length(turned)-1)<.00001f&&dot(turned,-outward)>-.999f,"opposite outer-guide headings choose a real finite great-circle turn");
            require(length(guide_direction_toward(outward,-outward,0)-outward)<.00001f&&length(guide_direction_toward(outward,-outward,1)+outward)<.00001f,"outer guidance preserves its zero/full blend endpoints");
            require(length(turned-guide_direction_toward(outward,-outward,.05f))==0,"antipodal turn direction is deterministic");
            for(float start_radius:{VoyageController::galaxy_soft_radius+1.f,VoyageController::galaxy_radius-.05f})for(int raw=172;raw<=255;++raw){
                constexpr Destination origin{1,"Boundary fixture",{0,0,0},20};VoyageController boundary(&origin,1);std::array<bool,19> known{};known[1]=true;boundary.begin(1,known);boundary.departure_complete();
                auto& pose=const_cast<FlightState&>(boundary.flight());pose.position=outward*start_radius;pose.yaw=yaw_of(outward);pose.pitch=std::clamp(pitch_of(outward),-FlightController::pitch_limit,FlightController::pitch_limit);pose.model_yaw=pose.camera.yaw=pose.yaw;pose.model_pitch=pose.pitch;pose.camera.pitch=wrap_angle(pose.pitch+.09817477f);
                const auto forward=direction(pose.yaw,pose.pitch),left=unit(cross({0,0,1},forward)),up=cross(forward,left);pose.camera.position=pose.position-forward*3.75f+up;
                for(int frame=0;frame<600;++frame){
                    const auto before=boundary.flight();std::uint8_t analog[4]{128,128,std::uint8_t(frame<180?raw:frame<360?128:std::max(0,254-raw)),128};auto input=flight_input_from_pad(analog,0);input.boost=true;boundary.tick(1./60,input,false);const auto& after=boundary.flight();
                    require(length(after.position-before.position)>.00001f,"radial soft/hard outer-edge trajectories do not enter a zero-movement clamp");
                    require(finite(after.position)&&finite(after.camera.position)&&length(after.position)<=VoyageController::galaxy_radius+.01f,"six-axis outer guidance keeps world and camera finite inside the hard guard");
                    require(after.model_yaw==after.yaw&&after.model_pitch==after.pitch,"outer-guide physical and model headings belong to the same60Hz step");
                    const float angular_step=std::acos(std::clamp(dot(direction(before.yaw,before.pitch),direction(after.yaw,after.pitch)),-1.f,1.f));
                    require(angular_step<.08f&&std::isfinite(after.camera.fov),"outer guidance turns smoothly instead of selecting a large evasive heading jump");
                }
            }
        }
    }
    {
        VoyageController cancellation(route.data(),route.size());cancellation.begin(1,c);cancellation.departure_complete();fly_until_candidate(cancellation);
        cancellation.tick(1./60,{},true);cancellation.cancel();
        require(cancellation.phase()==Phase::Ground&&cancellation.origin()==-1&&!cancellation.candidate()&&cancellation.take_destination()==-1,"cancel discards pendingentry and selection");
        require(cancellation.begin(2,c)&&cancellation.departure_complete(),"another voyage can begin aftercancel");
    }
    std::printf("Voyage: %d progression, timing, input and geometry checks passed\n",checks);
}

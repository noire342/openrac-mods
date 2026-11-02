// SPDX-License-Identifier: GPL-3.0-or-later
#include "asteroid_field.hpp"
#include "voyage.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
using namespace spacevoyages;
namespace {std::size_t heap_allocations;}
void* operator new(std::size_t bytes){++heap_allocations;if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks;
void require(bool pass,const char* why){++checks;if(!pass){std::fprintf(stderr,"asteroid_field: %s\n",why);std::exit(1);}}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(const Asteroid& a,const Asteroid& b){return same(a.position,b.position)&&a.radius==b.radius&&same(a.rotation_axis,b.rotation_axis)&&a.angular_speed==b.angular_speed&&a.variant==b.variant&&a.rotation_phase==b.rotation_phase&&a.field_id==b.field_id;}
// Independent geometric reference: closest point of a closed segment, not the
// production quadratic first-contact calculation or grid traversal.
double segment_distance(Vec3 p,Vec3 a,Vec3 b,double* fraction=nullptr){
    const double vx=double(b.x)-a.x,vy=double(b.y)-a.y,vz=double(b.z)-a.z;
    const double ux=double(p.x)-a.x,uy=double(p.y)-a.y,uz=double(p.z)-a.z;
    const double d=vx*vx+vy*vy+vz*vz;
    const double t=d==0?0:std::clamp((ux*vx+uy*vy+uz*vz)/d,0.,1.);
    if(fraction)*fraction=t;
    return std::sqrt((ux-vx*t)*(ux-vx*t)+(uy-vy*t)*(uy-vy*t)+(uz-vz*t)*(uz-vz*t));
}
struct Random {
    unsigned value=48151623;
    float next(){value=value*1664525u+1013904223u;return float(value>>8)*(1.f/16777216.f);}
    Vec3 point(float extent){return {(next()*2-1)*extent,(next()*2-1)*extent,(next()*2-1)*extent};}
};
void compare_queries(const AsteroidSpatialIndex& index,Vec3 start,Vec3 end,float radius){
    std::vector<std::size_t> expected_sphere,expected_sweep,actual_sphere,actual_sweep;
    const auto& rocks=index.asteroids();
    for(std::size_t i=0;i<rocks.size();++i){
        if(segment_distance(rocks[i].position,start,start)<=double(radius)+rocks[i].radius)expected_sphere.push_back(i);
        if(segment_distance(rocks[i].position,start,end)<=double(radius)+rocks[i].radius)expected_sweep.push_back(i);
    }
    std::vector<AsteroidSweepHit> swept;
    require(index.query_sphere(start,radius,actual_sphere)&&index.sweep_sphere(start,end,radius,swept),"valid query geometry succeeds");
    for(std::size_t i=0;i<swept.size();++i){
        const auto& hit=swept[i];actual_sweep.push_back(hit.index);
        require(hit.time>=0&&hit.time<=1&&finite(hit.normal)&&std::abs(length(hit.normal)-1)<.0001f,"swept contact time and normal are finite and normalized");
        if(i)require(swept[i-1].time<hit.time||(swept[i-1].time==hit.time&&swept[i-1].index<hit.index),"contacts are uniquely ordered by time and stable index");
        const auto& rock=rocks[hit.index];const auto point=lerp(start,end,hit.time);
        const float combined=radius+rock.radius;
        if(length(start-rock.position)>combined+.001f)
            require(std::abs(length(point-rock.position)-combined)<.02f,"earliest contact lies on the expanded sphere surface");
    }
    std::sort(actual_sweep.begin(),actual_sweep.end());
    require(actual_sphere==expected_sphere,"indexed sphere results match independent brute force without duplicates");
    require(actual_sweep==expected_sweep,"continuous swept results match independent segment geometry without tunneling");
}
}
int main(){
    const auto& planets=galaxy_catalog();const auto& definitions=asteroid_field_catalog();
    const AsteroidLayoutPolicy policy{};
    const auto layout=build_asteroid_layout(planets.data(),planets.size());
    const auto duplicate=build_asteroid_layout(planets.data(),planets.size());
    require(layout.complete()&&layout.asteroids().size()==176&&layout.fields().size()==8,"eight localized route fields retain the same176-rock instance budget");
    const std::array<std::array<int,2>,8> routes{{{{18,1}},{{3,2}},{{8,9}},{{5,11}},{{12,16}},{{15,14}},{{13,10}},{{7,4}}}};
    Vec3 lowest=definitions.front().center,highest=lowest;double authored_volume=0;
    for(std::size_t f=0;f<definitions.size();++f){
        const auto& field=definitions[f];const auto* from=find_destination(planets.data(),planets.size(),routes[f][0]);
        const auto* to=find_destination(planets.data(),planets.size(),routes[f][1]);
        require(from&&to&&same(field.center,(from->position+to->position)*.5f),"each authored zone remains at the midpoint of its own distinct interplanetary route");
        require(dot(unit(field.passage_axis),unit(to->position-from->position))>.99999f,"each safe passage follows the corresponding interplanetary route");
        require(field.count==22&&field.passage_radius>=22.f&&length(field.center)>300.f,"all eight fields share the object budget and avoid a single dense central pile");
        for(std::size_t j=0;j<f;++j)require(length(field.center-definitions[j].center)>275.f,"separate route-zone centers do not collapse into adjacent chunks");
        lowest={std::min(lowest.x,field.center.x),std::min(lowest.y,field.center.y),std::min(lowest.z,field.center.z)};
        highest={std::max(highest.x,field.center.x),std::max(highest.y,field.center.y),std::max(highest.z,field.center.z)};
        authored_volume+=double(field.half_extents.x)*field.half_extents.y*field.half_extents.z;
    }
    require(highest.x-lowest.x>1400&&highest.y-lowest.y>700&&highest.z-lowest.z>700,"the fixed route network spans widely separated quadrants and genuine three-dimensional altitude");
    const double previous_volume=135.*100*70+110.*125*90+110.*90*110;
    require(authored_volume>previous_volume*1.8,"the same176 rocks occupy over1.8times the former ellipsoid volume, reducing the average density");
    auto reordered=planets;std::reverse(reordered.begin(),reordered.end());
    const auto another_region=build_asteroid_layout(reordered.data(),reordered.size());
    auto single_variant=policy;single_variant.variants=1;single_variant.variant_radius_count=0;single_variant.variant_radii=nullptr;
    const auto one_model=build_asteroid_layout(planets.data(),planets.size(),single_variant);
    require(one_model.complete(),"a generic single-model uniform-radius policy remains supported");
    for(std::size_t i=0;i<layout.asteroids().size();++i){
        const auto& rock=layout.asteroids()[i];
        require(same(rock,duplicate.asteroids()[i])&&same(rock,another_region.asteroids()[i]),"layout repeats exactly and does not depend on catalog ordering or regional native state");
        require(finite(rock.position)&&rock.radius>=2.4f&&rock.radius<=6.4f&&std::abs(length(rock.rotation_axis)-1)<.00001f&&
            std::abs(rock.angular_speed)>=.025f&&std::abs(rock.angular_speed)<=.135f&&rock.rotation_phase>=0&&rock.rotation_phase<2*pi&&rock.variant<2,
            "static geometry and visual spin/phase/variant are finite and bounded");
        const auto& radii=asteroid_default_radius_ranges[rock.variant];
        require(rock.radius>=radii.minimum&&rock.radius<=radii.maximum,"large-rock and fragment radius distributions follow the separate model-neutral data");
        require(asteroid_outside_protected_volumes(rock,planets.data(),planets.size(),nullptr,0,policy),"every generated sphere respects all planet and outer exclusions");
        for(const auto& planet:planets)require(length(rock.position-planet.position)>rock.radius+planet.radius+90,"rocks leave the full planet surface/intro/entry envelope empty");
        for(std::size_t j=0;j<i;++j)require(length(rock.position-layout.asteroids()[j].position)>rock.radius+layout.asteroids()[j].radius+policy.separation,"rocks retain their conservative mutual gap");
    }
    const AsteroidSpatialIndex index(layout.asteroids());
    std::vector<AsteroidSweepHit> contacts;
    for(std::size_t f=0;f<layout.fields().size();++f){
        const auto& range=layout.fields()[f];const auto& field=definitions[f];
        require(range.id==field.id&&range.count==field.count&&range.requested==range.count&&same(range.center,field.center),"stable chunk ranges preserve authored field ids and counts");
        std::array<unsigned,2> variants{};
        for(std::size_t i=range.begin;i<range.begin+range.count;++i){
            const auto& rock=layout.asteroids()[i];const auto local=rock.position-field.center;
            ++variants[rock.variant];
            require(rock.field_id==field.id&&length(local)+rock.radius<=range.radius+.0001f,"every physical sphere fits its conservative static chunk culling bound");
            const Vec3 side=local-unit(field.passage_axis)*dot(local,unit(field.passage_axis));
            require(length(side)>field.passage_radius+rock.radius,"each field retains an unobstructed central passage");
        }
        require(variants[0]>0&&variants[1]>0,"every static field retains both native model variants for exactly two renderer batches");
        const auto axis=unit(field.passage_axis);
        require(index.sweep_sphere(field.center-axis*range.radius*2,field.center+axis*range.radius*2,field.passage_radius,contacts)&&contacts.empty(),"the complete authored passage is navigable as a swept volume");
        const auto* route_from=find_destination(planets.data(),planets.size(),routes[f][0]);
        const auto* route_to=find_destination(planets.data(),planets.size(),routes[f][1]);
        require(index.sweep_sphere(route_from->position,route_to->position,field.passage_radius,contacts)&&contacts.empty(),"every complete authored route retains its wide passage across all eight fields");
        float nearest=10000;int from=-1,to=-1;
        for(std::size_t p=0;p<planets.size();++p)for(std::size_t q=0;q<p;++q){double fraction;
            const float d=float(segment_distance(field.center,planets[p].position,planets[q].position,&fraction));
            if(fraction>.15&&fraction<.85&&d<nearest){nearest=d;from=planets[p].level;to=planets[q].level;}}
        std::printf("field%u: %zu rocks, closest intermediate route%d->%d center distance%.2f (bound%.2f)\n",field.id,range.count,from,to,nearest,range.radius);
        require(nearest<range.radius,"fields occur along some interplanetary routes rather than only in unreachable outskirts");
    }
    // All378 native unlock patterns use the real spawn/intro planner. Protect
    // the whole path analytically, not just sampled endpoints.
    for(const auto& planet:planets)for(int pattern=-2;pattern<19;++pattern){
        std::array<bool,19> available{};if(pattern==-2)available.fill(true);else if(pattern>=0)available[unsigned(pattern)]=true;
        VoyageController voyage;require(voyage.begin(planet.level,available)&&voyage.begin_space_entry(),"real intro planner accepts every origin/unlock fixture");
        const auto from=voyage.flight().position;
        for(int frame=0;frame<105;++frame)voyage.tick(1./60,{},false);
        require(voyage.phase()==Phase::FreeFlight&&index.sweep_sphere(from,voyage.flight().position,3.f,contacts)&&contacts.empty(),"all378 complete intro sweeps remain free of asteroid collisions");
    }
    for(const auto& planet:planets)for(int yaw=0;yaw<72;++yaw)for(float pitch:{-.95f,-.4f,0.f,.4f,.95f}){
        const auto radial=direction(yaw*pi/36.f,pitch);
        const auto start=planet.position+radial*(planet.radius+VoyageController::prompt_margin+VoyageController::hysteresis);
        const auto end=planet.position+radial*(planet.radius+VoyageController::keepout_margin+2.f);
        require(index.sweep_sphere(start,end,3.f,contacts)&&contacts.empty(),"every tested approach direction including hysteresis range is asteroid-free");
    }
    {
        const ProtectedFlightCorridor protected_path{{-250,-45,25},{40,-45,25},28};
        const auto extra=build_asteroid_layout(planets.data(),planets.size(),definitions.data(),definitions.size(),policy,&protected_path,1);
        require(extra.complete(),"additional authored protected corridors do not require relaxing the default object count");
        for(const auto& rock:extra.asteroids())require(segment_distance(rock.position,protected_path.start,protected_path.end)>rock.radius+protected_path.radius,"optional future corridors exclude the complete rock volume");
    }
    Random random;
    for(int i=0;i<1500;++i){const auto start=random.point(1200),end=random.point(1200);compare_queries(index,start,end,random.next()*18);}
    {
        AsteroidQueryScratch scratch;scratch.reserve(index.asteroids().size());
        std::vector<std::size_t> overlaps;overlaps.reserve(index.asteroids().size());
        std::vector<AsteroidSweepHit> swept;swept.reserve(index.asteroids().size());
        const auto allocations_before=heap_allocations;bool correct=true;
        for(int i=0;i<1000;++i){
            const auto point=index.asteroids()[std::size_t(i)%index.asteroids().size()].position;
            correct=correct&&index.query_sphere(point,1.5f,overlaps,scratch)&&!overlaps.empty();
            correct=correct&&index.sweep_sphere({-1400,-500,-450},{1400,500,450},1.5f,swept,scratch);
            correct=correct&&scratch.candidates.size()<=index.asteroids().size();
        }
        correct=correct&&index.query_sphere({0,0,0},2000.f,overlaps,scratch)&&overlaps.size()==index.asteroids().size();
        require(correct&&heap_allocations==allocations_before,"reserved caller scratch and outputs perform2000 repeated local/global queries with zero heap allocations");
        scratch.stamp=std::numeric_limits<std::uint32_t>::max();
        std::fill(scratch.seen.begin(),scratch.seen.end(),std::numeric_limits<std::uint32_t>::max());
        const auto before_rollover=heap_allocations;
        require(index.query_sphere(index.asteroids().front().position,1.5f,overlaps,scratch)&&!overlaps.empty()&&scratch.stamp==1&&heap_allocations==before_rollover,
            "stamp overflow resets deduplication without missing hits or allocating memory");
        require(!index.query_sphere({0,0,0},1,scratch.candidates,scratch)&&scratch.candidates.empty(),"aliased scratch/output is refused rather than invalidating traversal");
    }
    {
        std::vector<Asteroid> dense;
        for(int x=-4;x<4;++x)for(int y=-4;y<4;++y)for(int z=-4;z<4;++z)dense.push_back({{float(x*3),float(y*3),float(z*3)},.8f});
        const AsteroidSpatialIndex crowded(dense,8);
        for(int i=0;i<400;++i)compare_queries(crowded,random.point(25),random.point(25),random.next()*2);
        AsteroidQueryStats stats;std::vector<std::size_t> hit;
        require(crowded.query_sphere(dense.front().position,.1f,hit,&stats)&&hit.size()==1&&stats.candidate_count<dense.size()/4,"spatial buckets narrow a local query in a dense512-rock field");
        compare_queries(crowded,{-10000,0,0},{10000,0,0},.2f);
    }
    {
        const AsteroidSpatialIndex simple({{{0,0,0},1},{{0,4,0},1},{{-32,0,0},1}});
        std::vector<std::size_t> hit;
        require(simple.query_sphere({2,0,0},1,hit)&&hit==std::vector<std::size_t>{0},"sphere tangency is included at grid edges");
        require(simple.sweep_sphere({-10000,0,0},{10000,0,0},.25f,contacts)&&contacts.size()==2&&contacts[0].index==2&&contacts[1].index==0,
            "a fast sweep finds both intervening rocks even with clear endpoints");
        require(std::abs(contacts[1].time-.4999375f)<.000001f&&contacts[1].normal.x<-.999f,"first-contact time has the analytic value and outward normal");
        require(simple.sweep_sphere({0,0,0},{0,0,0},0,contacts)&&contacts.size()==1&&contacts[0].time==0&&finite(contacts[0].normal),"stationary overlap returns immediate finite contact");
        require(simple.sweep_sphere({-20,1.25f,0},{20,1.25f,0},.25f,contacts)&&contacts.size()==1&&contacts[0].time==.5f,"swept tangency is retained without false tunneling");
        require(!simple.query_sphere({NAN,0,0},1,hit)&&hit.empty()&&!simple.sweep_sphere({0,0,0},{1,0,0},-1,contacts)&&contacts.empty(),"invalid queries fail closed and clear stale results");
        const AsteroidSpatialIndex moved=AsteroidSpatialIndex(simple);
        require(moved.query_sphere({-32,0,0},0,hit)&&hit==std::vector<std::size_t>{2},"copied index owns its data and retains negative-cell boundary hits");
    }
    {
        auto blocked=definitions[0];blocked.half_extents={1,1,1};blocked.count=8;
        const auto empty=build_asteroid_layout(planets.data(),planets.size(),&blocked,1,policy);
        require(!empty.complete()&&empty.asteroids().empty()&&empty.fields()[0].requested==8,"impossible placements finish within their fixed budget and never relax exclusions");
        bool refused=false;try{auto invalid=policy;invalid.maximum_radius=NAN;(void)build_asteroid_layout(planets.data(),planets.size(),invalid);}catch(const std::invalid_argument&){refused=true;}
        require(refused,"invalid layout policy is rejected before generation");
        refused=false;try{const AsteroidSpatialIndex invalid({{{NAN,0,0},1}});(void)invalid;}catch(const std::invalid_argument&){refused=true;}
        require(refused,"invalid physical geometry cannot enter the index");
    }
    std::printf("asteroid_field: %u deterministic layout, protected route and continuous spatial query checks passed\n",checks);
}

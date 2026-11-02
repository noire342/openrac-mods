// SPDX-License-Identifier: GPL-3.0-or-later
#include "asteroid_field.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace spacevoyages {
namespace {
constexpr float query_limit=1000000.f;
bool nonnegative(float f){return std::isfinite(f)&&f>=0.f;}
bool point_in_range(Vec3 p){return finite(p)&&std::abs(p.x)<=query_limit&&std::abs(p.y)<=query_limit&&std::abs(p.z)<=query_limit;}
bool valid_asteroid(const Asteroid& a){return point_in_range(a.position)&&std::isfinite(a.radius)&&a.radius>0&&a.radius<=1024.f&&finite(a.rotation_axis)&&std::isfinite(a.angular_speed)&&std::isfinite(a.rotation_phase);}
bool valid_policy(const AsteroidLayoutPolicy& p){if(!(nonnegative(p.planet_clearance)&&nonnegative(p.separation)&&
    std::isfinite(p.minimum_radius)&&std::isfinite(p.maximum_radius)&&p.minimum_radius>0&&p.maximum_radius>=p.minimum_radius&&p.maximum_radius<=100.f&&
    std::isfinite(p.outer_radius)&&p.outer_radius>p.maximum_radius&&p.outer_radius<=query_limit/2&&p.variants>0&&p.variants<=256&&p.attempts_per_asteroid>0&&p.attempts_per_asteroid<=4096))return false;
    if(p.variant_radius_count){if(!p.variant_radii||p.variant_radius_count!=p.variants)return false;
        for(std::size_t i=0;i<p.variant_radius_count;++i){const auto& r=p.variant_radii[i];if(!std::isfinite(r.minimum)||!std::isfinite(r.maximum)||r.minimum<p.minimum_radius||r.maximum>p.maximum_radius||r.maximum<r.minimum)return false;}}
    return true;
}
bool valid_corridor(const ProtectedFlightCorridor& c){return point_in_range(c.start)&&point_in_range(c.end)&&nonnegative(c.radius)&&c.radius<=query_limit/2;}
double distance_segment_squared(Vec3 p,Vec3 a,Vec3 b){
    const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,dz=double(b.z)-a.z;
    const double px=double(p.x)-a.x,py=double(p.y)-a.y,pz=double(p.z)-a.z;
    const double l2=dx*dx+dy*dy+dz*dz;
    const double t=l2>0?std::clamp((px*dx+py*dy+pz*dz)/l2,0.,1.):0.;
    const double x=px-dx*t,y=py-dy*t,z=pz-dz*t;return x*x+y*y+z*z;
}
struct StableRandom {
    std::uint32_t state;
    std::uint32_t next(){state+=0x9e3779b9u;auto x=state;x=(x^(x>>16))*0x21f0aaadu;x=(x^(x>>15))*0x735a2d97u;return x^(x>>15);}
    float fraction(){return float(next()>>8)*(1.f/16777216.f);}
    float signed_fraction(){return fraction()*2.f-1.f;}
};
bool in_field(Vec3 local,float radius,const AsteroidFieldDefinition& field){
    const float minimum=std::min({field.half_extents.x,field.half_extents.y,field.half_extents.z});
    const float q=local.x*local.x/(field.half_extents.x*field.half_extents.x)+
        local.y*local.y/(field.half_extents.y*field.half_extents.y)+local.z*local.z/(field.half_extents.z*field.half_extents.z);
    const float inner=1.f-radius/minimum;
    if(inner<=0||q>inner*inner)return false;
    const Vec3 axis=unit(field.passage_axis),side=local-axis*dot(local,axis);
    const float clearance=field.passage_radius+radius;
    return length_squared(side)>clearance*clearance;
}
bool sweep_contact(const Asteroid& rock,Vec3 start,Vec3 end,float radius,AsteroidSweepHit& hit){
    const double mx=double(start.x)-rock.position.x,my=double(start.y)-rock.position.y,mz=double(start.z)-rock.position.z;
    const double dx=double(end.x)-start.x,dy=double(end.y)-start.y,dz=double(end.z)-start.z;
    const double r=double(rock.radius)+radius,c=mx*mx+my*my+mz*mz-r*r;
    double t=0;
    if(c>0){
        const double a=dx*dx+dy*dy+dz*dz,b=mx*dx+my*dy+mz*dz;
        if(a==0||b>=0)return false;
        const double disc=b*b-a*c;if(disc<0)return false;
        // Equivalent to(-b-sqrt(disc))/a without cancellation for distant,
        // fast motion whose first contact is close to the start point.
        const double divisor=-b+std::sqrt(disc);if(divisor<=0)return false;
        t=c/divisor;if(t<0||t>1)return false;
    }
    hit.time=float(t);
    const Vec3 normal{float(mx+dx*t),float(my+dy*t),float(mz+dz*t)};
    hit.normal=unit(normal,unit(start-end));return true;
}
}
const std::array<AsteroidFieldDefinition,asteroid_field_count>& asteroid_field_catalog() noexcept {
    // Authored mod locations, not canonical astronomical data. The eight
    // ellipsoids lie at fixed route midpoints, with deliberately different
    // extents and altitudes. Their common total stays176: spreading rocks
    // across the galaxy must not increase the scene's instance budget.
    // The passage axis follows each route; the22--24u tube stays empty.
    static constexpr std::array<AsteroidFieldDefinition,asteroid_field_count> fields{{
        // Veldin--Novalis; Kerwan--Aridia.
        {1,0x19a2457du,{-875,65.625f,-68.75f},{110,80,95},{393.75f,-131.25f,512.5f},24,22},
        {2,0xc063d18bu,{-612.5f,-21.875f,62.5f},{85,125,135},{-43.75f,-568.75f,-600},24,22},
        // Batalia--Gaspar; Rilgar--Pokitaru.
        {3,0x58b7e902u,{284.375f,-218.75f,-81.25f},{80,75,120},{175,175,-337.5f},22,22},
        {4,0xb27e91c3u,{546.875f,-87.5f,118.75f},{145,75,90},{1050,87.5f,-362.5f},24,22},
        // Hoven--Kalebo III; Quartu--Oltanis.
        {5,0x6df835a1u,{196.875f,262.5f,-343.75f},{155,85,65},{-875,262.5f,62.5f},24,22},
        {6,0xe41b70d9u,{196.875f,437.5f,37.5f},{100,70,100},{350,-87.5f,275},22,22},
        // Base Gemlik--Orxon; Umbris--Eudora.
        {7,0x93ca461fu,{503.125f,43.75f,362.5f},{75,150,65},{87.5f,-875,-50},24,22},
        {8,0x2a7d0b65u,{-249.375f,-306.25f,-250},{110,95,85},{-367.5f,350,325},22,22}
    }};return fields;
}
bool asteroid_outside_protected_volumes(const Asteroid& rock,const Destination* planets,std::size_t planet_count,
    const ProtectedFlightCorridor* corridors,std::size_t corridor_count,const AsteroidLayoutPolicy& policy) noexcept {
    if(!valid_asteroid(rock)||!valid_policy(policy)||(!planets&&planet_count)||(!corridors&&corridor_count))return false;
    if(length(rock.position)+rock.radius>policy.outer_radius)return false;
    for(std::size_t i=0;i<planet_count;++i){
        const auto& p=planets[i];if(!point_in_range(p.position)||!nonnegative(p.radius))return false;
        const double r=double(p.radius)+policy.planet_clearance+rock.radius;
        if(distance_segment_squared(rock.position,p.position,p.position)<=r*r)return false;
    }
    for(std::size_t i=0;i<corridor_count;++i){
        const auto& c=corridors[i];if(!valid_corridor(c))return false;
        const double r=double(c.radius)+rock.radius;
        if(distance_segment_squared(rock.position,c.start,c.end)<=r*r)return false;
    }
    return true;
}
bool AsteroidLayout::complete()const noexcept {for(const auto& f:fields_)if(f.count!=f.requested)return false;return true;}
AsteroidLayout build_asteroid_layout(const Destination* planets,std::size_t planet_count,
    const AsteroidFieldDefinition* fields,std::size_t field_count,const AsteroidLayoutPolicy& policy,
    const ProtectedFlightCorridor* corridors,std::size_t corridor_count){
    if(!valid_policy(policy)||(!planets&&planet_count)||(!fields&&field_count)||(!corridors&&corridor_count)||planet_count>64||field_count>64||corridor_count>4096)
        throw std::invalid_argument("invalid asteroid layout inputs");
    for(std::size_t i=0;i<planet_count;++i)if(!point_in_range(planets[i].position)||!nonnegative(planets[i].radius))throw std::invalid_argument("invalid asteroid planet exclusion");
    for(std::size_t i=0;i<corridor_count;++i)if(!valid_corridor(corridors[i]))throw std::invalid_argument("invalid protected asteroid corridor");
    std::size_t total=0;
    for(std::size_t i=0;i<field_count;++i){
        const auto& f=fields[i];if(!point_in_range(f.center)||!finite(f.half_extents)||f.half_extents.x<=0||f.half_extents.y<=0||f.half_extents.z<=0||
            f.half_extents.x>query_limit/2||f.half_extents.y>query_limit/2||f.half_extents.z>query_limit/2||!finite(f.passage_axis)||length_squared(f.passage_axis)<1e-9f||
            !nonnegative(f.passage_radius)||f.passage_radius>query_limit/2||f.count>4096||total>16384-f.count)
            throw std::invalid_argument("invalid asteroid field specification");
        for(std::size_t j=0;j<i;++j)if(fields[j].id==f.id)throw std::invalid_argument("duplicate asteroid field id");
        total+=f.count;
    }
    AsteroidLayout result;result.asteroids_.reserve(total);result.fields_.reserve(field_count);
    for(std::size_t i=0;i<field_count;++i){
        const auto& f=fields[i];StableRandom random{f.seed};const auto begin=result.asteroids_.size();
        for(std::size_t attempt=0;attempt<f.count*policy.attempts_per_asteroid&&result.asteroids_.size()-begin<f.count;++attempt){
            Asteroid rock;
            const Vec3 local{random.signed_fraction()*f.half_extents.x,random.signed_fraction()*f.half_extents.y,random.signed_fraction()*f.half_extents.z};
            const float radius_fraction=random.fraction();
            rock.position=f.center+local;
            rock.rotation_axis=unit({random.signed_fraction(),random.signed_fraction(),random.signed_fraction()},{0,0,1});
            rock.angular_speed=(.025f+.11f*random.fraction())*((random.next()&1u)?1.f:-1.f);
            rock.rotation_phase=random.fraction()*2.f*pi;rock.field_id=f.id;
            rock.variant=random.next()%policy.variants;
            const AsteroidRadiusRange radii=policy.variant_radius_count?policy.variant_radii[rock.variant]:AsteroidRadiusRange{policy.minimum_radius,policy.maximum_radius};
            rock.radius=radii.minimum+(radii.maximum-radii.minimum)*radius_fraction;
            if(!in_field(local,rock.radius,f)||!asteroid_outside_protected_volumes(rock,planets,planet_count,corridors,corridor_count,policy))continue;
            bool separated=true;
            for(const auto& other:result.asteroids_){const double r=double(rock.radius)+other.radius+policy.separation;
                if(distance_segment_squared(rock.position,other.position,other.position)<=r*r){separated=false;break;}}
            if(separated)result.asteroids_.push_back(rock);
        }
        result.fields_.push_back({f.id,begin,result.asteroids_.size()-begin,f.count,f.center,std::max({f.half_extents.x,f.half_extents.y,f.half_extents.z})});
    }
    return result;
}
AsteroidLayout build_asteroid_layout(const Destination* planets,std::size_t planet_count,const AsteroidLayoutPolicy& policy){
    const auto& fields=asteroid_field_catalog();return build_asteroid_layout(planets,planet_count,fields.data(),fields.size(),policy);
}
std::size_t AsteroidSpatialIndex::CellHash::operator()(const Cell& c)const noexcept {
    std::uint64_t h=std::uint32_t(c.x)*0x9e3779b1u;h^=std::uint64_t(std::uint32_t(c.y))*0x85ebca77u;h^=std::uint64_t(std::uint32_t(c.z))*0xc2b2ae3du;
    return std::size_t(h^(h>>32));
}
AsteroidSpatialIndex::Cell AsteroidSpatialIndex::cell(Vec3 p)const noexcept{return {int(std::floor(p.x/cell_size_)),int(std::floor(p.y/cell_size_)),int(std::floor(p.z/cell_size_))};}
AsteroidSpatialIndex::AsteroidSpatialIndex(std::vector<Asteroid> asteroids,float cell_size):asteroids_(std::move(asteroids)),cell_size_(cell_size){
    if(!std::isfinite(cell_size)||cell_size<4.f||cell_size>1024.f||asteroids_.size()>65536)throw std::invalid_argument("invalid asteroid spatial index");
    for(std::size_t i=0;i<asteroids_.size();++i){
        const auto& a=asteroids_[i];if(!valid_asteroid(a)||a.radius>cell_size*4.f)throw std::invalid_argument("invalid indexed asteroid");
        const Vec3 r{a.radius,a.radius,a.radius};const Cell lo=cell(a.position-r),hi=cell(a.position+r);
        for(int x=lo.x;x<=hi.x;++x)for(int y=lo.y;y<=hi.y;++y)for(int z=lo.z;z<=hi.z;++z)cells_[{x,y,z}].push_back(i);
    }
}
void AsteroidQueryScratch::reserve(std::size_t asteroid_count){
    candidates.reserve(asteroid_count);if(seen.size()<asteroid_count)seen.resize(asteroid_count,0);
}
void AsteroidSpatialIndex::candidates(Vec3 lower,Vec3 upper,AsteroidQueryScratch& scratch,AsteroidQueryStats* stats)const {
    scratch.reserve(asteroids_.size());auto& out=scratch.candidates;out.clear();if(stats)*stats={};
    if(++scratch.stamp==0){std::fill(scratch.seen.begin(),scratch.seen.end(),0u);scratch.stamp=1;}
    const auto add=[&](const std::vector<std::size_t>& bucket){for(auto i:bucket)if(scratch.seen[i]!=scratch.stamp){scratch.seen[i]=scratch.stamp;out.push_back(i);}};
    const Cell lo=cell(lower),hi=cell(upper);
    const std::uint64_t volume=std::uint64_t(hi.x-lo.x+1)*std::uint64_t(hi.y-lo.y+1)*std::uint64_t(hi.z-lo.z+1);
    // A diagonal sweep across the whole galaxy must not enumerate millions of
    // empty cells. Scan only occupied buckets when its AABB is much larger.
    if(volume>cells_.size()*2u){
        for(const auto& entry:cells_){if(stats)++stats->occupied_cells_examined;const auto& c=entry.first;
            if(c.x>=lo.x&&c.x<=hi.x&&c.y>=lo.y&&c.y<=hi.y&&c.z>=lo.z&&c.z<=hi.z)add(entry.second);}
    }else{
        for(int x=lo.x;x<=hi.x;++x)for(int y=lo.y;y<=hi.y;++y)for(int z=lo.z;z<=hi.z;++z){
            const auto found=cells_.find({x,y,z});if(found==cells_.end())continue;if(stats)++stats->occupied_cells_examined;add(found->second);}
    }
    std::sort(out.begin(),out.end());if(stats)stats->candidate_count=out.size();
}
bool AsteroidSpatialIndex::query_sphere(Vec3 center,float radius,std::vector<std::size_t>& hits,AsteroidQueryStats* stats)const {
    AsteroidQueryScratch scratch;return query_sphere(center,radius,hits,scratch,stats);
}
bool AsteroidSpatialIndex::query_sphere(Vec3 center,float radius,std::vector<std::size_t>& hits,AsteroidQueryScratch& scratch,AsteroidQueryStats* stats)const {
    if(&hits==&scratch.candidates){hits.clear();if(stats)*stats={};return false;}
    hits.clear();if(stats)*stats={};if(!point_in_range(center)||!nonnegative(radius)||radius>query_limit/2)return false;
    const Vec3 r{radius,radius,radius};candidates(center-r,center+r,scratch,stats);
    for(auto i:scratch.candidates){const auto& a=asteroids_[i];const double sum=double(radius)+a.radius;if(distance_segment_squared(center,a.position,a.position)<=sum*sum)hits.push_back(i);}return true;
}
bool AsteroidSpatialIndex::sweep_sphere(Vec3 start,Vec3 end,float radius,std::vector<AsteroidSweepHit>& hits,AsteroidQueryStats* stats)const {
    AsteroidQueryScratch scratch;return sweep_sphere(start,end,radius,hits,scratch,stats);
}
bool AsteroidSpatialIndex::sweep_sphere(Vec3 start,Vec3 end,float radius,std::vector<AsteroidSweepHit>& hits,AsteroidQueryScratch& scratch,AsteroidQueryStats* stats)const {
    hits.clear();if(stats)*stats={};if(!point_in_range(start)||!point_in_range(end)||!nonnegative(radius)||radius>query_limit/2)return false;
    const Vec3 lower{std::min(start.x,end.x)-radius,std::min(start.y,end.y)-radius,std::min(start.z,end.z)-radius};
    const Vec3 upper{std::max(start.x,end.x)+radius,std::max(start.y,end.y)+radius,std::max(start.z,end.z)+radius};
    candidates(lower,upper,scratch,stats);
    for(auto i:scratch.candidates){AsteroidSweepHit hit;hit.index=i;if(sweep_contact(asteroids_[i],start,end,radius,hit))hits.push_back(hit);}
    std::sort(hits.begin(),hits.end(),[](const auto& a,const auto& b){return a.time<b.time||(a.time==b.time&&a.index<b.index);});return true;
}
}

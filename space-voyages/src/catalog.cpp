// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalog.hpp"
#include <cstring>

namespace spacevoyages {
const std::array<Destination,3>& prototype_catalog() noexcept {
    // Fixed coordinates, independent of campaign availability. Native cruise is
    // 24 units/s and boost30: centre-to-centre legs take about16–22s at boost.
    static constexpr std::array<Destination,3> entries{{
        {1,"Novalis",{-270.f,-120.f,0.f},30.f,DestinationKind::Planet,"planet_novalis"},
        {3,"Kerwan",{270.f,150.f,30.f},38.f,DestinationKind::Planet,"planet_kerwan"},
        {4,"Eudora",{-100.f,370.f,-35.f},28.f,DestinationKind::Planet,"planet_eudora"}
    }};
    return entries;
}
const std::array<Destination,18>& galaxy_catalog() noexcept {
    // Exact original galaxy-map XY from level_map_markers (NTSC 0x001DDE28,
    // retail ELF 0x000DEDA8). The complete 3D layout is physically expanded2.5x
    // from the initial XY scale3.5: final XY scale8.75 and authored Z*2.5.
    // This increases travel time instead of changing the ship's native speed.
    // The farthest centre-to-centre boosted leg is about72.23seconds. Explicit Z
    // depths below are authored by this mod to separate nearby bodies and
    // break up the common horizon. They are stable world positions, unrelated
    // to unlock order, and are NOT canon, measured orbital data or extracted
    // native altitude. Collision radii and Gemlik's flight remain unchanged.
    // See work/space-voyages-tools/native-galaxy-map.{json,md} for evidence.
    static constexpr auto point=[](float x,float y,float depth){return Vec3{(x-157.5f)*8.75f,(95.f-y)*8.75f,depth*2.5f};};
    static constexpr std::array<Destination,18> entries{{
        {18,"Veldin",point(35,80,-130),12,DestinationKind::Planet,"planet_veldin"},
        {1,"Novalis",point(80,95,75),11,DestinationKind::Planet,"planet_novalis"},
        {2,"Aridia",point(85,130,-95),12,DestinationKind::Planet,"planet_aridia"},
        {3,"Kerwan",point(90,65,145),14,DestinationKind::Planet,"planet_kerwan"},
        {4,"Eudora",point(108,110,-35),11,DestinationKind::Planet,"planet_eudora"},
        {5,"Rilgar",point(160,110,120),12,DestinationKind::Planet,"planet_rilgar"},
        {6,"Stazione Blarg",point(120,85,15),10,DestinationKind::Station,"station_blarg"},
        {7,"Umbris",point(150,150,-165),12,DestinationKind::Planet,"planet_umbris"},
        {8,"Batalia",point(180,130,35),13,DestinationKind::Planet,"planet_batalia"},
        {9,"Gaspar",point(200,110,-100),12,DestinationKind::Planet,"planet_gaspar"},
        {10,"Orxon",point(220,140,135),13,DestinationKind::Planet,"planet_orxon"},
        {11,"Pokitaru",point(280,100,-25),14,DestinationKind::Planet,"planet_pokitaru"},
        {12,"Hoven",point(230,80,-150),13,DestinationKind::Planet,"planet_hoven"},
        {13,"Base Gemlik",point(210,40,155),9,DestinationKind::Station,"station_gemlik"},
        {14,"Oltanis",point(200,50,70),13,DestinationKind::Planet,"planet_oltanis"},
        {15,"Quartu",point(160,40,-40),13,DestinationKind::Planet,"planet_quartu"},
        {16,"Kalebo III",point(130,50,-125),13,DestinationKind::Planet,"planet_kalebo_iii"},
        {17,"Flotta di Drek",point(45,90,-50),8,DestinationKind::Fleet,"fleet_drek"}
    }};
    return entries;
}
int celestial_body_for_level(int level) noexcept {
    if(level==18)return 0;
    return level>=0&&level<19?level:-1;
}
bool coordinates_for_body(const std::array<bool,19>& coordinates,int level) noexcept {
    const int body=celestial_body_for_level(level);if(body<0)return false;
    // The original map tests marker18's own flag. Veldin's initial story
    // coordinates0 must never grant the final landing program18 early.
    if(body==0)return coordinates[18];
    for(unsigned i=0;i<coordinates.size();++i)
        if(coordinates[i]&&celestial_body_for_level(int(i))==body)return true;
    return false;
}
const Destination* find_destination(const Destination* entries,std::size_t count,int level) noexcept {
    if(!entries)return nullptr;
    for(std::size_t i=0;i<count;++i)if(entries[i].level==level||
       celestial_body_for_level(entries[i].level)==celestial_body_for_level(level))return &entries[i];
    return nullptr;
}
bool valid_catalog(const Destination* entries,std::size_t count) noexcept {
    if(!entries||!count||count>64)return false;
    for(std::size_t i=0;i<count;++i){
        const auto& d=entries[i];
        if(d.level<0||d.level>=19||!d.name||!*d.name||std::strlen(d.name)>120||
           !finite(d.position)||!std::isfinite(d.radius)||d.radius<1.f||d.radius>100.f||
           length(d.position)+d.radius+12.f>galaxy_bounds::catalog_limit)return false;
        for(std::size_t j=0;j<i;++j){
            if(celestial_body_for_level(entries[j].level)==celestial_body_for_level(d.level))return false;
            if(length(d.position-entries[j].position)<d.radius+entries[j].radius+24.f)return false;
        }
    }
    return true;
}
}

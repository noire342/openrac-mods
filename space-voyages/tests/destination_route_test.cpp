// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_route.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {
unsigned checks{};
void require(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"destination route: %s\n",why);std::exit(1);}}
constexpr DestinationRoute home{18,DestinationVariant::VeldinGarage};
constexpr DestinationRoute final{18,DestinationVariant::Campaign};
}
int main(){
    static_assert(home.valid()&&final.valid()&&home!=final,"same native level, different route identity");
    static_assert(!DestinationRoute{}.valid()&&!DestinationRoute{0}.valid(),"tutorial0 is never a travel route");
    for(int level=-2;level<=20;++level){
        require(DestinationRoute{level}.valid()==(level>0&&level<19),"campaign native program range is explicit");
        require(DestinationRoute{level,DestinationVariant::VeldinGarage}.valid()==(level==18),"garage mount may only use its agreed native program18");
        require(!DestinationRoute{level,static_cast<DestinationVariant>(99)}.valid(),"unknown route variant is never valid");
    }
    // Compare every campaign origin/target pair against the old policy under
    // every single-coordinate flag and no/all flags. Default availability must
    // preserve campaign behavior without pretending the observer can enter home.
    for(int origin=1;origin<19;++origin)for(int flag=-2;flag<19;++flag){
        std::array<bool,19> coordinates{};if(flag==-2)coordinates.fill(true);else if(flag>=0)coordinates[unsigned(flag)]=true;
        const auto before=coordinates;
        for(int marker=0;marker<19;++marker){
            const int native=marker==0?18:marker;
            const bool allowed=native==origin||coordinates[unsigned(native)];
            const auto actual=resolve_destination_route(marker,coordinates,{origin});
            require(actual==(allowed?DestinationRoute{native}:DestinationRoute{}),"capability-off default is identical to native campaign coordinate/provenance behavior");
            require(!actual.valid()||actual.native_level!=0,"no flag or alias produces tutorial program0");
        }
        require(coordinates==before,"route resolution never invents or rewrites coordinates");
    }
    for(int origin=1;origin<19;++origin)for(bool initial:{false,true})for(bool end:{false,true})for(bool supported:{false,true})for(int marker:{0,18}){
        std::array<bool,19> coordinates{};coordinates[0]=initial;coordinates[18]=end;
        const auto actual=resolve_destination_route(marker,coordinates,{origin},{supported});
        const auto expected=end||origin==18?final:supported?home:DestinationRoute{};
        require(actual==expected,"both physical Veldin aliases select final campaign or verified home capability, never tutorial0");
    }
    for(bool final_unlocked:{false,true})for(bool supported:{false,true}){
        std::array<bool,19> coordinates{};coordinates[18]=final_unlocked;
        const auto actual=resolve_destination_route(18,coordinates,home,{supported});
        require(actual==(final_unlocked?final:supported?home:DestinationRoute{}),"home provenance is distinct from campaign18 and cannot bypass revoked entry support");
    }
    std::array<bool,19> flags{};flags[1]=true;
    require(!resolve_destination_route(18,flags,{1}).valid(),"read-only home observations without verified entry capability leave Veldin locked");
    require(resolve_destination_route(18,flags,{1},{true})==home,"host-verified entry capability enables pre-final home route without fake final coordinates");
    const auto captured=resolve_destination_route(18,flags,{1},{true});flags[18]=true;
    require(resolve_destination_route(18,flags,{1},{true})==final&&captured!=final&&captured.native_level==final.native_level,"variant identity detects a story unlock even when both native destinations are18");
    flags[18]=false;
    require(resolve_destination_route(18,flags,{1},{true})==captured,"story rollback resolves home without pretending native18 identifies the world");
    require(!resolve_destination_route(18,flags,{1},{false}).valid(),"provider availability revocation is fail-closed");
    for(int marker:{-1,19,100})require(!resolve_destination_route(marker,flags,home,{true}).valid(),"unknown marker cannot alias a real route");
    for(int marker=0;marker<19;++marker){const auto actual=resolve_destination_route(marker,flags,{0},{true});require(!actual.valid()||actual.native_level!=0,"invalid tutorial provenance cannot manufacture a tutorial route");}
    require(resolve_destination_route(1,flags,home,{true})==DestinationRoute{1}&&
        !resolve_destination_route(3,flags,home,{true}).valid(),"garage support does not grant unrelated campaign destinations");
    std::printf("destination_route: %u availability, variant identity, legacy campaign and no-tutorial checks passed\n",checks);
}

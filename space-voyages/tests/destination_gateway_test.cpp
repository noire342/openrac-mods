// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_gateway.hpp"
#include "destination_client.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
using namespace spacevoyages;
namespace {
unsigned checks{};
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"destination_gateway: %s\n",why);std::exit(1);}}
openrac_mod* const owner=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x1000));
struct Host {
    static Host* active;
    openrac_api api{};std::array<openrac_destination,257> worlds{};
    openrac_destination campaign{},mounted{},target{};
    openrac_destination_context now{};
    openrac_destination_departure departure{};
    unsigned begins{},prepares{},commits{},cancels{};
    bool lease{},ticket_live{},token_live{},substitute{},stale_target{},fail_commit{};
    std::uint64_t nonce{},ticket=701,token=801;
    Host(){
        active=this;
        campaign.size=sizeof campaign;campaign.kind=OPENRAC_DESTINATION_CAMPAIGN;
        campaign.handle=10003;campaign.registry_generation=19;campaign.native_level=3;
        for(std::size_t i=0;i<worlds.size();++i){auto& d=worlds[i];d.size=sizeof d;d.kind=OPENRAC_DESTINATION_ADDED;
            d.handle=0x100000000ull+i;d.registry_generation=19;d.native_level=int(i%19);
            std::snprintf(d.manifest_id,sizeof d.manifest_id,"custom-world-%u",unsigned(i));
            std::snprintf(d.name,sizeof d.name,"Custom world %u",unsigned(i));}
        now.size=sizeof now;now.native_generation=7;now.origin=campaign;
        api.size=sizeof api;api.api_major=1;api.api_minor=14;
        api.destination_lookup=+[](const char* id,openrac_destination* out){auto& h=*active;
            for(std::size_t i=0;i<h.worlds.size();++i)if(!std::strcmp(id,h.worlds[i].manifest_id)){
                *out=h.worlds[h.substitute?(i+1)%h.worlds.size():i];if(h.stale_target&&i==1)++out->registry_generation;return OPENRAC_HOOK_OK;}
            return OPENRAC_HOOK_UNKNOWN;};
        api.destination_campaign=+[](int level,openrac_destination* out){if(level!=3)return OPENRAC_HOOK_UNKNOWN;*out=active->campaign;return OPENRAC_HOOK_OK;};
        api.destination_current=+[](openrac_destination_context* out){*out=active->now;return OPENRAC_HOOK_OK;};
        api.destination_begin_departure=+[](openrac_mod* who,const openrac_destination_departure* in,std::uint64_t* out){auto& h=*active;++h.begins;
            if(who!=owner||h.lease||h.now.handoff_id||!in->nonce||in->nonce<=h.nonce||in->origin_handle!=h.now.origin.handle||in->native_generation!=h.now.native_generation)return OPENRAC_HOOK_UNKNOWN;
            h.nonce=in->nonce;h.departure=*in;h.ticket_live=true;*out=h.ticket;return OPENRAC_HOOK_OK;};
        api.destination_prepare=+[](openrac_mod* who,const openrac_destination_request* in,std::uint64_t* out){auto& h=*active;++h.prepares;
            if(who!=owner||!h.lease||h.token_live||in->handoff_id!=h.now.handoff_id||in->origin_handle!=h.departure.origin_handle||in->nonce!=h.nonce)return OPENRAC_HOOK_UNKNOWN;
            for(const auto& d:h.worlds)if(d.handle==in->target_handle){h.target=d;h.token_live=true;*out=h.token;return OPENRAC_HOOK_OK;}
            return OPENRAC_HOOK_UNKNOWN;};
        api.destination_commit=+[](openrac_mod* who,std::uint64_t value){auto& h=*active;++h.commits;
            if(who!=owner||h.lease||!h.token_live||value!=h.token||h.fail_commit)return OPENRAC_HOOK_UNKNOWN;
            h.mounted=h.target;h.token_live=false;return OPENRAC_HOOK_OK;};
        api.destination_cancel=+[](openrac_mod* who,std::uint64_t value){auto& h=*active;++h.cancels;
            if(who!=owner)return OPENRAC_HOOK_UNKNOWN;
            if(value==h.token&&h.token_live){h.token_live=false;return OPENRAC_HOOK_OK;}
            if(value==h.ticket&&h.ticket_live){h.ticket_live=false;return OPENRAC_HOOK_OK;}
            return OPENRAC_HOOK_UNKNOWN;};
    }
    RegisteredDestination identity(std::size_t i)const{return RegisteredDestination::added(worlds[i].manifest_id,worlds[i].native_level);}
    void enter(){lease=true;ticket_live=false;now.handoff_id=0x400000001ull;}
};
Host* Host::active{};
}
int main(){
    static_assert(std::is_trivially_copyable<RegisteredDestination>::value,"route identities do not allocate or borrow plugin strings");
    require(!RegisteredDestination::campaign(0).valid(),"native tutorial is not a campaign travel route");
    require(RegisteredDestination::added("custom-world",0).valid(),"a registered custom world may use template0 without becoming campaign tutorial0");
    for(const char* id:{"","../world","my/world","bad world",":world"})require(!RegisteredDestination::added(id,18).valid(),"unsafe or empty stable IDs rejected");
    std::array<char,129> id{};id.fill('a');id[127]=0;require(RegisteredDestination::added(id.data(),18).valid(),"127-byte registered ID supported");
    id[127]='a';id[128]=0;require(!RegisteredDestination::added(id.data(),18).valid(),"oversized ID never truncates into a different identity");
    for(std::size_t i=0;i<257;++i){
        Host h;DestinationGateway gateway(&h.api,owner);const auto from=h.identity(i),to=h.identity((i+19)%257);
        h.now.origin=h.worlds[i];RegisteredDestination current;
        require(gateway.current(current)&&current==from,"every registered world has its own stable current identity");
        require(from!=to,"separate custom IDs never alias even when native templates are identical");
        require(gateway.begin_departure(from),"departure from an arbitrary registered custom world");
        const auto nonce=h.nonce;h.enter();require(gateway.prepare(to)&&gateway.prepared_route()==to&&h.nonce==nonce,"target preparation retains exact custom identity and departure nonce");
        h.lease=false;require(gateway.commit()&&h.mounted.handle==h.worlds[(i+19)%257].handle,"commit chooses the exact registered target, not its native template or a fixed Garage");
        require(!gateway.has_departure()&&!gateway.prepared_route().valid()&&!gateway.commit(),"one handoff cannot be committed twice");
    }
    {
        Host h;DestinationGateway gateway(&h.api,owner);h.now.origin=h.worlds[0];RegisteredDestination current;
        require(gateway.current(current)&&current==h.identity(0),"generic boundary understands a non-Garage custom origin");
        DestinationClient legacy(&h.api,owner);DestinationRoute unsupported;
        require(!legacy.current(unsupported)&&!unsupported.valid(),"current galaxy adapter does not pretend an unconfigured custom world is Veldin");
        h.substitute=true;openrac_destination out{};require(!gateway.lookup(h.identity(0),out)&&!out.handle,"host returning another ID fails without aliasing a world");
    }
    {
        Host h;DestinationGateway gateway(&h.api,owner);require(gateway.begin_departure(RegisteredDestination::campaign(3)),"campaign origin still supported");
        h.enter();h.stale_target=true;require(!gateway.prepare(h.identity(1))&&!h.prepares,"custom target from another registry generation cannot prepare");
        h.stale_target=false;require(gateway.prepare(h.identity(1)),"fresh custom target prepares");
        h.lease=false;h.fail_commit=true;require(!gateway.commit()&&!h.mounted.handle&&!gateway.has_departure(),"failed custom commit cancels without mounting or replaying");
    }
    std::printf("destination_gateway: %u generic identity, template sharing, registration and transactional checks passed\n",checks);
}

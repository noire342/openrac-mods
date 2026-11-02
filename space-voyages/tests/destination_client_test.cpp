// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_client.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace spacevoyages;
namespace {
unsigned checks{};
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"destination_client: %s\n",why);std::exit(1);}}
constexpr DestinationRoute original3{3,DestinationVariant::Campaign},night{18,DestinationVariant::Campaign},home{18,DestinationVariant::VeldinGarage};
openrac_mod* owner=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x1000));
openrac_mod* foreign=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x2000));
openrac_destination destination(int level,bool added=false){
    openrac_destination d{};d.size=sizeof d;d.kind=added?OPENRAC_DESTINATION_ADDED:OPENRAC_DESTINATION_CAMPAIGN;
    d.handle=added?18:unsigned(100+level); //18 is legal as an opaque handle, never a menu row.
    d.registry_generation=17;d.native_level=level;
    std::snprintf(d.manifest_id,sizeof d.manifest_id,"%s",added?"veldin-casa":"");std::snprintf(d.name,sizeof d.name,"%s",added?"Veldin Casa":"Campaign");return d;
}
struct Fake {
    static Fake* active;
    openrac_api api{};openrac_destination_context now{};
    std::array<openrac_destination,19> campaign{};openrac_destination garage=destination(18,true);
    unsigned lookups{},campaigns{},currents{},begins{},prepares{},commits{};
    std::vector<std::uint64_t> cancelled;
    openrac_destination_departure departure{};openrac_destination_request request{};
    openrac_mod* accepted_owner=owner;
    std::uint64_t ticket=501,token=701,last_nonce{};
    bool ticket_live{},token_live{},lease_open{},committed{},available=true,current_ok=true;
    bool zero_ticket{},zero_token{},fail_begin{},fail_prepare{},fail_commit{},fail_cancel{};
    DestinationClient* recursive{};bool recursive_result=true;
    Fake(){
        active=this;for(int i=1;i<19;++i)campaign[unsigned(i)]=destination(i);
        now.size=sizeof now;now.native_generation=9;now.origin=campaign[3];
        api.size=sizeof api;api.api_major=1;api.api_minor=14;
        api.destination_lookup=+[](const char* id,openrac_destination* out){auto& f=*active;++f.lookups;
            require(out&&out->size==sizeof *out,"lookup receives descriptor size");
            if(!f.available||std::strcmp(id,"veldin-casa"))return OPENRAC_HOOK_UNKNOWN;*out=f.garage;return OPENRAC_HOOK_OK;};
        api.destination_campaign=+[](int level,openrac_destination* out){auto& f=*active;++f.campaigns;
            require(out&&out->size==sizeof *out,"campaign receives descriptor size");
            if(!f.available||level<1||level>18)return OPENRAC_HOOK_UNKNOWN;*out=f.campaign[unsigned(level)];return OPENRAC_HOOK_OK;};
        api.destination_current=+[](openrac_destination_context* out){auto& f=*active;++f.currents;
            require(out&&out->size==sizeof *out&&out->origin.size==sizeof out->origin,"current receives both output sizes");
            if(!f.current_ok)return OPENRAC_HOOK_UNKNOWN;*out=f.now;return OPENRAC_HOOK_OK;};
        api.destination_begin_departure=+[](openrac_mod* who,const openrac_destination_departure* r,std::uint64_t* out){auto& f=*active;++f.begins;
            require(r&&r->size==sizeof *r&&out&&!*out,"begin receives sized request and zero output");
            if(who!=f.accepted_owner||f.fail_begin||!r->nonce||r->nonce<=f.last_nonce||r->native_generation!=f.now.native_generation||
               r->origin_handle!=f.now.origin.handle||r->registry_generation!=f.now.origin.registry_generation)return OPENRAC_HOOK_UNKNOWN;
            f.departure=*r;f.last_nonce=r->nonce;f.ticket_live=true;*out=f.zero_ticket?0:f.ticket;return OPENRAC_HOOK_OK;};
        api.destination_prepare=+[](openrac_mod* who,const openrac_destination_request* r,std::uint64_t* out){auto& f=*active;++f.prepares;
            require(r&&r->size==sizeof *r&&out&&!*out,"prepare receives sized request and zero output");
            if(who!=f.accepted_owner||f.fail_prepare||!f.lease_open||!f.now.handoff_id||r->handoff_id!=f.now.handoff_id||
               r->native_generation!=f.departure.native_generation||r->registry_generation!=f.departure.registry_generation||
               r->origin_handle!=f.departure.origin_handle||r->nonce!=f.departure.nonce)return OPENRAC_HOOK_UNKNOWN;
            f.request=*r;f.token_live=true;*out=f.zero_token?0:f.token;return OPENRAC_HOOK_OK;};
        api.destination_commit=+[](openrac_mod* who,std::uint64_t value){auto& f=*active;++f.commits;
            if(f.recursive)f.recursive_result=f.recursive->commit();
            if(who!=f.accepted_owner||f.fail_commit||f.lease_open||!f.token_live||value!=f.token)return OPENRAC_HOOK_UNKNOWN;
            f.token_live=false;f.committed=true;return OPENRAC_HOOK_OK;};
        api.destination_cancel=+[](openrac_mod* who,std::uint64_t value){auto& f=*active;f.cancelled.push_back(value);
            if(who!=f.accepted_owner||f.fail_cancel)return OPENRAC_HOOK_UNKNOWN;
            if(value==f.token&&f.token_live){f.token_live=false;return OPENRAC_HOOK_OK;}
            if(value==f.ticket&&f.ticket_live){f.ticket_live=false;return OPENRAC_HOOK_OK;}
            return OPENRAC_HOOK_UNKNOWN;};
    }
    void handoff(std::uint64_t id=801){now.handoff_id=id;ticket_live=false;lease_open=true;}
};
Fake* Fake::active{};
void support_and_routes(){
    Fake f;DestinationClient c(&f.api,owner);require(c.supported(),"complete SDK14 supported");
    DestinationRoute r;std::uint32_t generation=0;
    require(c.current(r,&generation)&&r==original3&&generation==9,"current route and native generation read without departure");
    require(!c.has_departure()&&!c.origin_route().valid()&&!c.prepared_route().valid(),"read-only current acquires no owner handles");
    openrac_destination d{};require(c.lookup(home,d)&&d.kind==OPENRAC_DESTINATION_ADDED&&d.handle==18,"garage uses exact manifest identity and allows opaque handle18");
    require(c.lookup(night,d)&&d.handle==118&&d.kind==OPENRAC_DESTINATION_CAMPAIGN,"campaign18 does not alias garage18");
    const auto calls=f.lookups+f.campaigns;
    for(auto invalid:{DestinationRoute{},DestinationRoute{0,DestinationVariant::Campaign},DestinationRoute{18,static_cast<DestinationVariant>(99)},DestinationRoute{3,DestinationVariant::VeldinGarage}})
        require(!c.lookup(invalid,d)&&!d.handle,"invalid and native tutorial0 routes rejected");
    require(f.lookups+f.campaigns==calls,"invalid routes never call host lookup");
    f.now.origin=f.garage;require(c.current(r)&&r==home,"current added18 preserved as garage");
    require(!c.begin_departure(night)&&f.begins==0,"campaign18 expected origin rejects same-program garage18");
    require(c.begin_departure(home)&&c.origin_route()==home,"expected garage origin begins with zero-success status");
    require(c.context().origin.handle==18&&!c.context().handoff_id&&c.has_departure(),"captured origin context retained");
    require(c.cancel()&&!c.has_departure()&&!c.origin_route().valid(),"ticket cancellation maps HOOK_OK0 to true");
    f.available=false;require(!c.lookup(home,d),"unavailable registry entry rejected");
    generation=42;r=home;require(!c.current(r,&generation)&&!r.valid()&&!generation,"failed current clears both caller outputs");
}
void malformed_descriptors(){
    for(unsigned which=0;which<9;++which){Fake f;DestinationClient c(&f.api,owner);auto& d=f.garage;
        switch(which){case 0:d.size=sizeof d-1;break;case 1:d.handle=0;break;case 2:d.registry_generation=0;break;
        case 3:d.reserved=1;break;case 4:std::memset(d.manifest_id,'x',sizeof d.manifest_id);break;
        case 5:std::memset(d.name,'x',sizeof d.name);break;case 6:d.kind=OPENRAC_DESTINATION_CAMPAIGN;break;
        case 7:d.native_level=0;break;default:std::snprintf(d.manifest_id,sizeof d.manifest_id,"%s","veldin-casa-other");}
        openrac_destination out{};require(!c.lookup(home,out)&&!out.handle,"malformed garage descriptor rejected");
    }
    {Fake f;DestinationClient c(&f.api,owner);f.campaign[18].manifest_id[0]='x';openrac_destination out{};
        require(!c.lookup(night,out),"campaign descriptor cannot carry an added ID");}
    for(unsigned which=0;which<4;++which){Fake f;DestinationClient c(&f.api,owner);
        switch(which){case 0:f.now.size=sizeof f.now-1;break;case 1:f.now.native_generation=0;break;
        case 2:f.now.origin.registry_generation++;break;default:f.now.origin.handle++;}
        DestinationRoute r=home;require(!c.current(r)&&!r.valid(),"malformed or registry-stale current rejected");
    }
}
void support_prefix(){
    Fake f;openrac_destination d{};DestinationRoute r;
    require(!DestinationClient(nullptr,owner).supported()&&!DestinationClient(&f.api,nullptr).supported(),"null API/owner unavailable");
    for(unsigned which=0;which<10;++which){auto api=f.api;
        switch(which){case 0:api.api_minor=13;break;case 1:api.size=offsetof(openrac_api,destination_cancel);break;
        case 2:api.api_major=2;break;case 3:api.destination_lookup=nullptr;break;case 4:api.destination_campaign=nullptr;break;
        case 5:api.destination_current=nullptr;break;case 6:api.destination_begin_departure=nullptr;break;
        case 7:api.destination_prepare=nullptr;break;case 8:api.destination_commit=nullptr;break;default:api.destination_cancel=nullptr;}
        DestinationClient c(&api,owner);require(!c.supported()&&!c.current(r)&&!c.lookup(home,d)&&!c.begin_departure(original3)&&!c.prepare(home)&&!c.commit()&&c.cancel(),"old/truncated/null-callback API fails without acquiring state");
    }
    // Put the physical SDK13 prefix immediately before an inaccessible page.
    // Any speculative read of one of the appended seven pointers would fault.
#ifdef _WIN32
    SYSTEM_INFO info{};GetSystemInfo(&info);const auto page=std::size_t(info.dwPageSize);
    auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(memory!=nullptr,"guard allocation");DWORD old{};require(VirtualProtect(memory+page,page,PAGE_NOACCESS,&old)!=0,"guard protection");
#else
    const auto page=std::size_t(sysconf(_SC_PAGESIZE));
    auto* memory=static_cast<unsigned char*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    require(memory!=MAP_FAILED&&mprotect(memory+page,page,PROT_NONE)==0,"guard allocation/protection");
#endif
    constexpr auto prefix=offsetof(openrac_api,destination_lookup);require(prefix<page,"SDK13 prefix fits page");
    auto* api=reinterpret_cast<openrac_api*>(memory+page-prefix);std::memcpy(api,&f.api,prefix);api->size=prefix;api->api_minor=13;
    {DestinationClient c(api,owner);require(!c.supported()&&!c.current(r)&&!c.begin_departure(original3)&&c.cancel(),"physical13prefix checked without reading appended page");}
    api->api_minor=14;
    {DestinationClient c(api,owner);require(!c.supported()&&!c.current(r)&&c.cancel(),"minor14 cannot bypass physically short SDK13 size guard");}
#ifdef _WIN32
    VirtualFree(memory,0,MEM_RELEASE);
#else
    munmap(memory,page*2);
#endif
}
void departure_prepare_commit(){
    Fake f;DestinationClient c(&f.api,owner);require(c.begin_departure(original3),"begin captures own departure");
    const auto nonce=f.departure.nonce;require(nonce!=0,"departure nonce nonzero");
    require(!c.prepare(home)&&f.prepares==0,"prepare requires native post-take-off handoff");
    f.handoff();require(c.prepare(home)&&c.prepared_route()==home,"added target prepared inside own handoff");
    require(f.request.nonce==nonce&&f.request.handoff_id==801&&f.request.native_generation==f.departure.native_generation&&f.request.registry_generation==f.departure.registry_generation,"prepare reuses exact nonce and epochs");
    require(f.request.origin_handle==103&&f.request.target_handle==18,"request uses opaque origin/target handles");
    require(!c.prepare(home)&&f.prepares==1,"duplicate prepare does not create another token");
    f.lease_open=false;f.recursive=&c;require(c.commit()&&f.committed,"commit maps success0 to true after lease close");
    require(!f.recursive_result&&f.commits==1,"token consumed before callback, including reentrant commit");
    require(!c.has_departure()&&!c.prepared_route().valid()&&!c.commit()&&c.cancel(),"successful commit clears local state and is not repeated");
    require(f.cancelled.empty(),"successful commit never cancels consumed host handles");
}
void cancellation_and_failures(){
    {Fake f;DestinationClient c(&f.api,owner);require(c.begin_departure(original3),"first begin");const auto first=f.departure.nonce;
        require(c.begin_departure(original3)&&f.departure.nonce>first,"new begin cancels previous ticket and takes fresh nonce");
        require(f.cancelled.size()==1&&f.cancelled[0]==f.ticket,"replaced departure cancelled only its own ticket");}
    {Fake f;{DestinationClient c(&f.api,owner);require(c.begin_departure(original3),"destructor begin");f.handoff();require(c.prepare(night),"destructor prepare");}
        require(f.cancelled==std::vector<std::uint64_t>{f.token,f.ticket}&&!f.token_live,"destructor cancels token before consumed ticket");}
    {Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.handoff();c.prepare(home);
        require(c.cancel()&&f.cancelled==std::vector<std::uint64_t>{f.token,f.ticket},"token success0 makes cancel true even with consumed ticketUNKNOWN");
        const auto count=f.cancelled.size();require(c.cancel()&&f.cancelled.size()==count,"second cancel is local idempotent success");}
    {Fake f;DestinationClient c(&f.api,owner);require(c.begin_departure(original3),"approach departure begins once");
        const auto nonce=f.departure.nonce;f.handoff();require(c.prepare(home),"initial approach has prepared target");
        const auto context=c.context();require(c.discard_prepared(),"cancelled approach releases token with success0");
        require(f.cancelled==std::vector<std::uint64_t>{f.token}&&!c.prepared_route().valid(),"discard releases only target token");
        require(c.has_departure()&&c.origin_route()==original3&&c.context().handoff_id==context.handoff_id&&
            c.context().origin.handle==context.origin.handle&&c.context().native_generation==context.native_generation,"discard keeps captured origin and same departure/handoff");
        require(c.discard_prepared()&&f.cancelled.size()==1,"discard without target idempotent");
        ++f.token;require(c.prepare(night)&&f.begins==1&&f.prepares==2&&f.request.nonce==nonce&&c.prepared_route()==night,"new target prepared in same flight using original nonce, no new begin");
        f.lease_open=false;require(c.commit()&&f.committed,"replacement approach commits its own fresh token");}
    {Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.handoff();c.prepare(home);c.discard_prepared();
        ++f.now.handoff_id;const auto calls=f.prepares;require(!c.prepare(night)&&f.prepares==calls,"discard cannot carry nonce into a different handoff");}
    {Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.handoff();c.prepare(home);f.fail_cancel=true;
        require(!c.discard_prepared()&&c.has_departure()&&!c.prepared_route().valid(),"stale target discard clears only selection while preserving departure context");}
    for(unsigned which=0;which<6;++which){Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.handoff();
        switch(which){case 0:f.now.native_generation++;break;case 1:f.now.origin=f.garage;break;
        case 2:f.now.origin.handle++;break;case 3:f.campaign[3].registry_generation++;f.now.origin=f.campaign[3];break;
        case 4:f.garage.registry_generation++;break;default:f.accepted_owner=foreign;}
        require(!c.prepare(home),"stale origin/nativeepoch/registry/targetepoch or owner cannot prepare");
        require(!c.prepared_route().valid()&&!c.commit(),"failed prepare creates no consumable token");}
    for(unsigned which=0;which<5;++which){Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.handoff();c.prepare(home);
        switch(which){case 0:f.now.handoff_id++;break;case 1:f.now.handoff_id=0;break;case 2:f.now.native_generation++;break;
        case 3:f.fail_commit=true;break;default:break;} // Last case keeps lease open: host refuses early commit.
        require(!c.commit()&&!f.committed&&!c.has_departure()&&!c.prepared_route().valid(),"stale/failed/lease-open commit fails closed and clears owned state");
        const auto calls=f.commits;require(!c.commit()&&f.commits==calls,"failed token cannot be offered again");}
    for(unsigned which=0;which<4;++which){Fake f;DestinationClient c(&f.api,owner);
        if(which==0)f.fail_begin=true;else if(which==1)f.zero_ticket=true;
        if(which<2){require(!c.begin_departure(original3)&&!c.has_departure(),"begin status error or zero output rejected");continue;}
        c.begin_departure(original3);f.handoff();if(which==2)f.fail_prepare=true;else f.zero_token=true;
        require(!c.prepare(home)&&!c.prepared_route().valid(),"prepare status error or zero token rejected");}
    {Fake f;DestinationClient c(&f.api,owner);f.now.handoff_id=801;require(!c.begin_departure(original3)&&f.begins==0,"begin refuses active handoff");}
    {Fake f;DestinationClient c(&f.api,owner);c.begin_departure(original3);f.fail_cancel=true;
        require(!c.cancel()&&!c.has_departure()&&c.cancel(),"retired/owner-invalid cancel clears state and remains idempotent");}
}
}
static_assert(!std::is_copy_constructible_v<DestinationClient>&&!std::is_move_constructible_v<DestinationClient>);
int main(){support_and_routes();malformed_descriptors();support_prefix();departure_prepare_commit();cancellation_and_failures();
    std::printf("destination client: %u checks; SDK14/physical13prefix/route identity/nonce/epochs/owner/zero-success/one-shot commit/cleanup; no game or guest memory\n",checks);}

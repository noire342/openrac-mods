// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_gateway.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace spacevoyages {
namespace {
RegisteredDestination route(const openrac_destination& d)noexcept {
    if(d.size<sizeof d||!d.handle||!d.registry_generation||d.reserved||
       !std::memchr(d.manifest_id,0,sizeof d.manifest_id)||!std::memchr(d.name,0,sizeof d.name))return {};
    if(d.kind==OPENRAC_DESTINATION_CAMPAIGN&&d.manifest_id[0]==0){
        const auto result=RegisteredDestination::campaign(d.native_level);
        return result.valid()?result:RegisteredDestination{};
    }
    if(d.kind==OPENRAC_DESTINATION_ADDED){
        const auto result=RegisteredDestination::added(d.manifest_id,d.native_level);
        return result.valid()?result:RegisteredDestination{};
    }
    return {};
}
bool same(const openrac_destination& a,const openrac_destination& b)noexcept {
    return a.handle==b.handle&&a.registry_generation==b.registry_generation&&a.native_level==b.native_level&&
           a.kind==b.kind&&std::strcmp(a.manifest_id,b.manifest_id)==0;
}
std::uint64_t next_nonce()noexcept {
    // All SDK callbacks run on the game thread. Never recycle a nonce on wrap.
    static std::uint64_t serial{};
    return serial==std::numeric_limits<std::uint64_t>::max()?0:++serial;
}
}
DestinationGateway::~DestinationGateway()noexcept{cancel();}
bool DestinationGateway::supported()const noexcept {
    constexpr auto prefix=offsetof(openrac_api,api_minor)+sizeof(openrac_api::api_minor);
    constexpr auto complete=offsetof(openrac_api,destination_cancel)+sizeof(openrac_api::destination_cancel);
    return api_&&mod_&&api_->size>=prefix&&api_->api_major==1&&api_->api_minor>=14&&api_->size>=complete&&
        api_->destination_lookup&&api_->destination_campaign&&api_->destination_current&&api_->destination_begin_departure&&
        api_->destination_prepare&&api_->destination_commit&&api_->destination_cancel;
}
bool DestinationGateway::lookup(RegisteredDestination expected,openrac_destination& out)const noexcept {
    out={};if(!supported()||!expected.valid())return false;
    openrac_destination copy{};copy.size=sizeof copy;
    const auto result=expected.kind==RegisteredDestinationKind::Added?
        api_->destination_lookup(expected.manifest_id.data(),&copy):api_->destination_campaign(expected.native_level,&copy);
    if(result!=OPENRAC_HOOK_OK||route(copy)!=expected)return false;
    out=copy;return true;
}
bool DestinationGateway::read_context(openrac_destination_context& out,RegisteredDestination& identity)const noexcept {
    out={};identity={};if(!supported())return false;
    openrac_destination_context copy{};copy.size=sizeof copy;copy.origin.size=sizeof copy.origin;
    if(api_->destination_current(&copy)!=OPENRAC_HOOK_OK||copy.size<sizeof copy||!copy.native_generation)return false;
    const auto candidate=route(copy.origin);openrac_destination registered{};
    if(!candidate.valid()||!lookup(candidate,registered)||!same(copy.origin,registered))return false;
    out=copy;identity=candidate;return true;
}
bool DestinationGateway::current(RegisteredDestination& out,std::uint32_t* generation)const noexcept {
    out={};if(generation)*generation=0;
    openrac_destination_context now{};RegisteredDestination identity{};
    if(!read_context(now,identity))return false;
    out=identity;if(generation)*generation=now.native_generation;return true;
}
bool DestinationGateway::same_departure(const openrac_destination_context& now,RegisteredDestination identity)const noexcept {
    return has_departure()&&identity==origin_&&now.native_generation==context_.native_generation&&same(now.origin,context_.origin);
}
bool DestinationGateway::begin_departure(RegisteredDestination expected)noexcept {
    cancel();if(!supported()||!expected.valid())return false;
    const auto rejected=[&](const char* reason){if(api_->log)api_->log(mod_,OPENRAC_LOG_INFO,reason);return false;};
    openrac_destination_context now{};RegisteredDestination identity{};
    if(!read_context(now,identity))return rejected("Destination departure rejected: current logical context unavailable");
    if(now.handoff_id)return rejected("Destination departure rejected: native handoff already active");
    if(identity!=expected)return rejected("Destination departure rejected: logical origin changed");
    const auto nonce=next_nonce();if(!nonce)return false;
    const openrac_destination_departure request{sizeof(openrac_destination_departure),now.native_generation,
        now.origin.registry_generation,now.origin.handle,nonce};
    std::uint64_t ticket{};
    const int result=api_->destination_begin_departure(mod_,&request,&ticket);
    if(result!=OPENRAC_HOOK_OK||!ticket){
        char message[256]{};std::snprintf(message,sizeof message,
            "Destination departure rejected by host: result=%d ticket=%llu level=%d native-generation=%u registry=%llu origin=%llu nonce=%llu",
            result,static_cast<unsigned long long>(ticket),identity.native_level,now.native_generation,
            static_cast<unsigned long long>(request.registry_generation),static_cast<unsigned long long>(request.origin_handle),
            static_cast<unsigned long long>(nonce));
        return rejected(message);
    }
    context_=now;origin_=identity;ticket_=ticket;nonce_=nonce;return true;
}
bool DestinationGateway::prepare(RegisteredDestination target)noexcept {
    if(!supported()||!has_departure()||token_||!target.valid())return false;
    openrac_destination_context now{};RegisteredDestination identity{};openrac_destination destination{};
    if(!read_context(now,identity)||!now.handoff_id||!same_departure(now,identity)||
       (context_.handoff_id&&now.handoff_id!=context_.handoff_id)||!lookup(target,destination)||
       destination.registry_generation!=context_.origin.registry_generation)return false;
    const openrac_destination_request request{sizeof(openrac_destination_request),now.native_generation,
        now.origin.registry_generation,now.origin.handle,destination.handle,now.handoff_id,nonce_};
    std::uint64_t token{};
    if(api_->destination_prepare(mod_,&request,&token)!=OPENRAC_HOOK_OK||!token)return false;
    context_=now;prepared_=target;token_=token;return true;
}
bool DestinationGateway::discard_prepared()noexcept {
    const auto token=std::exchange(token_,0);prepared_={};
    return !token||(supported()&&api_->destination_cancel(mod_,token)==OPENRAC_HOOK_OK);
}
bool DestinationGateway::commit()noexcept {
    if(!token_)return false;
    openrac_destination_context now{};RegisteredDestination identity{};
    if(!read_context(now,identity)||!same_departure(now,identity)||!now.handoff_id||now.handoff_id!=context_.handoff_id){cancel();return false;}
    const auto token=std::exchange(token_,0); // Never submit it twice, even on host failure.
    if(api_->destination_commit(mod_,token)==OPENRAC_HOOK_OK){clear();return true;}
    api_->destination_cancel(mod_,token);cancel();return false;
}
bool DestinationGateway::cancel()noexcept {
    const auto token=std::exchange(token_,0),ticket=std::exchange(ticket_,0);
    const bool pending=token||ticket;bool ok=!pending;
    if(pending&&supported()){
        if(token)ok=api_->destination_cancel(mod_,token)==OPENRAC_HOOK_OK||ok;
        if(ticket)ok=api_->destination_cancel(mod_,ticket)==OPENRAC_HOOK_OK||ok;
    }
    clear();return ok;
}
void DestinationGateway::clear()noexcept{context_={};origin_={};prepared_={};ticket_=token_=nonce_=0;}
}

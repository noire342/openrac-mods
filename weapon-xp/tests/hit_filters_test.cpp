// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hit_filters.hpp"
#include "check.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>

using namespace weaponxp;
static_assert(sizeof(weapon_xp_hit_context_v4)==64);
static_assert(offsetof(weapon_xp_hit_context_v4,tick)==40);
static_assert(offsetof(weapon_xp_api_v4,v3)==0);
struct Client {
    HitFilters* hub{};unsigned token{},calls{};bool claim=false;
    int nested_register{},self_unregister{};bool nested_claim=false;
};
int classify(void* user,const weapon_xp_hit_context_v4* context){
    auto& c=*static_cast<Client*>(user);++c.calls;
    CHECK(context&&context->size==sizeof(*context));
    return c.claim?WEAPON_XP_HIT_EXTERNAL:WEAPON_XP_HIT_NATIVE;
}
int modify(void* user,const weapon_xp_hit_context_v4* context){
    auto& c=*static_cast<Client*>(user);++c.calls;unsigned token=123;
    c.nested_register=c.hub->subscribe(classify,&c,&token);
    CHECK(token==123);
    c.nested_claim=c.hub->external(*context);
    c.self_unregister=c.hub->unsubscribe(c.token);
    return WEAPON_XP_HIT_EXTERNAL;
}
int throws(void*,const weapon_xp_hit_context_v4*){throw std::runtime_error("consumer exception");}
int main(){
    weapon_xp_hit_context_v4 context{};context.size=sizeof(context);
    {
        HitFilters hub;Client first{&hub},second{&hub};
        CHECK(hub.subscribe(nullptr,&first,&first.token)==WEAPON_XP_INVALID_ARGUMENT);
        CHECK(hub.subscribe(classify,&first,nullptr)==WEAPON_XP_INVALID_ARGUMENT);
        CHECK(hub.subscribe(classify,&first,&first.token)==WEAPON_XP_OK);
        CHECK(hub.subscribe(classify,&first,&first.token)==WEAPON_XP_INVALID_ARGUMENT);
        CHECK(hub.subscribe(classify,&second,&second.token)==WEAPON_XP_OK);
        CHECK(!hub.external(context)&&first.calls==1&&second.calls==1);
        first.claim=true;CHECK(hub.external(context)&&first.calls==2&&second.calls==1);
        CHECK(!hub.dispatching());
        const auto stale=first.token;CHECK(hub.unsubscribe(stale)==WEAPON_XP_OK);
        CHECK(hub.subscribe(classify,&first,&first.token)==WEAPON_XP_OK&&first.token!=stale);
        CHECK(hub.unsubscribe(stale)==WEAPON_XP_NOT_FOUND);
    }
    {
        HitFilters hub;Client c{&hub};
        CHECK(hub.subscribe(modify,&c,&c.token)==WEAPON_XP_OK);
        CHECK(hub.external(context)&&c.calls==1&&c.nested_register==WEAPON_XP_BUSY);
        CHECK(c.nested_claim&&c.self_unregister==WEAPON_XP_OK&&!hub.dispatching());
        CHECK(!hub.external(context)&&c.calls==1);
    }
    {
        HitFilters hub;std::array<Client,17> clients{};
        for(unsigned i=0;i<16;++i)CHECK(hub.subscribe(classify,&clients[i],&clients[i].token)==WEAPON_XP_OK);
        CHECK(hub.subscribe(classify,&clients[16],&clients[16].token)==WEAPON_XP_FULL);
        CHECK(hub.unsubscribe(0)==WEAPON_XP_INVALID_ARGUMENT);
        CHECK(hub.unsubscribe(clients[7].token)==WEAPON_XP_OK);
        CHECK(hub.subscribe(classify,&clients[16],&clients[16].token)==WEAPON_XP_OK);
    }
    {
        HitFilters hub;unsigned token{};
        CHECK(hub.subscribe(throws,nullptr,&token)==WEAPON_XP_OK);
        CHECK(hub.external(context)&&!hub.dispatching());
        CHECK(!hub.external(context));
        CHECK(hub.unsubscribe(token)==WEAPON_XP_NOT_FOUND);
    }
    std::puts("hit filters: ownership, capacity, tokens, self-unsubscribe and reentrancy: ok");
}

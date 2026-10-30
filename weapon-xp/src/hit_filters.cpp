// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "hit_filters.hpp"

namespace weaponxp {
int HitFilters::subscribe(weapon_xp_hit_filter_v4 filter,void* user,std::uint32_t* token){
    if(!filter||!token)return WEAPON_XP_INVALID_ARGUMENT;
    if(dispatching_)return WEAPON_XP_BUSY;
    for(const auto& entry:entries_)if(entry.filter==filter&&entry.user==user)return WEAPON_XP_INVALID_ARGUMENT;
    for(auto& entry:entries_)if(!entry.filter){
        if(!next_)return WEAPON_XP_FULL;
        entry={filter,user,next_++};*token=entry.token;return WEAPON_XP_OK;
    }
    return WEAPON_XP_FULL;
}
int HitFilters::unsubscribe(std::uint32_t token){
    if(!token)return WEAPON_XP_INVALID_ARGUMENT;
    for(auto& entry:entries_)if(entry.token==token){entry={};return WEAPON_XP_OK;}
    return WEAPON_XP_NOT_FOUND;
}
bool HitFilters::external(const weapon_xp_hit_context_v4& context)noexcept{
    // A callback must not enter native combat recursively. If it does, prevent
    // that nested packet from falling through to an unrelated native weapon.
    if(dispatching_)return true;
    dispatching_=true;
    bool claimed=false;
    for(auto& entry:entries_){
        if(!entry.filter)continue;
        const auto filter=entry.filter;void* user=entry.user;
        try{
            if(filter(user,&context)==WEAPON_XP_HIT_EXTERNAL){claimed=true;break;}
        }catch(...){
            entry={};claimed=true;break;
        }
    }
    dispatching_=false;return claimed;
}
}

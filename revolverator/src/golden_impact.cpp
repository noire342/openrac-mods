// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "golden_impact.hpp"
#include "damage_targets.hpp"
#include <algorithm>
#include <cmath>

namespace revolverator {
bool golden_damage_class(unsigned klass){
    if(destructible_crate_class(int(klass)))return true;
    // Verified combat actors, including the native bosses. Do not infer damage
    // eligibility from the presence of pvars, collision or an update callback:
    // story NPCs, Ratchet, pickups and moving scenery have those too.
    switch(klass){
    case 28:case 29:case 30:case 31:case 35:case 36:case 44:case 52:case 63:
    case 123:case 133:case 211:case 217:case 252:case 253:case 294:case 326:
    case 333:case 340:case 344:case 347:case 427:case 459:case 563:case 572:
    case 573:case 577:case 578:case 580:case 623:case 625:case 631:case 638:
    case 749:case 865:case 866:case 1023:case 1048:case 1051:case 1059:
    case 1068:case 1106:case 1126:case 1130:case 1196:case 1201:case 1202:
    case 1231:case 1246:case 1262:case 1267:case 1346:case 1371:case 1382:
    case 1422:case 1445:case 1454:case 1564:return true;
    default:return false;
    }
}
float GoldenBlast::damage_at(float distance)const{
    if(!std::isfinite(distance)||distance<0||!std::isfinite(radius)||radius<=0||
       !std::isfinite(damage)||damage<=0||distance>=radius)return 0;
    return damage*(1.f-.65f*(distance/radius));
}
void GoldenImpact::launch(Target projectile,bool golden,float charge,float multiplier){
    cancel();
    if(!projectile||!std::isfinite(charge)||!std::isfinite(multiplier))return;
    projectile_=projectile;golden_=golden;
    const float q=std::clamp(charge,0.f,1.f);
    const float scale=std::clamp(multiplier,1.f,16.f);
    blast_={q,2.5f+3.5f*q,(8.f+8.f*q)*scale,1.2f+1.8f*q,10.f+6.f*q,2.f+3.5f*q};
}
std::optional<GoldenBlast> GoldenImpact::consume(Target projectile,ImpactReason reason){
    if(!pending(projectile))return std::nullopt;
    const bool explode=golden_&&reason==ImpactReason::Collision;
    const auto blast=blast_;
    cancel(); // Consume before dispatch, including re-entrant native callbacks.
    return explode?std::optional<GoldenBlast>{blast}:std::nullopt;
}
bool GoldenBlastPlan::consider(Target target,Vec3 position,float radius,bool visible,bool primary){
    if(!target||!visible||!golden_damage_class(target.klass)||!finite(centre_)||
       !finite(position)||!std::isfinite(radius)||radius<0)return false;
    for(std::size_t i=0;i<size_;++i)if(hits_[i].target.address==target.address)return false;
    const Vec3 delta=position-centre_;
    const float square=length_squared(delta);
    if(!std::isfinite(square)||square<0)return false;
    const float distance=std::max(0.f,std::sqrt(square)-std::clamp(radius,0.f,4.f));
    const float damage=blast_.damage_at(distance);
    if(!(damage>0))return false;
    const float planar=std::sqrt(delta.x*delta.x+delta.y*delta.y);
    const Vec3 direction=planar>.0001f?
        Vec3{delta.x/planar*blast_.knockback,delta.y/planar*blast_.knockback,1.3f+blast_.charge}:
        Vec3{blast_.knockback,0,1.3f+blast_.charge};
    const GoldenBlastHit hit{target,direction,damage,distance,primary};
    const auto before=[](const GoldenBlastHit& a,const GoldenBlastHit& b){
        if(a.primary!=b.primary)return a.primary;
        return a.distance<b.distance;
    };
    if(size_==maximum_hits){
        if(!before(hit,hits_[size_-1]))return false;
        hits_[size_-1]=hit;
    }else hits_[size_++]=hit;
    std::sort(hits_.begin(),hits_.begin()+size_,before);
    return true;
}
}

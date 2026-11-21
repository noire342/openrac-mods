// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal_xp.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__linux__)
#include <link.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace revolverator {
namespace {
constexpr unsigned custom_origin=2017,transport_item=arsenal::NativeWalloperKey;
constexpr unsigned quarters_per_xp=4;
constexpr std::array<unsigned,WEAPON_XP_LEVEL_COUNT> default_thresholds{
    0,400,1400,3200,6000,10000,15500,23000,33000,46000
};
constexpr std::array<float,WEAPON_XP_LEVEL_COUNT> default_damage_multipliers{
    1,1.15f,1.30f,1.50f,1.75f,2,2.25f,2.50f,2.75f,3
};
constexpr std::array<float,WEAPON_XP_LEVEL_COUNT> default_ammo_multipliers{
    1,1.125f,1.25f,1.375f,1.50f,1.625f,1.75f,1.875f,2,2.125f
};

#if !defined(_WIN32)
weapon_xp_query_fn query_existing_image(const char* path){
    if(!path||!*path)return nullptr;
    // NOLOAD only opens an existing reference; closing it balances our probe.
    void* image=dlopen(path,RTLD_NOW|RTLD_NOLOAD);
    if(!image)return nullptr;
    auto query=reinterpret_cast<weapon_xp_query_fn>(dlsym(image,WEAPON_XP_QUERY_SYMBOL));
    dlclose(image);return query;
}
#endif
weapon_xp_query_fn find_provider(){
#if defined(_WIN32)
    const auto module=GetModuleHandleW(L"weapon_xp.dll");
    return module?reinterpret_cast<weapon_xp_query_fn>(GetProcAddress(module,WEAPON_XP_QUERY_SYMBOL)):nullptr;
#elif defined(__linux__)
    weapon_xp_query_fn result{};
    dl_iterate_phdr(+[](dl_phdr_info* info,std::size_t,void* user)->int{
        const auto* path=info->dlpi_name;
        if(!path)return 0;
        const auto* slash=std::strrchr(path,'/');
        if(std::strcmp(slash?slash+1:path,"weapon_xp.so"))return 0;
        auto* result=static_cast<weapon_xp_query_fn*>(user);
        *result=query_existing_image(path);return *result?1:0;
    },&result);
    return result;
#elif defined(__APPLE__)
    for(std::uint32_t i=0;i<_dyld_image_count();++i){
        const auto* path=_dyld_get_image_name(i);
        if(!path)continue;
        const auto* slash=std::strrchr(path,'/');
        if(std::strcmp(slash?slash+1:path,"weapon_xp.dylib"))continue;
        if(auto query=query_existing_image(path))return query;
    }
    return nullptr;
#else
    return nullptr;
#endif
}

// Exact conservative whitelist and nominal bolt formula from our GPL weapon-xp
// enemies.hpp/rewards.hpp. Keep the adapter self-contained in the public mod.
bool rewarded_enemy(int klass){
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
bool valid_configuration(const weapon_xp_configuration_v3& c,const weapon_xp_rules_v3& r){
    if(c.size<sizeof(c)||r.size<sizeof(r)||c.level_count!=10||r.level_count!=10||r.item_id!=18||r.xp_multiplier>16||
       !std::isfinite(c.xp_rate)||c.xp_rate<.25||c.xp_rate>4||c.kill_xp_cap>100000||
       r.thresholds[0]!=0||r.thresholds[9]>1000000000||r.damage[0]!=1||r.ammo[0]!=1)return false;
    for(unsigned i=0;i<10;++i){
        if((i&&r.thresholds[i]<=r.thresholds[i-1])||!std::isfinite(r.damage[i])||!std::isfinite(r.ammo[i])||
           r.damage[i]<1||r.damage[i]>16||r.ammo[i]<1||r.ammo[i]>16||
           (i&&(r.damage[i]<r.damage[i-1]||r.ammo[i]<r.ammo[i-1])))return false;
    }
    return true;
}
unsigned weapon_level(unsigned xp,const weapon_xp_rules_v3& r){
    return unsigned(std::upper_bound(std::begin(r.thresholds),std::end(r.thresholds),xp)-std::begin(r.thresholds));
}
}

NativeArsenalXP::NativeArsenalXP(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal,ProviderResolver resolver)
    :Memory(api),mod_(mod),arsenal_(arsenal),resolver_(resolver?resolver:find_provider){
    configuration_.size=sizeof configuration_;configuration_.xp_rate=1;configuration_.kill_xp_cap=500;configuration_.level_count=10;
    rules_.size=sizeof rules_;rules_.item_id=18;rules_.level_count=10;rules_.xp_multiplier=1;
    std::copy(default_thresholds.begin(),default_thresholds.end(),rules_.thresholds);
    std::copy(default_damage_multipliers.begin(),default_damage_multipliers.end(),rules_.damage);
    std::copy(default_ammo_multipliers.begin(),default_ammo_multipliers.end(),rules_.ammo);
    if(const auto* state=arsenal_.registry().state(arsenal::RevolveratorKey))observed_xp_=state->xp;
}
NativeArsenalXP::~NativeArsenalXP(){detach();}

int NativeArsenalXP::request_acquisition_presentation(int native_item){
    if(!provider_)return WEAPON_XP_NOT_READY;
    constexpr auto bytes=offsetof(weapon_xp_api_v4,request_acquisition_presentation)+sizeof(weapon_xp_api_v4{}.request_acquisition_presentation);
    if(provider_->v3.v2.base.size<bytes||!provider_->request_acquisition_presentation)return WEAPON_XP_NOT_FOUND;
    return provider_->request_acquisition_presentation(native_item);
}
int NativeArsenalXP::acquisition_presentation_active()const{
    if(!provider_)return WEAPON_XP_NOT_READY;
    constexpr auto bytes=offsetof(weapon_xp_api_v4,acquisition_presentation_active)+sizeof(weapon_xp_api_v4{}.acquisition_presentation_active);
    if(provider_->v3.v2.base.size<bytes||!provider_->acquisition_presentation_active)return WEAPON_XP_NOT_FOUND;
    return provider_->acquisition_presentation_active();
}

int NativeArsenalXP::format_name(unsigned level,int language,const char* default_name,
                                char* out,unsigned capacity)const{
    if(!ready())return WEAPON_XP_NOT_READY;
    if(!provider_v5_||!provider_v5_->format_custom_name)return WEAPON_XP_NOT_FOUND;
    const auto* descriptor=arsenal_.registry().descriptor(arsenal::RevolveratorKey);
    if(!descriptor)return WEAPON_XP_NOT_FOUND;
    const weapon_xp_name_context_v5 context{
        sizeof(weapon_xp_name_context_v5),-1,level,language,default_name,descriptor->stable_id.c_str()
    };
    return provider_v5_->format_custom_name(&context,out,capacity);
}

void NativeArsenalXP::report_attach_failure(int result,const char* message){
    if(result==WEAPON_XP_NOT_READY||result==WEAPON_XP_BUSY){
        integration_error_=nullptr;
        return;
    }
    integration_error_=message;
    if(logged_incompatible_)return;
    logged_incompatible_=true;
    if(api_->log)api_->log(mod_,OPENRAC_LOG_WARN,message);
}
void NativeArsenalXP::attach(){
    const auto query=resolver_();
    if(provider_&&query!=query_){
        provider_=nullptr;provider_v5_=nullptr;query_=nullptr;
        filter_token_=listener_token_=0;configuration_valid_=false;reset();
    }
    present_=query!=nullptr;
    if(!query){integration_error_=nullptr;return;}
    if(provider_)return;
    const auto* base=query(WEAPON_XP_ABI_V4);
    constexpr auto filter_api_bytes=offsetof(weapon_xp_api_v4,unregister_hit_filter)+sizeof(weapon_xp_api_v4{}.unregister_hit_filter);
    constexpr const char* unsupported="Revolverator XP requires the Weapon Experience v4 hit and lifecycle APIs";
    if(!base||base->size<filter_api_bytes||base->abi_version!=WEAPON_XP_ABI_V4){
        report_attach_failure(WEAPON_XP_INVALID_ARGUMENT,unsupported);
        return;
    }
    const auto* candidate=reinterpret_cast<const weapon_xp_api_v4*>(base);
    if(!candidate->register_hit_filter||!candidate->unregister_hit_filter||
       !candidate->v3.get_configuration||!candidate->v3.get_rules||!base->subscribe||!base->unsubscribe){
        report_attach_failure(WEAPON_XP_INVALID_ARGUMENT,unsupported);
        return;
    }

    unsigned filter_token{};
    const auto filter_result=candidate->register_hit_filter(filter,this,&filter_token);
    if(filter_result!=WEAPON_XP_OK||!filter_token){
        report_attach_failure(filter_result,"Revolverator XP could not register a Weapon Experience hit filter");
        return;
    }

    const auto* extension=weapon_xp_get_v5(query);
    constexpr auto lifecycle_events=WEAPON_XP_EVENT_MASK(WEAPON_XP_LEVEL_LOADED)|
                                    WEAPON_XP_EVENT_MASK(WEAPON_XP_STOPPING);
    unsigned listener_token{};
    int listener_result=WEAPON_XP_FULL;
    if(extension&&extension->subscribe_filtered){
        listener_result=extension->subscribe_filtered(
            lifecycle_events,WEAPON_XP_SESSION_ITEM,provider_event,this,&listener_token);
    }
    if(listener_result==WEAPON_XP_FULL){
        listener_result=base->subscribe(provider_event,this,&listener_token);
    }
    if(listener_result!=WEAPON_XP_OK||!listener_token){
        // The filter must never outlive a successful STOPPING subscription.
        candidate->unregister_hit_filter(filter_token);
        report_attach_failure(listener_result,"Revolverator XP could not subscribe to Weapon Experience lifecycle events");
        return;
    }

    query_=query;
    provider_=candidate;
    provider_v5_=extension;
    filter_token_=filter_token;
    listener_token_=listener_token;
    integration_error_=nullptr;
}
void NativeArsenalXP::detach()noexcept{
    const auto* provider=provider_;const auto query=query_;
    const auto filter_token=filter_token_,listener_token=listener_token_;
    provider_=nullptr;provider_v5_=nullptr;query_=nullptr;filter_token_=listener_token_=0;configuration_valid_=false;
    if(!provider||resolver_()!=query)return;
    const auto* base=query(WEAPON_XP_ABI_V4);
    if(base!=&provider->v3.v2.base)return;
    if(filter_token)provider->unregister_hit_filter(filter_token);
    if(listener_token&&provider->v3.v2.base.unsubscribe)provider->v3.v2.base.unsubscribe(listener_token);
}
void NativeArsenalXP::refresh_configuration(){
    if(!provider_)return;
    weapon_xp_configuration_v3 configuration{};configuration.size=sizeof configuration;
    weapon_xp_rules_v3 rules{};rules.size=sizeof rules;
    if(provider_->v3.get_configuration(&configuration)!=WEAPON_XP_OK||
       provider_->v3.get_rules(transport_item,&rules)!=WEAPON_XP_OK||!valid_configuration(configuration,rules)){
        configuration_valid_=false;return;
    }
    configuration_=configuration;rules_=rules;configuration_valid_=true;
}
bool NativeArsenalXP::recent(std::uint64_t now,std::uint64_t before,unsigned ttl)noexcept{
    return now>=before&&now-before<=ttl;
}
void NativeArsenalXP::frame(){
    ++tick_;attach();refresh_configuration();
    const int loaded=api_->loaded_level();
    if(loaded!=level_){reset();level_=loaded;}
    before_hit();
    if(const auto* state=arsenal_.registry().state(arsenal::RevolveratorKey)){
        if(state->xp!=observed_xp_)quarters_=0;
        observed_xp_=state->xp;
    }
    for(auto& p:pending_)if(p.valid&&p.at!=tick_)p={};
    for(auto& a:accepted_)if(a.valid&&!recent(tick_,a.at,ReceiptLifetime))a={};
}
void NativeArsenalXP::before_hit(){
    active_=false;body_=held_=0;
    const int loaded=api_->loaded_level();
    if(loaded<0||loaded>18)return;
    const auto hero=global(0x13f350);
    body_=read<unsigned>(hero+0x2080);held_=read<unsigned>(hero+0x1090);
    active_=arsenal_.revolverator_active();
}
void NativeArsenalXP::reset(bool clear_fraction)noexcept{
    pending_={};accepted_={};event_head_=event_count_=0;provider_epoch_=0;
    if(clear_fraction)quarters_=0;
}
void NativeArsenalXP::provider_event(void* user,const weapon_xp_event_v1* event)noexcept{
    if(!user||!event||event->size<WEAPON_XP_EVENT_V1_BASE_SIZE)return;
    auto& self=*static_cast<NativeArsenalXP*>(user);
    if(event->kind==WEAPON_XP_LEVEL_LOADED)self.reset();
    if(event->kind==WEAPON_XP_STOPPING){
        self.provider_=nullptr;self.provider_v5_=nullptr;self.query_=nullptr;self.filter_token_=self.listener_token_=0;
        self.configuration_valid_=false;self.present_=false;self.reset();
    }
}
std::int32_t NativeArsenalXP::filter(void* user,const weapon_xp_hit_context_v4* context)noexcept{
    if(!user||!context||context->size<sizeof(*context))return WEAPON_XP_HIT_NATIVE;
    return static_cast<NativeArsenalXP*>(user)->classify(*context);
}
std::int32_t NativeArsenalXP::classify(const weapon_xp_hit_context_v4& context)noexcept{
    const bool custom=context.origin_class==int(custom_origin)||
        (active_&&context.attacker_moby&&(context.attacker_moby==body_||context.attacker_moby==held_)&&
         (context.flags&0x30000u)==0x30000u);
    if(!custom){block_hit(context.target_moby);return WEAPON_XP_HIT_NATIVE;}
    // No guest reads/writes, native calls or provider mutations in this callback.
    if(provider_epoch_&&provider_epoch_!=context.level_epoch)reset();
    provider_epoch_=context.level_epoch;
    Pending* slot=nullptr;
    for(auto& p:pending_)if(p.valid&&p.context.target_moby==context.target_moby){slot=&p;break;}
    if(!slot)for(auto& p:pending_)if(!p.valid){slot=&p;break;}
    if(!slot)slot=&*std::min_element(pending_.begin(),pending_.end(),[](const Pending& a,const Pending& b){return a.at<b.at;});
    *slot={context,tick_,true};return WEAPON_XP_HIT_EXTERNAL;
}
NativeArsenalXP::Identity NativeArsenalXP::identity(unsigned moby)const{
    if(!valid(moby,256))return {};
    const auto pvars=read<unsigned>(moby+0x78);
    const auto klass=read<unsigned short>(moby+0xa6);
    if(!valid(pvars,1)||!rewarded_enemy(klass))return {};
    return {moby,pvars,klass,read<unsigned>(moby+0x38)};
}
bool NativeArsenalXP::same(const Identity& a,const Identity& b)noexcept{
    return a.moby==b.moby&&a.pvars==b.pvars&&a.klass==b.klass&&a.stamp==b.stamp;
}
void NativeArsenalXP::block_hit(unsigned target)noexcept{
    for(auto& p:pending_)if(p.valid&&p.context.target_moby==target)p={};
    for(auto& a:accepted_)if(a.valid&&a.identity.moby==target)a={};
}
void NativeArsenalXP::confirm_hit(unsigned target,unsigned hit){
    if(!ready()||!valid(hit,64)||read<unsigned>(hit+0x34)!=target)return;
    Pending* pending=nullptr;
    for(auto& p:pending_)if(p.valid&&p.context.target_moby==target&&p.context.hit_address==hit){pending=&p;break;}
    if(!pending||pending->at!=tick_)return;
    const auto context=pending->context;pending->valid=false;
    const auto token=identity(target);
    const float outgoing_damage=read<float>(hit+0x2c);
    const int origin=read<float>(hit+0x1c)==5627.9248046875f?read<unsigned short>(hit+0x2a):-1;
    if(!token.moby||int(token.klass)!=context.target_class||read<unsigned char>(target+0x20)>=0x80||
       read<unsigned>(hit+0x20)!=context.attacker_moby||read<unsigned>(hit+0x24)!=context.flags||origin!=context.origin_class||
       !std::isfinite(outgoing_damage)||outgoing_damage<=0||outgoing_damage>1000000)return;
    std::array<unsigned char,64> payload{};std::memcpy(payload.data(),api_->memory+hit,64);
    Accepted* slot=nullptr;
    for(auto& a:accepted_)if(a.valid&&a.identity.moby==target){slot=&a;break;}
    if(slot&&same(slot->identity,token)&&slot->hit==hit&&slot->epoch==context.level_epoch&&slot->payload==payload)return;
    if(!slot)for(auto& a:accepted_)if(!a.valid){slot=&a;break;}
    if(!slot)slot=&*std::min_element(accepted_.begin(),accepted_.end(),[](const Accepted& a,const Accepted& b){return a.at<b.at;});
    // Charged throws (directional origin2017) already apply their multiplier in
    // NativeWorld. Accepted native melee uses this final GetHit boundary instead.
    // Keep the resulting packet as the receipt so repeated GetHit observations
    // return above without multiplying the same outgoing damage again.
    if(origin!=int(custom_origin)){
        const float multiplier=get_stats().damage_multiplier;
        if(multiplier!=1){
            const float scaled_damage=outgoing_damage*multiplier;
            write(hit+0x2c,scaled_damage);
            std::memcpy(payload.data()+0x2c,&scaled_damage,sizeof scaled_damage);
        }
    }
    *slot={token,payload,hit,context.level_epoch,tick_,true,false};
}
int NativeArsenalXP::legacy_nominal_bolts(unsigned target)const{
    if(!valid(target,256)||!rewarded_enemy(read<short>(target+0xa6)))return -1;
    const int group=read<unsigned char>(target+0xb1);
    const int save_id=read<short>(target+0xb2);
    if(group==0xfe||save_id<0)return 0;
    int base=read<short>(target+0xb4),budget=read<short>(target+0xb6);
    if(group<128){
        if(group>=64)return 0;
        const int level=read<int>(global(0x15ed84));
        if(level<0||level>=20)return 0;
        const int collected=read<short>(global(0x14d590)+(unsigned(level)*64+unsigned(group))*4+2);
        const int remaining=budget-collected;
        if(remaining>(budget+1)/2)base=remaining;
        base=std::max(1,base);
    }
    return std::max(0,base);
}

std::optional<weapon_xp_balance_v5> NativeArsenalXP::balance_after_kill(
    unsigned target,weapon_xp_balance_v5 balance,unsigned cap)const{
    if(provider_v5_&&provider_v5_->get_native_reward&&provider_v5_->evaluate_progress){
        std::uint32_t bolts=0;
        if(provider_v5_->get_native_reward(target,&bolts)!=WEAPON_XP_OK)return std::nullopt;
        weapon_xp_progress_v5 result{};
        result.size=sizeof result;
        // Preserve the historical balancing preset with the Rotatore's own balance.
        if(provider_v5_->evaluate_progress(&rules_,&balance,bolts,&result)!=WEAPON_XP_OK||
           result.size<sizeof result||result.xp<balance.xp||result.xp>cap||
           result.xp_quarters>=quarters_per_xp||
           (result.xp==cap&&result.xp_quarters!=0)||
           (result.xp==balance.xp&&result.xp_quarters<balance.xp_quarters))return std::nullopt;
        balance.xp=result.xp;
        balance.xp_quarters=result.xp_quarters;
        return balance;
    }

    // The v4 provider predates the shared reward and progression helpers.
    const int bolts=legacy_nominal_bolts(target);
    if(bolts<0)return std::nullopt;
    const auto award=std::uint64_t(std::min(unsigned(bolts),configuration_.kill_xp_cap))*
        unsigned(std::lround(configuration_.xp_rate*quarters_per_xp))*rules_.xp_multiplier;
    const auto total=award+balance.xp_quarters;
    balance.xp+=unsigned(std::min<std::uint64_t>(total/quarters_per_xp,cap-balance.xp));
    balance.xp_quarters=balance.xp==cap?0:unsigned(total%quarters_per_xp);
    return balance;
}

void NativeArsenalXP::death_before(unsigned target){
    if(api_->loaded_level()!=level_||level_<0||level_>18)return;
    Accepted* accepted=nullptr;
    for(auto& a:accepted_)if(a.valid&&a.identity.moby==target){accepted=&a;break;}
    if(!accepted||accepted->credited||!recent(tick_,accepted->at,ReceiptLifetime)||
       !same(accepted->identity,identity(target)))return;
    accepted->credited=true; // Consume even a zero-rate/zero-bolt death exactly once.
    if(!ready()||read<int>(level(0x15f5c4))!=0)return;
    const auto* current=arsenal_.registry().state(arsenal::RevolveratorKey);
    if(!current||!current->owned)return;
    auto state=*current;
    if(state.xp!=observed_xp_)quarters_=0;
    const auto before=weapon_level(state.xp,rules_);
    const auto cap=rules_.thresholds[WEAPON_XP_LEVEL_COUNT-1];
    if(state.xp>=cap){quarters_=0;observed_xp_=state.xp;return;}
    const weapon_xp_balance_v5 balance{sizeof balance,state.xp,quarters_};
    const auto next=balance_after_kill(target,balance,cap);
    if(!next)return;
    state.xp=next->xp;
    if(!arsenal_.registry().set_state(arsenal::RevolveratorKey,state))return;
    quarters_=next->xp_quarters;observed_xp_=state.xp;
    queue_level_up(before,weapon_level(state.xp,rules_),state.xp);
}
void NativeArsenalXP::queue_level_up(unsigned before,unsigned after,unsigned xp)noexcept{
    for(unsigned next=before+1;next<=after&&event_count_<MaxEvents;++next){
        events_[(event_head_+event_count_)%MaxEvents]={next-1,next,xp};++event_count_;
    }
}
bool NativeArsenalXP::poll_level_up(LevelUp& event)noexcept{
    if(!event_count_)return false;
    event=events_[event_head_];event_head_=(event_head_+1)%MaxEvents;--event_count_;return true;
}
NativeArsenalXP::Stats NativeArsenalXP::get_stats()const{
    Stats result;
    if(const auto* state=arsenal_.registry().state(arsenal::RevolveratorKey))result.xp=state->xp;
    result.level=weapon_level(result.xp,rules_);
    result.level_start_xp=rules_.thresholds[result.level-1];
    result.next_level_xp=rules_.thresholds[result.level==10?9:result.level];
    result.progress=result.level==10?1.f:float(result.xp-result.level_start_xp)/float(result.next_level_xp-result.level_start_xp);
    if(flags()&WEAPON_XP_SCALE_DAMAGE)result.damage_multiplier=rules_.damage[result.level-1];
    if(flags()&WEAPON_XP_SCALE_CAPACITY)result.ammo_multiplier=rules_.ammo[result.level-1];
    return result;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal_xp.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

using namespace revolverator;
namespace {
int checks;
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"arsenal XP: %s\n",why);std::exit(1);}}
struct View final:ArsenalIdentityView {void select_identity(bool)override{}};
struct Fixture;
Fixture* live;
struct Fixture {
    enum Provider {Absent,Legacy,V4};
    std::vector<unsigned char> memory=std::vector<unsigned char>(0x2000000);
    openrac_api api{};arsenal::Registry registry;View view;NativeArsenal arsenal;
    weapon_xp_api_v4 provider{};weapon_xp_configuration_v3 configuration{};weapon_xp_rules_v3 rules{};
    weapon_xp_hit_filter_v4 filter{};void* filter_user{};
    weapon_xp_listener_v1 listener{};void* listener_user{};
    Provider mode=V4;unsigned registers{},unregisters{},subscriptions{},unsubscriptions{},warnings{},native_calls{},next_target{};
    int level=1,registration_result=WEAPON_XP_OK,configuration_result=WEAPON_XP_OK;
    int presentation_result=WEAPON_XP_OK,presentation_active=0,last_presentation_item=-1;
    unsigned presentation_requests{};
    unsigned epoch=1;
    static constexpr unsigned hero=0x13f350,body=0x300000,held=0x300100;
    template<class T>void put(unsigned at,const T& value){std::memcpy(memory.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,memory.data()+at,sizeof value);return value;}
    Fixture():arsenal(&api,registry,view){
        live=this;api.memory=memory.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=+[]{return live->level;};api.relocate_data=+[](unsigned at){return at;};
        api.relocate_level_data=+[](int,unsigned at){return at;};
        api.log=+[](openrac_mod*,int level,const char*){if(level==OPENRAC_LOG_WARN)++live->warnings;};
        api.function=+[](const char*)->void*{++live->native_calls;return nullptr;};
        registry.register_weapon({arsenal::RevolveratorKey,"noire342.revolverator",18,60037,10,15000,12});
        put<int>(0x15ed84,level);put<int>(0x15f5c4,0);put<unsigned>(hero+0x2080,body);put<unsigned>(hero+0x1090,held);
        put<int>(hero+0x10b8,18);put<int>(hero+0x10b4,2);
        arsenal.begin_level();arsenal.finish_level();
        registry.set_state(arsenal::RevolveratorKey,{true,10,0});registry.equip(arsenal::RevolveratorKey);arsenal.restore_gameplay();
        provider.v3.v2.base.size=sizeof provider;provider.v3.v2.base.abi_version=4;
        provider.register_hit_filter=+[](weapon_xp_hit_filter_v4 fn,void* user,unsigned* token)->int{
            ++live->registers;if(live->registration_result)return live->registration_result;
            live->filter=fn;live->filter_user=user;*token=41;return WEAPON_XP_OK;
        };
        provider.unregister_hit_filter=+[](unsigned token)->int{
            require(token==41,"consumer unregisters its own filter token");++live->unregisters;live->filter=nullptr;return WEAPON_XP_OK;
        };
        provider.v3.v2.base.subscribe=+[](weapon_xp_listener_v1 fn,void* user,unsigned* token)->int{
            ++live->subscriptions;live->listener=fn;live->listener_user=user;*token=42;return WEAPON_XP_OK;
        };
        provider.v3.v2.base.unsubscribe=+[](unsigned token)->int{
            require(token==42,"consumer unsubscribes its own listener token");++live->unsubscriptions;live->listener=nullptr;return WEAPON_XP_OK;
        };
        provider.v3.get_configuration=+[](weapon_xp_configuration_v3* out)->int{
            if(live->configuration_result)return live->configuration_result;
            *out=live->configuration;return WEAPON_XP_OK;
        };
        provider.v3.get_rules=+[](int item,weapon_xp_rules_v3* out)->int{
            require(item==18,"custom progression reads the Walloper transport's balancing rules");*out=live->rules;return WEAPON_XP_OK;
        };
        provider.request_acquisition_presentation=+[](int item)->int{
            ++live->presentation_requests;live->last_presentation_item=item;return live->presentation_result;
        };
        provider.acquisition_presentation_active=+[]()->int{return live->presentation_active;};
        configuration.size=sizeof configuration;configuration.flags=WEAPON_XP_SHOW_HUD|WEAPON_XP_VERSIONED_NAMES;
        configuration.level_count=10;configuration.xp_rate=1;configuration.kill_xp_cap=500;configuration.revision=7;
        rules.size=sizeof rules;rules.item_id=18;rules.level_count=10;rules.xp_multiplier=1;
        const std::array<unsigned,10> thresholds{0,400,1400,3200,6000,10000,15500,23000,33000,46000};
        std::copy(thresholds.begin(),thresholds.end(),rules.thresholds);
        for(unsigned i=0;i<10;++i){rules.damage[i]=1+.2f*i;rules.ammo[i]=1+.1f*i;}
    }
    static const weapon_xp_api_v1* query(unsigned abi){return abi==4&&live->mode==V4?&live->provider.v3.v2.base:nullptr;}
    static weapon_xp_query_fn resolve(){return live->mode==Absent?nullptr:query;}
    unsigned xp()const{return registry.state(arsenal::RevolveratorKey)->xp;}
    void set_xp(unsigned xp){auto state=*registry.state(arsenal::RevolveratorKey);state.xp=xp;registry.set_state(arsenal::RevolveratorKey,state);}
    weapon_xp_hit_context_v4 target(int bolts=16,int klass=459){
        const auto index=next_target++;
        const unsigned moby=0x400000+index*256,hit=0x500000+(index%64)*64;
        put<unsigned>(moby+0x78,0x600000+index*64);put<unsigned>(moby+0x38,index+1);
        put<unsigned short>(moby+0xa6,static_cast<unsigned short>(klass));
        put<unsigned char>(moby+0xb1,0xff);put<short>(moby+0xb2,5);
        put<short>(moby+0xb4,static_cast<short>(bolts));put<short>(moby+0xb6,static_cast<short>(bolts));
        put<unsigned>(hit+0x20,held);put<unsigned>(hit+0x24,0x30000);
        put<unsigned short>(hit+0x2a,2017);put<float>(hit+0x1c,5627.9248046875f);
        put<float>(hit+0x2c,3);put<unsigned>(hit+0x34,moby);
        weapon_xp_hit_context_v4 context{};context.size=sizeof context;context.target_moby=moby;context.hit_address=hit;
        context.target_class=klass;context.attacker_moby=held;context.attacker_class=180;context.native_item=18;
        context.origin_class=2017;context.raw_damage=3;context.flags=0x30000;context.tick=index+100;context.level_epoch=epoch;
        return context;
    }
    int dispatch(const weapon_xp_hit_context_v4& context){require(filter!=nullptr,"filter registered before dispatch");return filter(filter_user,&context);}
    void accepted(NativeArsenalXP& xp,const weapon_xp_hit_context_v4& context){
        require(dispatch(context)==WEAPON_XP_HIT_EXTERNAL,"custom damage is excluded from native Walloper progression");
        xp.confirm_hit(context.target_moby,context.hit_address);
    }
    void kill(NativeArsenalXP& xp,const weapon_xp_hit_context_v4& context){accepted(xp,context);xp.death_before(context.target_moby);}
    void change_source(weapon_xp_hit_context_v4& context,unsigned attacker,int origin,unsigned flags){
        context.attacker_moby=attacker;context.origin_class=origin;context.flags=flags;
        put<unsigned>(context.hit_address+0x20,attacker);put<unsigned>(context.hit_address+0x24,flags);
        put<unsigned short>(context.hit_address+0x2a,static_cast<unsigned short>(origin<0?0:origin));
        put<float>(context.hit_address+0x1c,origin<0?0:5627.9248046875f);
    }
};
}

int main(){
    {Fixture f;f.mode=Fixture::Absent;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(!xp.ready()&&xp.compatible()&&!xp.provider_present(),"standalone Revolverator remains available without an XP provider");
     const auto c=f.target();xp.confirm_hit(c.target_moby,c.hit_address);xp.death_before(c.target_moby);
     require(f.xp()==0&&f.registers==0,"an absent provider never grants or invents XP");}
    {Fixture f;f.mode=Fixture::Absent;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(xp.request_acquisition_presentation()==WEAPON_XP_NOT_READY&&xp.acquisition_presentation_active()==WEAPON_XP_NOT_READY,
             "optional presentation calls never dereference an absent provider");}
    {Fixture f;f.mode=Fixture::Legacy;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;
     for(int i=0;i<8;++i)xp.frame();
     require(!xp.compatible()&&!xp.ready()&&f.warnings==1,"an installed older provider is rejected clearly and logged once");
     f.mode=Fixture::V4;xp.frame();require(xp.compatible()&&xp.ready(),"provider compatibility can recover when the v4 service appears");}
    {Fixture f;{auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;for(int i=0;i<8;++i)xp.frame();
     require(xp.ready()&&f.registers==1&&f.subscriptions==1,"stable service attaches one filter and one lifecycle listener");
     require(xp.flags()==f.configuration.flags&&xp.rules().thresholds[9]==46000,"rules and flags are copied from the provider");}
     require(f.unregisters==1&&f.unsubscriptions==1,"consumer unload releases both tokens exactly once");}
    {Fixture f;f.provider.v3.v2.base.size=offsetof(weapon_xp_api_v4,request_acquisition_presentation);
     auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(xp.ready()&&xp.compatible(),"the original v4 filter prefix remains compatible without the new optional tail");
     require(xp.request_acquisition_presentation()==WEAPON_XP_NOT_FOUND&&xp.acquisition_presentation_active()==WEAPON_XP_NOT_FOUND&&f.presentation_requests==0,
             "an absent presentation tail is checked by size before dereferencing its pointers");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(xp.request_acquisition_presentation()==WEAPON_XP_OK&&f.last_presentation_item==18,"custom level-up scenes use the native carrier without native XP awards");
     f.presentation_result=WEAPON_XP_BUSY;require(xp.request_acquisition_presentation(11)==WEAPON_XP_BUSY&&f.last_presentation_item==11,"presentation queue busy status is preserved for retry");
     f.presentation_result=WEAPON_XP_DISABLED;require(xp.request_acquisition_presentation()==WEAPON_XP_DISABLED,"animation-disabled setting is preserved");
     f.presentation_active=1;require(xp.acquisition_presentation_active()==1,"custom presentation active state includes its pending native scene");
     f.presentation_active=0;require(xp.acquisition_presentation_active()==0,"normal native scenes do not hold the custom presentation state");}
    {Fixture f;f.registration_result=WEAPON_XP_NOT_READY;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(xp.compatible()&&!xp.ready()&&f.warnings==0,"a not-yet-ready provider can be retried without a false incompatibility");
     f.registration_result=WEAPON_XP_OK;xp.frame();require(xp.ready(),"registration is retried after provider startup");}
    {Fixture f;f.registration_result=WEAPON_XP_FULL;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     require(!xp.compatible()&&!xp.ready()&&f.warnings==1,"lack of a filter slot cannot silently credit the native Walloper");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();const auto memory=f.memory;
     require(f.dispatch(c)==WEAPON_XP_HIT_EXTERNAL,"directional2017 origin owns the hit");
     require(f.memory==memory&&f.native_calls==0&&f.xp()==0,"classification only copies a receipt; no guest mutation, game call or award occurs");
     xp.death_before(c.target_moby);require(f.xp()==0,"an observed hit without parent acceptance cannot earn XP");
     xp.confirm_hit(c.target_moby,c.hit_address);xp.death_before(c.target_moby);require(f.xp()==16,"accepted custom death earns the native bolt payout");
     xp.death_before(c.target_moby);f.accepted(xp,c);xp.death_before(c.target_moby);
     require(f.xp()==16,"duplicate death and identical ring observations cannot award twice");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();auto c=f.target(10);
     f.change_source(c,Fixture::body,-1,0x30000);f.kill(xp,c);require(f.xp()==10,"active Revolverator native body strikes count against uncapturable targets");
     c=f.target(12);f.change_source(c,Fixture::held,-1,0x30000);f.kill(xp,c);require(f.xp()==22,"active held transport strikes are also isolated");}
    {Fixture f;f.set_xp(400);f.configuration.flags|=WEAPON_XP_SCALE_DAMAGE;
     auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     auto c=f.target(16);f.change_source(c,Fixture::body,-1,0x30000);
     f.dispatch(c);require(f.get<float>(c.hit_address+0x2c)==3,"optional melee damage never mutates a packet from the filter callback");
     xp.confirm_hit(c.target_moby,c.hit_address);
     const float scaled=3*f.rules.damage[1];
     require(std::abs(f.get<float>(c.hit_address+0x2c)-scaled)<.00001f,"accepted native Revolverator melee applies its configured damage multiplier");
     f.accepted(xp,c);require(f.get<float>(c.hit_address+0x2c)==scaled,"duplicate accepted reads do not multiply native melee damage twice");
     f.rules.damage[1]=1.4f;xp.frame();f.accepted(xp,c);
     require(f.get<float>(c.hit_address+0x2c)==scaled,"a settings change does not rescale an already accepted native packet");
     f.put<float>(c.hit_address+0x2c,3);f.accepted(xp,c);
     require(std::abs(f.get<float>(c.hit_address+0x2c)-4.2f)<.00001f,"fresh raw damage reusing a ring cell gets the current multiplier once");
     xp.death_before(c.target_moby);xp.death_before(c.target_moby);
     require(f.xp()==416,"scaled melee still consumes the death reward once");}
    {Fixture f;f.set_xp(1400);f.configuration.flags|=WEAPON_XP_SCALE_DAMAGE;
     auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     auto c=f.target();const float charged_damage=3*f.rules.damage[2];f.put<float>(c.hit_address+0x2c,charged_damage);c.raw_damage=charged_damage;
     f.accepted(xp,c);f.accepted(xp,c);
     require(f.get<float>(c.hit_address+0x2c)==charged_damage,"origin2017 charged throws retain NativeWorld's damage without a second bonus");
     c=f.target();f.change_source(c,Fixture::held,-1,0x30000);f.accepted(xp,c);
     require(std::abs(f.get<float>(c.hit_address+0x2c)-charged_damage)<.00001f,"native held-weapon melee gets the same optional configured bonus");}
    {Fixture f;f.set_xp(1400);auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     auto c=f.target();f.change_source(c,Fixture::body,-1,0x30000);f.accepted(xp,c);
     require(f.get<float>(c.hit_address+0x2c)==3,"native melee damage remains unchanged with the default damage setting off");
     f.configuration.flags|=WEAPON_XP_SCALE_DAMAGE;xp.frame();c=f.target();f.change_source(c,Fixture::body,-1,0x30000);f.dispatch(c);xp.block_hit(c.target_moby);xp.confirm_hit(c.target_moby,c.hit_address);
     require(f.get<float>(c.hit_address+0x2c)==3,"suppressed melee is never scaled by the optional damage adapter");}
    {Fixture f;f.registry.equip(11);f.arsenal.restore_gameplay();auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     f.registry.equip(arsenal::RevolveratorKey);f.arsenal.restore_gameplay();xp.before_hit();auto c=f.target(6);f.change_source(c,Fixture::body,-1,0x30000);f.kill(xp,c);
     require(f.xp()==6,"a quick-select change after frame dispatch is reflected before inner GetHit classification");
     f.registry.equip(11);f.arsenal.restore_gameplay();xp.before_hit();c=f.target(8);f.change_source(c,Fixture::body,-1,0x30000);
     require(f.dispatch(c)==WEAPON_XP_HIT_NATIVE,"switching away within the same frame restores native body-strike credit immediately");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();auto c=f.target();f.accepted(xp,c);
     f.registry.equip(11);f.arsenal.restore_gameplay();xp.frame();
     f.change_source(c,Fixture::body,-1,0x30000);
     require(f.dispatch(c)==WEAPON_XP_HIT_NATIVE,"the original Walloper remains native when custom identity is inactive");
     xp.death_before(c.target_moby);require(f.xp()==0,"a later native source erases earlier custom damage provenance");
     c=f.target(7);f.kill(xp,c);require(f.xp()==7,"stamped custom projectiles retain ownership after switching weapons");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();auto c=f.target();
     f.change_source(c,0x300200,-1,0x30000);require(f.dispatch(c)==WEAPON_XP_HIT_NATIVE,"matching source class alone cannot impersonate Ratchet's weapon");
     f.change_source(c,Fixture::body,-1,0x10000);require(f.dispatch(c)==WEAPON_XP_HIT_NATIVE,"unrelated body damage flags remain native");
     xp.confirm_hit(c.target_moby,c.hit_address);xp.death_before(c.target_moby);require(f.xp()==0,"unowned native hits never use the custom balance");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
     xp.block_hit(c.target_moby);xp.death_before(c.target_moby);require(f.xp()==0,"parent suppression invalidates accepted and pending credit");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();auto c=f.target();f.accepted(xp,c);
     f.change_source(c,0x300300,153,0x10000);c.native_item=11;
     require(f.dispatch(c)==WEAPON_XP_HIT_NATIVE,"another native player's weapon stays native while Revolverator is selected");
     xp.death_before(c.target_moby);require(f.xp()==0,"the last native damage source supersedes earlier accepted custom provenance");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.dispatch(c);xp.frame();
     xp.confirm_hit(c.target_moby,c.hit_address);xp.death_before(c.target_moby);require(f.xp()==0,"acceptance must occur synchronously after its GetHit observation");}
    for(unsigned field:{0x78u,0x38u,0xa6u}){
        Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
        if(field==0xa6)f.put<unsigned short>(c.target_moby+field,577);else f.put<unsigned>(c.target_moby+field,f.get<unsigned>(c.target_moby+field)+64);
        xp.death_before(c.target_moby);require(f.xp()==0,"class, pvars and actor stamp all guard reused native addresses");
    }
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();
     f.put<unsigned>(c.target_moby+0x78,0);f.kill(xp,c);require(f.xp()==0,"invalid entity-private memory cannot authorize a reward");}
    for(unsigned age:{120u,121u}){
        Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
        for(unsigned i=0;i<age;++i)xp.frame();
        xp.death_before(c.target_moby);
        require(f.xp()==(age==120?16u:0u),"credit expires after the same120-frame window as native weapon XP");
    }
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
     for(unsigned i=0;i<119;++i)xp.frame();
     f.accepted(xp,c);xp.frame();xp.frame();xp.death_before(c.target_moby);
     require(f.xp()==0,"repeated unchanged ring reads never extend the lifetime of an old accepted hit");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
     ++f.epoch;const auto next=f.target();f.dispatch(next);xp.death_before(c.target_moby);
     require(f.xp()==0,"provider epoch invalidates receipts from previous native worlds");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();auto c=f.target(4);
     f.put<unsigned char>(c.target_moby+0xb1,3);f.put<short>(c.target_moby+0xb6,100);f.kill(xp,c);
     require(f.xp()==100,"first enemy encounter follows its remaining native bolt budget");
     f.put<short>(0x14d590+(64u+3)*4+2,60);c=f.target(4);f.put<unsigned char>(c.target_moby+0xb1,3);f.put<short>(c.target_moby+0xb6,100);f.kill(xp,c);
     require(f.xp()==104,"repeated enemy XP follows the lower native bolt payout");
     f.put<short>(0x14d590+(64u+3)*4+2,100);c=f.target(0);f.put<unsigned char>(c.target_moby+0xb1,3);f.put<short>(c.target_moby+0xb6,100);f.kill(xp,c);
     require(f.xp()==105,"tracked native sources retain the retail one-bolt floor");
     c=f.target(50);f.put<short>(c.target_moby+0xb2,-1);f.kill(xp,c);require(f.xp()==105,"non-rewarding save IDs grant no XP");
     c=f.target(50);f.put<unsigned char>(c.target_moby+0xb1,0xfe);f.kill(xp,c);require(f.xp()==105,"native disabled reward groups grant no XP");
     c=f.target(50);f.put<unsigned char>(c.target_moby+0xb1,70);f.kill(xp,c);require(f.xp()==105,"unsupported reward groups match native zero payout");
     c=f.target(50,502);f.kill(xp,c);require(f.xp()==105,"crates and helpers never grant weapon experience");}
    {Fixture f;f.configuration.xp_rate=.25;f.configuration.kill_xp_cap=1;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();
     for(int i=0;i<3;++i)f.kill(xp,f.target(100));
     require(f.xp()==0,"fractional rewards are retained instead of rounded up per kill");
     xp.reset();f.kill(xp,f.target(100));require(f.xp()==1,"quarter XP survives ordinary level/receipt resets");
     for(int i=0;i<3;++i)f.kill(xp,f.target(100));
     xp.reset(true);f.kill(xp,f.target(100));
     require(f.xp()==1,"save-slot resets discard only the session fraction");}
    {Fixture f;f.configuration.xp_rate=1.125;f.configuration.kill_xp_cap=7;f.rules.xp_multiplier=3;
     auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();f.kill(xp,f.target(100));
     require(f.xp()==26,"global rate rounds to quarters exactly like provider and cap precedes per-weapon multiplier");
     f.rules.xp_multiplier=0;xp.frame();f.kill(xp,f.target(100));require(f.xp()==26,"zero weapon multiplier pauses future awards");}
    {Fixture f;for(unsigned i=0;i<10;++i)f.rules.thresholds[i]=i;
     auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target(4);f.accepted(xp,c);
     NativeArsenalXP::LevelUp event;require(!xp.poll_level_up(event),"level-up events never fire from the XP provider's filter callback");
     xp.death_before(c.target_moby);unsigned seen=0;
     while(xp.poll_level_up(event)){++seen;require(event.old_level==seen&&event.new_level==seen+1&&event.xp==4,"each crossed level produces an ordered queued transition");}
     auto stats=xp.get_stats();require(seen==4&&stats.level==5&&stats.xp==4&&stats.level_start_xp==4&&stats.next_level_xp==5&&stats.progress==0,"custom stats use copied thresholds and persisted logical XP");
     require(stats.damage_multiplier==1&&stats.ammo_multiplier==1,"damage and capacity boosts default off");
     f.configuration.flags|=WEAPON_XP_SCALE_DAMAGE|WEAPON_XP_SCALE_CAPACITY;xp.frame();stats=xp.get_stats();
     require(std::abs(stats.damage_multiplier-f.rules.damage[4])<.0001f&&std::abs(stats.ammo_multiplier-f.rules.ammo[4])<.0001f,"optional upgrades expose the provider's configured values");
     f.kill(xp,f.target(100));require(f.xp()==9&&xp.get_stats().level==10&&xp.get_stats().progress==1,"progression caps at the tenth configured threshold");
     f.set_xp(100);xp.frame();f.kill(xp,f.target(100));require(f.xp()==100&&xp.get_stats().level==10,"lowering a rule cap never erases already earned XP");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();f.configuration.xp_rate=0;xp.frame();
     require(!xp.ready(),"invalid configuration is never used for awards");const auto c=f.target();f.accepted(xp,c);xp.death_before(c.target_moby);
     require(f.xp()==0,"invalid balancing still isolates native credit while disabling custom awards");
     f.configuration.xp_rate=1;xp.frame();require(xp.ready(),"valid provider configuration restores progression");}
    {Fixture f;auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
     f.configuration.xp_rate=0;xp.frame();xp.death_before(c.target_moby);f.configuration.xp_rate=1;xp.frame();xp.death_before(c.target_moby);
     require(f.xp()==0,"a death is consumed once even if configuration temporarily disables progression");}
    {Fixture f;{auto xp_owner=std::make_unique<NativeArsenalXP>(&f.api,nullptr,f.arsenal,Fixture::resolve);auto& xp=*xp_owner;xp.frame();const auto c=f.target();f.accepted(xp,c);
     weapon_xp_event_v1 event{};event.size=sizeof event;event.kind=WEAPON_XP_STOPPING;f.listener(f.listener_user,&event);
     require(!xp.ready()&&!xp.provider_present(),"provider stopping notification invalidates borrowed service pointers");}
     require(f.unregisters==0&&f.unsubscriptions==0,"no callback into the stopped provider is made during later consumer unload");}
    std::printf("arsenal XP: %d provenance, configuration, reward and lifecycle checks passed\n",checks);
}

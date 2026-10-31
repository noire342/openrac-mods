// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace revolverator;
using namespace arsenal;
namespace {
constexpr unsigned owned=0x13d4c0,unlocked=0x13d408,acquired=0x13d4e8,ammo=0x13d428,variant=0x13e520;
constexpr unsigned definitions=0x1863d0,overlay_definitions=0x179ac0,prices=0x1c40b0,vendor_slots=0x1c8ff0;
constexpr unsigned favourites=0x141ea0,hero=0x13f350,mode=0x15f5c4;
constexpr unsigned item=18;
int checks;
void require(bool pass,const char* reason){++checks;if(!pass){std::fprintf(stderr,"native arsenal: %s\n",reason);std::exit(1);}}
struct View final:ArsenalIdentityView {bool custom=false;unsigned calls=0;void select_identity(bool value)override{custom=value;++calls;}};
struct Fixture;
Fixture* live;
struct Fixture {
    std::vector<unsigned char> memory=std::vector<unsigned char>(0x2000000);openrac_api api{};
    Registry registry;View view;unsigned delta=0;int level=1;
    template<class T>void put(unsigned at,const T& value){std::memcpy(memory.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,memory.data()+at,sizeof value);return value;}
    unsigned address(unsigned base,unsigned stride=0)const{return base+delta+item*stride;}
    Fixture(){live=this;api.memory=memory.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=+[]{return live->level;};api.relocate_data=+[](unsigned at){return at+live->delta;};
        api.relocate_level_data=+[](int,unsigned at){return at+live->delta;};
        require(registry.register_weapon({RevolveratorKey,"noire342.revolverator",18,60037,10,15000,12}),"register custom descriptor");seed();}
    void seed(bool has_walloper=true){
        put<unsigned char>(address(owned,1),has_walloper?1:0);put<unsigned char>(address(unlocked,1),7);
        put<unsigned char>(address(acquired,1),2);put<int>(address(ammo,4),0);put<unsigned char>(address(variant,1),5);
        put<int>(hero+delta+0x10b8,8);put<int>(hero+delta+0x10b4,2);put<int>(mode+delta,0);
        put(favourites+delta,std::array<unsigned,8>{18,11,13,0,0,0,0,0});
        for(auto base:{definitions,overlay_definitions}){
            const unsigned at=address(base,76);
            put<int>(at,20037);put<int>(at+0xc,56);put<int>(at+0x10,180);put<int>(at+0x24,123456);
            put<unsigned short>(at+0x34,301);put<unsigned short>(at+0x36,302);
            put<unsigned short>(at+0x38,60018);put<unsigned short>(at+0x3a,0);
        }
        const unsigned table=address(prices,24);put<int>(table,7500);put<int>(table+4,6500);
        for(unsigned offset=8;offset<24;offset+=2)put<unsigned short>(table+offset,0);
        put<int>(address(vendor_slots,64)+0x30,225);
    }
    void custom_metadata(){
        for(auto base:{definitions,overlay_definitions}){
            const unsigned at=address(base,76);
            put<unsigned short>(at+0x34,32700);put<unsigned short>(at+0x36,32701);put<unsigned short>(at+0x3a,225);
        }
        const unsigned table=address(prices,24);put<int>(table,15000);
        put<unsigned short>(table+8,25);put<unsigned short>(table+10,25);put<unsigned short>(table+12,2);
        put<unsigned short>(table+14,10);put<unsigned short>(table+18,10);
        put<int>(address(vendor_slots,64)+0x30,2016);
    }
    void initialize(NativeArsenal& arsenal){arsenal.begin_level();custom_metadata();arsenal.finish_level();}
    void expect_native(const char* why){
        require(get<int>(address(prices,24))==7500&&get<unsigned short>(address(prices,24)+8)==0&&
                get<unsigned short>(address(prices,24)+14)==0&&get<unsigned short>(address(definitions,76)+0x38)==60018,why);
    }
    void expect_custom(const char* why){
        require(get<int>(address(prices,24))==15000&&get<unsigned short>(address(prices,24)+8)==25&&
                get<unsigned short>(address(prices,24)+14)==10&&get<unsigned short>(address(definitions,76)+0x38)==60037,why);
    }
};
}
int main(){
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(arsenal.ready()&&arsenal.projected()==18&&!f.view.custom,"unowned custom weapon leaves native presentation active after initialization");
     require(f.registry.walloper().owned&&f.registry.walloper().ammo==0&&f.registry.walloper().unlocked==0&&f.registry.walloper().acquired==2&&f.registry.walloper().text_variant==5,
             "native flags and original unlimited-melee ammo remain distinct");
     require(!f.registry.owned(RevolveratorKey)&&f.registry.state(RevolveratorKey)->ammo==0,"metadata initialization never grants custom ownership or ammo");
     f.expect_native("default finish_level restores the native7500/no-ammo/icon60018 metadata");
     require(f.get<unsigned char>(f.address(owned,1))==1&&f.get<int>(f.address(ammo,4))==0,"native Walloper inventory is restored after custom initialization");}
    {Fixture f;f.seed(false);NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(!f.registry.owned(18)&&!f.registry.owned(RevolveratorKey)&&f.get<unsigned char>(f.address(owned,1))==0,
             "a new campaign with neither weapon does not gain either one from a projection");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();
     require(f.registry.set_state(RevolveratorKey,{true,10,39})&&f.registry.equip(RevolveratorKey),"restore equipped custom identity before metadata initialization finishes");
     f.custom_metadata();arsenal.finish_level();
     f.expect_custom("saved equipped custom identity must apply all custom metadata including60037 even without an identity transition");
     require(f.view.custom&&arsenal.revolverator_active()==false,"view selects custom while actual hero still has wrench");
     f.put<int>(hero+0x10b8,18);require(arsenal.revolverator_active(),"logical equipped identity and native carrier are both required for combat activation");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(f.registry.set_state(RevolveratorKey,{true,10,39})&&f.registry.equip(RevolveratorKey),"grant a distinct owned custom weapon");
     arsenal.restore_gameplay();f.expect_custom("custom gameplay projects custom economy and icon");
     require(f.get<int>(f.address(ammo,4))==10&&f.get<unsigned char>(f.address(variant,1))==0&&f.registry.walloper().text_variant==5,
             "custom transport uses custom ammo and a neutral variant without editing stored native variant");
     f.put<int>(f.address(ammo,4),9);arsenal.commit();
     require(f.registry.state(RevolveratorKey)->ammo==9&&f.registry.walloper().ammo==0,"a native carrier shot reduces only custom ammunition");
     {
        ArsenalScope native(arsenal,18);f.expect_native("viewing Walloper temporarily projects only its native metadata");
        require(f.get<int>(f.address(ammo,4))==0&&f.get<unsigned char>(f.address(variant,1))==5&&!f.view.custom,
                "opposite UI view exposes the native inventory rather than custom carrier contents");
        f.put<unsigned char>(f.address(variant,1),6);
     }
     f.expect_custom("leaving the temporary native view restores custom gameplay");
     require(f.registry.walloper().text_variant==6&&f.registry.state(RevolveratorKey)->ammo==9&&f.get<int>(f.address(ammo,4))==9&&f.get<unsigned char>(f.address(variant,1))==0,
             "native UI changes commit to Walloper without contaminating the custom variant or ammo");
     require(f.registry.assign_quick(0,18)&&f.registry.assign_quick(1,RevolveratorKey)&&f.registry.assign_quick(2,11),"both weapons are assigned independently");
     arsenal.project_quick_slots();auto native=f.get<std::array<unsigned,8>>(favourites);
     require(native[0]==18&&native[1]==18&&native[2]==11&&f.registry.quick()[1]==RevolveratorKey,"gameplay projects two logical favourites through one bounded native carrier");
     {
        ArsenalScope saved(arsenal,18);arsenal.project_quick_slots(true);native=f.get<std::array<unsigned,8>>(favourites);
        require(native[0]==18&&native[1]==0&&native[2]==11&&f.registry.quick()[1]==RevolveratorKey,
                "native save filtering removes only custom carrier entries without erasing their logical favourites");
        require(f.get<int>(f.address(ammo,4))==0&&f.get<unsigned char>(f.address(variant,1))==6,"native save projection serializes actual Walloper state");
     }
     arsenal.project_quick_slots(false);native=f.get<std::array<unsigned,8>>(favourites);
     require(native[0]==18&&native[1]==18&&f.registry.equipped()==RevolveratorKey,"caller restores full gameplay favourites after native save scope");
     f.put<int>(f.address(definitions,76)+0x24,987654);f.put<int>(f.address(overlay_definitions,76)+0x24,987655);
     arsenal.project(18);arsenal.project(RevolveratorKey);
     require(f.get<int>(f.address(definitions,76)+0x24)==987654&&f.get<int>(f.address(overlay_definitions,76)+0x24)==987655,
             "metadata projection never touches unrelated animation/damage words outside its owned fields");
     require(f.get<int>(f.address(definitions,76)+0x10)==180&&f.get<int>(f.address(overlay_definitions,76)+0x10)==180,
             "native WAD class180 remains canonical in every identity");
     require(f.registry.equip(11),"switching to an unrelated native item remains valid");arsenal.restore_gameplay();f.expect_native("unrelated native gameplay restores the native Walloper definition");
     require(!f.view.custom&&f.registry.state(RevolveratorKey)->ammo==9&&f.registry.walloper().ammo==0,
             "switching to another weapon preserves both inactive inventories");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,10,0});
     f.registry.equip(RevolveratorKey);arsenal.restore_gameplay();f.put<int>(f.address(ammo,4),8);
     // Only the low byte differs from our last price. A byte-wise teardown
     // would incorrectly splice7500's upper byte into this owner's15001.
     f.put<int>(f.address(prices,24),15001);f.put<int>(f.address(definitions,76)+0x24,222222);arsenal.reset();
     require(!arsenal.ready()&&f.get<int>(f.address(prices,24))==15001,"reset preserves another owner's complete price field rather than splicing individual bytes");
     require(f.get<unsigned short>(f.address(prices,24)+8)==0&&f.get<unsigned short>(f.address(definitions,76)+0x38)==60018,
             "other unchanged owned fields still restore normally");
     require(f.get<int>(f.address(definitions,76)+0x24)==222222&&f.get<int>(f.address(ammo,4))==0&&f.registry.state(RevolveratorKey)->ammo==8,
             "reset commits custom counters, restores native counters and preserves unrelated owners");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,10,0});
     f.registry.equip(RevolveratorKey);arsenal.restore_gameplay();const auto old=f.memory;const auto calls=f.view.calls;
     f.level=2;f.delta=0x600000;arsenal.reset();
     require(f.memory==old&&f.view.calls==calls,"leaving an overlay never writes old tables or stale presentation state");
     f.seed();arsenal.begin_level();f.custom_metadata();arsenal.finish_level();
     require(arsenal.ready()&&f.registry.equipped()==RevolveratorKey&&f.get<int>(f.address(ammo,4))==10,
             "level change restores the independent custom equipped state with new relocated tables");
     f.expect_custom("new level has custom metadata even when the logical equipped key did not change");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,6,0});
     f.registry.equip(RevolveratorKey);f.put<int>(hero+0x10b8,11);f.put<int>(mode,3);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey,"paused menu state does not overwrite logical equipment");
     f.put<int>(mode,0);f.put<int>(0x141660,18);f.put<int>(hero+0x20b8,-1);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey,"consumed pending switch retains the logical key while the previous actor is held");
     f.put<int>(0x141660,11);arsenal.observe_equipped();require(f.registry.equipped()==11,"a completed native gameplay switch to another item is observed");}
    for(const char* version:{"pal","ntsc"}){
     Fixture f;f.api.version=version;const auto& region=*RegionBindings::for_version(version);
     const auto h=region.data(hero),equip=region.data(0x141660),m=region.level(0,mode),quick=region.data(favourites);
     NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();arsenal.finish_level();
     auto restore=[&]{
       auto snapshot=f.registry.snapshot();snapshot.equipped=RevolveratorKey;snapshot.quick={RevolveratorKey,0,0,0,0,0,0,0};
       for(auto& r:snapshot.records)if(r.key==RevolveratorKey)r.state={true,7,117,true};
       require(f.registry.restore(snapshot),"restore a Gold custom selection with earned XP and partial ammo");arsenal.mark_snapshot_restored();arsenal.restore_gameplay();
       f.put<int>(equip,8);f.put<int>(h+0x10b8,8);f.put<int>(h+0x20b8,-1);f.put<int>(h+0x22a8,4);f.put<int>(h+0x2084,0);f.put<int>(m,0);
     };
     auto body_ready=[&]{f.put<unsigned>(h+0x2080,0x800000);f.put<unsigned short>(0x8000a6,0);f.put<unsigned char>(0x800020,0);f.put<unsigned>(0x800024,0x810000);};
     restore();f.put<unsigned>(h+0x2080,0);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey&&f.get<int>(equip)==8&&f.get<int>(h+0x20b8)==-1,"missing player body keeps the restored logical selection pending");
     body_ready();f.put<int>(m,3);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey&&f.get<int>(equip)==8,"a paused scene cannot consume the restored selection");
     f.put<int>(m,0);f.put<int>(h+0x22a8,0);arsenal.observe_equipped();
     require(f.get<int>(equip)==8,"a dead player cannot receive a native equip request");
     f.put<int>(h+0x22a8,4);f.put<unsigned short>(0x8000a6,459);arsenal.observe_equipped();
     require(f.get<int>(equip)==8,"an enemy body cannot be treated as Ratchet");
     f.put<unsigned short>(0x8000a6,0);f.put<unsigned char>(0x800020,0x80);arsenal.observe_equipped();
     require(f.get<int>(equip)==8,"a retired Ratchet body cannot consume the pending selection");
     f.put<unsigned char>(0x800020,0);f.put<int>(h+0x2084,0x3d);arsenal.observe_equipped();
     require(f.get<int>(equip)==8,"death animation state also postpones restoration with positive health");
     f.put<int>(h+0x2084,0);f.level=2;const auto before=f.memory;arsenal.observe_equipped();
     require(f.memory==before&&f.registry.equipped()==RevolveratorKey,"a different loaded overlay cannot receive stale equip writes");
     f.level=1;arsenal.observe_equipped();
     require(f.get<int>(equip)==18&&f.get<int>(h+0x20b8)==18&&f.registry.equipped()==RevolveratorKey,"a safe same-level restore queues the saved carrier before observing the native wrench fallback");
     require(f.get<std::array<unsigned,8>>(quick)[0]==18&&f.registry.quick()[0]==RevolveratorKey,"restored logical favourites project back to the bounded native transport");
     const auto* state=f.registry.state(RevolveratorKey);
     require(state->ammo==7&&state->xp==117&&state->golden,"selection restoration does not grant, refill or alter earned XP/Gold");
     f.put<int>(h+0x20b8,-1);arsenal.observe_equipped();
     require(f.get<int>(h+0x20b8)==-1&&f.registry.equipped()==RevolveratorKey,"the queued selection is consumed once while the existing pending18 guard preserves identity");
     f.put<int>(h+0x10b8,18);f.put<int>(equip,18);arsenal.observe_equipped();require(arsenal.revolverator_active(),"completed native equip enables the original restored logical weapon");
     restore();body_ready();f.registry.equip(8);arsenal.observe_equipped();
     require(f.registry.equipped()==8&&f.get<int>(h+0x20b8)==-1,"an explicit different selection cancels the old restoration request");
     restore();body_ready();arsenal.reset();const auto cleared=f.memory;arsenal.observe_equipped();
     require(f.memory==cleared,"teardown discards pending restoration and never writes after clear");
     arsenal.begin_level();arsenal.finish_level();f.registry.equip(8);arsenal.mark_snapshot_restored();f.put<int>(equip,8);f.put<int>(h+0x20b8,-1);arsenal.observe_equipped();
     require(f.registry.equipped()==8&&f.get<int>(h+0x20b8)==-1,"a new native campaign snapshot never resurrects the old custom selection");
    }
    for(const char* version:{"pal","ntsc"}){
     Fixture f;f.api.version=version;const auto& region=*RegionBindings::for_version(version);
     const auto skill=region.data(unlocked)+item;
     f.put<unsigned char>(skill,0);
     NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();arsenal.finish_level();
     require(f.registry.set_state(RevolveratorKey,{true,10,0})&&f.registry.equip(RevolveratorKey),"grant custom item for regional skill-point isolation");
     arsenal.restore_gameplay();require(f.get<unsigned char>(skill)==0,"equipping a custom weapon must not grant the Pokitaru skill point");
     f.put<unsigned char>(skill,1);arsenal.project(18);arsenal.project(RevolveratorKey);
     require(f.get<unsigned char>(skill)==1,"skill point earned during a projected scope must remain earned");
     auto legacy=f.registry.walloper();legacy.unlocked=7;f.registry.set_walloper(legacy);
     arsenal.project(18);arsenal.reset();
     require(f.get<unsigned char>(skill)==1,"legacy sidecar and teardown must never restore an unrelated skill-point byte");
    }
    std::printf("native arsenal: %d ownership, projection, save-scope and teardown checks passed\n",checks);
}

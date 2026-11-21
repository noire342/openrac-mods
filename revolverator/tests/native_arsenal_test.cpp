// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal.hpp"
#include "native_arsenal_layout.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace revolverator;
using namespace arsenal;
namespace {
using namespace native_arsenal_layout;

constexpr unsigned skill_points = 0x13d408;
constexpr unsigned pending_head = 0x20c0;
constexpr unsigned pending_pack = 0x20c4;
constexpr unsigned item_display_name = 0;
constexpr unsigned item_description_text = 0xc;
constexpr unsigned unrelated_definition_word = 0x24;

constexpr int walloper_price = 7500;
constexpr int revolverator_price = 15000;
constexpr std::uint16_t walloper_icon_bank = 60018;
constexpr std::uint16_t expected_revolverator_icon_bank = 60037;
constexpr std::uint16_t no_ammo_price = 0;
constexpr std::uint16_t revolverator_ammo_price = 25;
constexpr std::uint16_t walloper_capacity = 0;
constexpr std::uint16_t revolverator_capacity = 10;
constexpr int walloper_ammo = 0; // The stock melee weapon has no rounds.
constexpr std::uint8_t walloper_variant = 5;
constexpr std::uint8_t edited_walloper_variant = 6;
constexpr int walloper_class = 180;
constexpr unsigned revolverator_xp = 39;
constexpr unsigned partially_spent_ammo = 6;
constexpr unsigned ammo_after_one_shot = revolverator_capacity - 1;

constexpr int pause_menu = 3;
constexpr int no_pending_item = -1;
constexpr unsigned alternate_weapon = 11;
constexpr unsigned secondary_favourite = 13;
constexpr int new_hand_item = 15;
constexpr int helmet_item = 7;
constexpr int pack_item = 2;

using QuickSlots = decltype(Snapshot::quick);

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
    unsigned address(unsigned base,unsigned stride=sizeof(std::uint8_t))const{
        return base+delta+NativeWalloperKey*stride;
    }
    unsigned ammo_address()const{return address(weapon_ammo_counts,sizeof(std::int32_t));}
    unsigned vendor_field(unsigned field)const{return address(vendor_item_info,vendor_item_info_size)+field;}
    Fixture(){live=this;api.memory=memory.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=+[]{return live->level;};api.relocate_data=+[](unsigned at){return at+live->delta;};
        api.relocate_level_data=+[](int,unsigned at){return at+live->delta;};
        require(registry.register_weapon({RevolveratorKey,"noire342.revolverator",NativeWalloperKey,expected_revolverator_icon_bank,
                                          revolverator_capacity,revolverator_price,12}),"register custom descriptor");seed();}
    void seed(bool has_walloper=true){
        put<std::uint8_t>(address(items_owned),has_walloper);
        put<std::uint8_t>(address(skill_points),7);
        put<std::uint8_t>(address(items_acquired),2);
        put(ammo_address(),walloper_ammo);
        put(address(item_text_variant),walloper_variant);
        put<int>(hero+delta+held_item,NativeWrenchKey);
        put(hero+delta+held_item_state,held_item_ready);
        put(game_mode+delta,gameplay_mode);
        put(quick_slots+delta,QuickSlots{NativeWalloperKey,alternate_weapon,secondary_favourite});
        for(auto base:{resident_item_definitions,level_item_definitions}){
            const auto at=address(base,item_definition_size);
            put<int>(at+item_display_name,20037);
            put<int>(at+item_description_text,56);
            put(at+item_definition_class,walloper_class);
            put<int>(at+unrelated_definition_word,123456);
            put<std::uint16_t>(at+pickup_plural_text,301);
            put<std::uint16_t>(at+pickup_singular_text,302);
            put(at+item_icon_bank,walloper_icon_bank);
            put<std::uint16_t>(at+ammo_pickup_class,0);
        }
        put(vendor_field(vendor_weapon_price),walloper_price);
        put<int>(vendor_field(discounted_weapon_price),6500);
        for(auto field=ammo_unit_price;field<vendor_item_info_size;field+=sizeof(std::uint16_t))
            put<std::uint16_t>(vendor_field(field),0);
        put<int>(address(vendor_item_slots,vendor_item_slot_size)+vendor_preview_class,225);
    }
    void custom_metadata(){
        for(auto base:{resident_item_definitions,level_item_definitions}){
            const auto at=address(base,item_definition_size);
            put<std::uint16_t>(at+pickup_plural_text,32700);
            put<std::uint16_t>(at+pickup_singular_text,32701);
            put<std::uint16_t>(at+ammo_pickup_class,225);
        }
        put(vendor_field(vendor_weapon_price),revolverator_price);
        put(vendor_field(ammo_unit_price),revolverator_ammo_price);
        put(vendor_field(challenge_ammo_unit_price),revolverator_ammo_price);
        put<std::uint16_t>(vendor_field(ammo_pickup_amount),2);
        put(vendor_field(ammo_capacity),revolverator_capacity);
        put(vendor_field(initial_ammo),revolverator_capacity);
        put<int>(address(vendor_item_slots,vendor_item_slot_size)+vendor_preview_class,2016);
    }
    void initialize(NativeArsenal& arsenal){arsenal.begin_level();custom_metadata();arsenal.finish_level();}
    void expect_native(const char* why){
        require(get<int>(vendor_field(vendor_weapon_price))==walloper_price&&
                get<std::uint16_t>(vendor_field(ammo_unit_price))==no_ammo_price&&
                get<std::uint16_t>(vendor_field(ammo_capacity))==walloper_capacity&&
                get<std::uint16_t>(address(resident_item_definitions,item_definition_size)+item_icon_bank)==walloper_icon_bank,why);
    }
    void expect_custom(const char* why){
        require(get<int>(vendor_field(vendor_weapon_price))==revolverator_price&&
                get<std::uint16_t>(vendor_field(ammo_unit_price))==revolverator_ammo_price&&
                get<std::uint16_t>(vendor_field(ammo_capacity))==revolverator_capacity&&
                get<std::uint16_t>(address(resident_item_definitions,item_definition_size)+item_icon_bank)==expected_revolverator_icon_bank,why);
    }
};

int grant_calls, grant_id, grant_equip, native_slot;
void original_grant(int id, int equip) {
    ++grant_calls;
    grant_id = id;
    grant_equip = equip;
    auto& f = *live;
    if (f.get<unsigned char>(items_owned + id)) return;
    f.put<unsigned char>(items_owned + id, 1);
    if (equip && native_slot >= 0) f.put<int>(hero + unsigned(native_slot), id);
    if (native_slot != int(pending_hand)) return;
    auto slots = f.get<QuickSlots>(quick_slots);
    for (auto& slot : slots) {
        if (slot != 0) continue;
        slot = id;
        break;
    }
    f.put(quick_slots, slots);
}

void check_item_grants() {
    Fixture f;
    f.seed(false);
    NativeArsenal arsenal(&f.api, f.registry, f.view);
    f.initialize(arsenal);
    require(f.registry.set_state(RevolveratorKey, {true, partially_spent_ammo, revolverator_xp}) &&
            f.registry.equip(RevolveratorKey) && f.registry.assign_quick(0, RevolveratorKey),
            "fixture starts with an owned, equipped and bound Revolverator");
    arsenal.restore_gameplay();
    arsenal.project_quick_slots();
    const auto revision = f.registry.revision();
    for (int id = 0; id <= int(Registry::MaximumNativeKey); ++id) {
        if (id == int(NativeWalloperKey)) continue;
        // Exercise non-hand grants, including slot-2 Pilot's Helmet on Gaspar.
        native_slot = id == helmet_item ? int(pending_head) : id == pack_item ? int(pending_pack) : no_pending_item;
        const int queued_hand = new_hand_item;
        f.put<int>(hero + pending_hand, queued_hand);
        f.put<int>(hero + pending_head, no_pending_item);
        f.put<int>(hero + pending_pack, no_pending_item);
        grant_calls = 0;
        arsenal.give_item(id, 1, false, original_grant);
        require(grant_calls == 1 && grant_id == id && grant_equip == 1, "native grant is forwarded once with original arguments");
        require(f.get<int>(hero + pending_hand) == queued_hand &&
                f.get<int>(hero + pending_head) == (id == helmet_item ? helmet_item : no_pending_item) &&
                f.get<int>(hero + pending_pack) == (id == pack_item ? pack_item : no_pending_item),
                "non-hand grants preserve the pending hand and every native equipment slot");
        require(f.registry.revision() == revision && f.registry.equipped() == RevolveratorKey &&
                f.view.custom && f.get<unsigned char>(items_owned + id) == 1,
                "unrelated acquisition leaves the projected weapon and logical inventory alone");
    }

    native_slot = pending_hand;
    f.put<unsigned char>(items_owned + new_hand_item, 0);
    arsenal.give_item(new_hand_item, 1, false, original_grant);
    require(f.get<int>(hero + pending_hand) == new_hand_item && f.registry.quick()[3] == new_hand_item &&
            f.registry.quick()[0] == RevolveratorKey,
            "native hand acquisition queues itself and inserts its own favourite");
    f.put<int>(hero + pending_hand, alternate_weapon);
    arsenal.give_item(new_hand_item, 1, false, original_grant);
    require(f.get<int>(hero + pending_hand) == alternate_weapon, "an owned native weapon grant leaves an existing hand request alone");

    arsenal.give_item(NativeWalloperKey, 1, false, original_grant);
    arsenal.restore_gameplay();
    require(f.registry.walloper().owned && f.registry.equipped() == NativeWalloperKey &&
            f.registry.quick()[4] == NativeWalloperKey && f.registry.state(RevolveratorKey)->ammo == partially_spent_ammo,
            "native Walloper grant keeps its ownership, selection and favourite independent");
    f.put<int>(hero + pending_hand, new_hand_item);
    arsenal.give_item(NativeWalloperKey, 1, false, original_grant);
    require(f.get<int>(hero + pending_hand) == new_hand_item, "an owned Walloper grant follows native regrant semantics");
    arsenal.give_item(NativeWalloperKey, 1, true, original_grant);
    require(f.get<int>(hero + pending_hand) == NativeWalloperKey && f.registry.equipped() == RevolveratorKey &&
            f.registry.state(RevolveratorKey)->ammo == partially_spent_ammo && f.registry.state(RevolveratorKey)->xp == revolverator_xp,
            "an explicit Rotatore regrant equips the custom weapon and preserves ammo and XP");
}
}
int main(){
    check_item_grants();
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(arsenal.ready()&&arsenal.projected()==NativeWalloperKey&&!f.view.custom,"unowned custom weapon leaves native presentation active after initialization");
     require(f.registry.walloper().owned&&f.registry.walloper().ammo==walloper_ammo&&f.registry.walloper().unlocked==0&&f.registry.walloper().acquired==2&&f.registry.walloper().text_variant==walloper_variant,
             "native flags and original unlimited-melee ammo remain distinct");
     require(!f.registry.owned(RevolveratorKey)&&f.registry.state(RevolveratorKey)->ammo==0,"metadata initialization never grants custom ownership or ammo");
     f.expect_native("default finish_level restores the native Walloper price, capacity and icon");
     require(f.get<unsigned char>(f.address(items_owned))==1&&f.get<int>(f.ammo_address())==walloper_ammo,"native Walloper inventory is restored after custom initialization");}
    {Fixture f;f.seed(false);NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(!f.registry.owned(NativeWalloperKey)&&!f.registry.owned(RevolveratorKey)&&f.get<unsigned char>(f.address(items_owned))==0,
             "a new campaign with neither weapon does not gain either one from a projection");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();
     require(f.registry.set_state(RevolveratorKey,{true,revolverator_capacity,revolverator_xp})&&f.registry.equip(RevolveratorKey),"restore equipped custom identity before metadata initialization finishes");
     f.custom_metadata();arsenal.finish_level();
     f.expect_custom("saved custom selection restores the Revolverator price, capacity and icon without an identity transition");
     require(f.view.custom&&arsenal.revolverator_active()==false,"view selects custom while actual hero still has wrench");
     f.put<int>(hero+held_item,NativeWalloperKey);require(arsenal.revolverator_active(),"logical equipped identity and native carrier are both required for combat activation");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);
     require(f.registry.set_state(RevolveratorKey,{true,revolverator_capacity,revolverator_xp})&&f.registry.equip(RevolveratorKey),"grant a distinct owned custom weapon");
     arsenal.restore_gameplay();f.expect_custom("custom gameplay projects custom economy and icon");
     require(f.get<int>(f.ammo_address())==revolverator_capacity&&f.get<unsigned char>(f.address(item_text_variant))==0&&f.registry.walloper().text_variant==walloper_variant,
             "custom transport uses custom ammo and a neutral variant without editing stored native variant");
     f.put<int>(f.ammo_address(),ammo_after_one_shot);arsenal.commit();
     require(f.registry.state(RevolveratorKey)->ammo==ammo_after_one_shot&&f.registry.walloper().ammo==walloper_ammo,"a native carrier shot reduces only custom ammunition");
     {
        ArsenalScope native(arsenal,NativeWalloperKey);f.expect_native("viewing Walloper temporarily projects only its native metadata");
        require(f.get<int>(f.ammo_address())==walloper_ammo&&f.get<unsigned char>(f.address(item_text_variant))==walloper_variant&&!f.view.custom,
                "opposite UI view exposes the native inventory rather than custom carrier contents");
        f.put<unsigned char>(f.address(item_text_variant),edited_walloper_variant);
     }
     f.expect_custom("leaving the temporary native view restores custom gameplay");
     require(f.registry.walloper().text_variant==edited_walloper_variant&&f.registry.state(RevolveratorKey)->ammo==ammo_after_one_shot&&f.get<int>(f.ammo_address())==ammo_after_one_shot&&f.get<unsigned char>(f.address(item_text_variant))==0,
             "native UI changes commit to Walloper without contaminating the custom variant or ammo");
     require(f.registry.assign_quick(0,NativeWalloperKey)&&f.registry.assign_quick(1,RevolveratorKey)&&f.registry.assign_quick(2,alternate_weapon),"both weapons are assigned independently");
     arsenal.project_quick_slots();auto native=f.get<QuickSlots>(quick_slots);
     require(native[0]==NativeWalloperKey&&native[1]==NativeWalloperKey&&native[2]==alternate_weapon&&f.registry.quick()[1]==RevolveratorKey,"gameplay projects two logical favourites through one bounded native carrier");
     {
        ArsenalScope saved(arsenal,NativeWalloperKey);arsenal.project_quick_slots(true);native=f.get<QuickSlots>(quick_slots);
        require(native[0]==NativeWalloperKey&&native[1]==0&&native[2]==alternate_weapon&&f.registry.quick()[1]==RevolveratorKey,
                "native save filtering removes only custom carrier entries without erasing their logical favourites");
        require(f.get<int>(f.ammo_address())==walloper_ammo&&f.get<unsigned char>(f.address(item_text_variant))==edited_walloper_variant,"native save projection serializes actual Walloper state");
     }
     arsenal.project_quick_slots(false);native=f.get<QuickSlots>(quick_slots);
     require(native[0]==NativeWalloperKey&&native[1]==NativeWalloperKey&&f.registry.equipped()==RevolveratorKey,"caller restores full gameplay favourites after native save scope");
     f.put<int>(f.address(resident_item_definitions,item_definition_size)+unrelated_definition_word,987654);f.put<int>(f.address(level_item_definitions,item_definition_size)+unrelated_definition_word,987655);
     arsenal.project(NativeWalloperKey);arsenal.project(RevolveratorKey);
     require(f.get<int>(f.address(resident_item_definitions,item_definition_size)+unrelated_definition_word)==987654&&f.get<int>(f.address(level_item_definitions,item_definition_size)+unrelated_definition_word)==987655,
             "metadata projection never touches unrelated animation/damage words outside its owned fields");
     require(f.get<int>(f.address(resident_item_definitions,item_definition_size)+item_definition_class)==walloper_class&&f.get<int>(f.address(level_item_definitions,item_definition_size)+item_definition_class)==walloper_class,
             "the original Walloper Moby class remains canonical in every identity");
     require(f.registry.equip(alternate_weapon),"switching to an unrelated native item remains valid");arsenal.restore_gameplay();f.expect_native("unrelated native gameplay restores the native Walloper definition");
     require(!f.view.custom&&f.registry.state(RevolveratorKey)->ammo==ammo_after_one_shot&&f.registry.walloper().ammo==walloper_ammo,
             "switching to another weapon preserves both inactive inventories");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,revolverator_capacity,0});
     f.registry.equip(RevolveratorKey);arsenal.restore_gameplay();f.put<int>(f.ammo_address(),8);
     // Only the low byte differs from our last price. A byte-wise teardown
     // would incorrectly splice7500's upper byte into this owner's15001.
     f.put<int>(f.vendor_field(vendor_weapon_price),15001);f.put<int>(f.address(resident_item_definitions,item_definition_size)+unrelated_definition_word,222222);arsenal.reset();
     require(!arsenal.ready()&&f.get<int>(f.vendor_field(vendor_weapon_price))==15001,"reset preserves another owner's complete price field rather than splicing individual bytes");
     require(f.get<std::uint16_t>(f.vendor_field(ammo_unit_price))==no_ammo_price&&f.get<std::uint16_t>(f.address(resident_item_definitions,item_definition_size)+item_icon_bank)==walloper_icon_bank,
             "other unchanged owned fields still restore normally");
     require(f.get<int>(f.address(resident_item_definitions,item_definition_size)+unrelated_definition_word)==222222&&f.get<int>(f.ammo_address())==walloper_ammo&&f.registry.state(RevolveratorKey)->ammo==8,
             "reset commits custom counters, restores native counters and preserves unrelated owners");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,revolverator_capacity,0});
     f.registry.equip(RevolveratorKey);arsenal.restore_gameplay();const auto old=f.memory;const auto calls=f.view.calls;
     f.level=2;f.delta=0x600000;arsenal.reset();
     require(f.memory==old&&f.view.calls==calls,"leaving an overlay never writes old tables or stale presentation state");
     f.seed();arsenal.begin_level();f.custom_metadata();arsenal.finish_level();
     require(arsenal.ready()&&f.registry.equipped()==RevolveratorKey&&f.get<int>(f.ammo_address())==revolverator_capacity,
             "level change restores the independent custom equipped state with new relocated tables");
     f.expect_custom("new level has custom metadata even when the logical equipped key did not change");}
    {Fixture f;NativeArsenal arsenal(&f.api,f.registry,f.view);f.initialize(arsenal);f.registry.set_state(RevolveratorKey,{true,partially_spent_ammo,0});
     f.registry.equip(RevolveratorKey);f.put<int>(hero+held_item,alternate_weapon);f.put<int>(game_mode,pause_menu);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey,"paused menu state does not overwrite logical equipment");
     f.put<int>(game_mode,gameplay_mode);f.put<int>(hand_selection,NativeWalloperKey);f.put<int>(hero+pending_hand,no_pending_item);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey,"consumed pending switch retains the logical key while the previous actor is held");
     f.put<int>(hand_selection,alternate_weapon);arsenal.observe_equipped();require(f.registry.equipped()==alternate_weapon,"a completed native gameplay switch to another item is observed");}
    for(const char* version:{"pal","ntsc"}){
     Fixture f;f.api.version=version;const auto& region=*RegionBindings::for_version(version);
     const auto h=region.data(hero),equip=region.data(hand_selection),m=region.level(0,game_mode),quick=region.data(quick_slots);
     NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();arsenal.finish_level();
     constexpr State saved_gold{true,7,117,true};
     constexpr unsigned body_address = 0x800000;
     constexpr unsigned body_class_address = 0x810000;
     constexpr std::uint16_t enemy_class = 459;
     constexpr std::uint8_t body_active = 0;
     constexpr int live_health = 4;
     constexpr int hero_idle = 0;
     auto restore=[&]{
       auto snapshot=f.registry.snapshot();snapshot.equipped=RevolveratorKey;snapshot.quick={RevolveratorKey};
       for(auto& r:snapshot.records)if(r.key==RevolveratorKey)r.state=saved_gold;
       require(f.registry.restore(snapshot),"restore a Gold custom selection with earned XP and partial ammo");arsenal.mark_snapshot_restored();arsenal.restore_gameplay();
       f.put<int>(equip,NativeWrenchKey);f.put<int>(h+held_item,NativeWrenchKey);f.put<int>(h+pending_hand,no_pending_item);f.put<int>(h+hero_health,live_health);f.put<int>(h+hero_state,hero_idle);f.put<int>(m,gameplay_mode);
     };
     auto body_ready=[&]{f.put(h+hero_moby,body_address);f.put(body_address+moby_class,ratchet_class);f.put(body_address+moby_state,body_active);f.put(body_address+moby_class_pointer,body_class_address);};
     restore();f.put<unsigned>(h+hero_moby,0);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey&&f.get<int>(equip)==NativeWrenchKey&&f.get<int>(h+pending_hand)==no_pending_item,"missing player body keeps the restored logical selection pending");
     body_ready();f.put<int>(m,pause_menu);arsenal.observe_equipped();
     require(f.registry.equipped()==RevolveratorKey&&f.get<int>(equip)==NativeWrenchKey,"a paused scene cannot consume the restored selection");
     f.put<int>(m,gameplay_mode);f.put<int>(h+hero_health,0);arsenal.observe_equipped();
     require(f.get<int>(equip)==NativeWrenchKey,"a dead player cannot receive a native equip request");
     f.put<int>(h+hero_health,live_health);f.put(body_address+moby_class,enemy_class);arsenal.observe_equipped();
     require(f.get<int>(equip)==NativeWrenchKey,"an enemy body cannot be treated as Ratchet");
     f.put(body_address+moby_class,ratchet_class);f.put(body_address+moby_state,moby_unavailable_state);arsenal.observe_equipped();
     require(f.get<int>(equip)==NativeWrenchKey,"a retired Ratchet body cannot consume the pending selection");
     f.put(body_address+moby_state,body_active);f.put<int>(h+hero_state,hero_dying);arsenal.observe_equipped();
     require(f.get<int>(equip)==NativeWrenchKey,"death animation state also postpones restoration with positive health");
     f.put<int>(h+hero_state,hero_idle);f.level=2;const auto before=f.memory;arsenal.observe_equipped();
     require(f.memory==before&&f.registry.equipped()==RevolveratorKey,"a different loaded overlay cannot receive stale equip writes");
     f.level=1;arsenal.observe_equipped();
     require(f.get<int>(equip)==NativeWalloperKey&&f.get<int>(h+pending_hand)==NativeWalloperKey&&f.registry.equipped()==RevolveratorKey,"a safe same-level restore queues the saved carrier before observing the native wrench fallback");
     require(f.get<QuickSlots>(quick)[0]==NativeWalloperKey&&f.registry.quick()[0]==RevolveratorKey,"restored logical favourites project back to the bounded native transport");
     const auto* state=f.registry.state(RevolveratorKey);
     require(state->ammo==saved_gold.ammo&&state->xp==saved_gold.xp&&state->golden,"selection restoration does not grant, refill or alter earned XP/Gold");
     f.put<int>(h+pending_hand,no_pending_item);arsenal.observe_equipped();
     require(f.get<int>(h+pending_hand)==no_pending_item&&f.registry.equipped()==RevolveratorKey,"the queued selection is consumed once while the native carrier guard preserves the logical identity");
     f.put<int>(h+held_item,NativeWalloperKey);f.put<int>(equip,NativeWalloperKey);arsenal.observe_equipped();require(arsenal.revolverator_active(),"completed native equip enables the original restored logical weapon");

     // The native hand switch consumes the request when it matches its cached
     // selection. A checkpoint can retain the carrier in that cache while
     // restoring the wrench or leaving the hand empty.
     constexpr unsigned native_equipment_slots = 4;
     using SelectedItems = std::array<int,native_equipment_slots>;
     constexpr SelectedItems cached_items{NativeWalloperKey,secondary_favourite,helmet_item,pack_item};
     const auto selected_items_address = h+hero_selected_item;
     for(int held:{int(NativeWrenchKey),empty_hand_item,int(alternate_weapon)}){
         restore();
         body_ready();
         f.put<int>(h+held_item,held);
         f.put<int>(equip,NativeWalloperKey);
         f.put(selected_items_address,cached_items);

         arsenal.observe_equipped();
         auto expected_items = cached_items;
         if(held!=int(alternate_weapon))expected_items.front()=held;
         require(f.get<SelectedItems>(selected_items_address)==expected_items,
                 "checkpoint re-arming updates only a stale cached hand selection with a wrench or empty hand");
         require(f.get<int>(h+pending_hand)==NativeWalloperKey&&f.get<int>(equip)==NativeWalloperKey&&
                 f.registry.equipped()==RevolveratorKey,
                 "checkpoint re-arming queues the native carrier and keeps the logical Revolverator selection");
         const auto* restored=f.registry.state(RevolveratorKey);
         require(restored->ammo==saved_gold.ammo&&restored->xp==saved_gold.xp&&restored->golden,
                 "checkpoint re-arming preserves partial ammunition, XP and Gold");
     }

     restore();body_ready();f.registry.equip(NativeWrenchKey);arsenal.observe_equipped();
     require(f.registry.equipped()==NativeWrenchKey&&f.get<int>(h+pending_hand)==no_pending_item,"an explicit different selection cancels the old restoration request");
     restore();body_ready();arsenal.reset();const auto cleared=f.memory;arsenal.observe_equipped();
     require(f.memory==cleared,"teardown discards pending restoration and never writes after clear");
     arsenal.begin_level();arsenal.finish_level();f.registry.equip(NativeWrenchKey);arsenal.mark_snapshot_restored();f.put<int>(equip,NativeWrenchKey);f.put<int>(h+pending_hand,no_pending_item);arsenal.observe_equipped();
     require(f.registry.equipped()==NativeWrenchKey&&f.get<int>(h+pending_hand)==no_pending_item,"a new native campaign snapshot never resurrects the old custom selection");
    }
    for(const char* version:{"pal","ntsc"}){
     Fixture f;f.api.version=version;const auto& region=*RegionBindings::for_version(version);
     const auto skill=region.data(skill_points)+NativeWalloperKey;
     f.put<unsigned char>(skill,0);
     NativeArsenal arsenal(&f.api,f.registry,f.view);arsenal.begin_level();arsenal.finish_level();
     require(f.registry.set_state(RevolveratorKey,{true,revolverator_capacity,0})&&f.registry.equip(RevolveratorKey),"grant custom item for regional skill-point isolation");
     arsenal.restore_gameplay();require(f.get<unsigned char>(skill)==0,"equipping a custom weapon must not grant the Pokitaru skill point");
     f.put<unsigned char>(skill,1);arsenal.project(NativeWalloperKey);arsenal.project(RevolveratorKey);
     require(f.get<unsigned char>(skill)==1,"skill point earned during a projected scope must remain earned");
     auto legacy=f.registry.walloper();legacy.unlocked=7;f.registry.set_walloper(legacy);
     arsenal.project(NativeWalloperKey);arsenal.reset();
     require(f.get<unsigned char>(skill)==1,"legacy sidecar and teardown must never restore an unrelated skill-point byte");
    }
    std::printf("native arsenal: %d ownership, projection, save-scope and teardown checks passed\n",checks);
}

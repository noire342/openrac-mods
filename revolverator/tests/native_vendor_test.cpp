// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_vendor.hpp"
#include "arsenal_state.hpp"
#include "native_weapon_alias.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

using namespace revolverator;
namespace {
int current_level=1,checks=0;
int loaded(){return current_level;}
unsigned relocate(unsigned at){return at;}
unsigned relocate_level(int,unsigned at){return at;}
void require(bool condition,const char* message){
    ++checks;if(!condition){std::fprintf(stderr,"vendor: %s\n",message);std::exit(1);}
}
struct Row{int item,is_ammo,state,unused,sound;};
constexpr unsigned price=0x1c40b0+18*24,menu=0x1ca4c0,owned=0x13d4c0;
constexpr unsigned visit=0x13dd58+12,stock=0x15edd0;
constexpr unsigned preview_list=0x1f00000,preview_model=0x1c8ff0+18*0x40+0x30;
constexpr unsigned monitor=preview_list+0x100,hologram=preview_list+0x300;
struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x2000000);
    openrac_api api{};
    NativeWeaponAlias weapon_alias{&api};
    GuestArena arena;
    Fixture(){
        current_level=1;api.memory=ram.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=loaded;api.relocate_data=relocate;api.relocate_level_data=relocate_level;
        put(price,7500);put(price+4,5000);put<std::uint16_t>(price+8,25);put<std::uint16_t>(price+10,25);
        put<std::uint16_t>(price+12,4);put<std::uint16_t>(price+14,20);put<std::uint16_t>(price+18,20);
        put(0x1c2108+18*20,20888);put<std::uint8_t>(owned+10,1);
        put(stock,std::array<std::uint8_t,12>{0x4a,18,15,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff});
    }
    template<class T>void put(unsigned address,T value){std::memcpy(ram.data()+address,&value,sizeof value);}
    template<class T>T get(unsigned address)const{T v{};std::memcpy(&v,ram.data()+address,sizeof v);return v;}
    void rows(std::initializer_list<Row> values,int selection=0){
        std::array<Row,16> rows{};unsigned i=0;for(auto row:values)rows[i++]=row;
        put(menu+0xd0,rows);put(menu+0x210,int(values.size()));put(menu+0x58,selection);
    }
    Row row(unsigned i)const{return get<Row>(menu+0xd0+i*20);}
    int count()const{return get<int>(menu+0x210);}
    std::vector<std::uint8_t> inventory()const{return {ram.begin()+0x13d388,ram.begin()+0x13d550};}
    std::array<std::uint8_t,12> saved_stock()const{return get<std::array<std::uint8_t,12>>(stock);}
    void previews(){
        put(menu+0x20,preview_list);put(menu+8,1);put(0x15f5c4,5);
        for(unsigned i=0;i<6;++i){
            const unsigned moby=preview_list+i*0x100;
            put<std::uint8_t>(moby+0x20,1);put(moby+0x24,0x120000+i*0x80);
            put(moby+0x78,0x130000+i*0x40);put(moby+0x38,100+i);
            put<std::uint16_t>(moby+0xa6,i==2?225:(i==1||i==3?180:13));
        }
    }
    void weapon_resources(){
        constexpr unsigned resource=0x120080, slot=0;
        for(const unsigned id:{180u,2017u,2018u})put<std::uint8_t>(0x1b3ac0+id,0xff);
        put<std::uint8_t>(0x1b3ac0+180,slot);put<std::int16_t>(0x1b3900+slot*2,180);
        put(0x15ff00,1);put(0x1b3200+slot*4,resource);put<std::uint8_t>(resource+8,5);
        put(hologram+0x24,resource);
        constexpr unsigned skeleton=0x140000;
        put(resource+0x14,skeleton);put(resource+0x24,0.1f);
        const std::array<float,16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        for(unsigned joint=0;joint<5;++joint)put(skeleton+joint*64,identity);
        arena.reserve(0x150000,1024);
    }
};
}
int main(){
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);
     constexpr unsigned scene=0x16c860;
     f.rows({{18,0,0,0,0},{18,0,0,0x52564f31,0}},1);f.previews();vendor.frame();
     f.put(menu,1);
     require(vendor.weapon_purchase_key(18)==0,"highlighting a row cannot reinterpret an unrelated grant");
     f.put(menu+0x5c,4);f.put(menu+0x40,1);
     require(vendor.weapon_purchase_key(18)==0,"the PDA is not the physical weapon vendor");
     f.put(menu+0x40,0);
     require(vendor.weapon_purchase_key(7)==0,"a matching purchase phase cannot capture a story gadget grant");
     require(vendor.weapon_purchase_key(18)==arsenal::RevolveratorKey,"only the confirmed marked row identifies a drill purchase");
     vendor.remember_weapon_purchase(vendor.weapon_purchase_key(18));
     f.put(menu+0x48,18);f.put(menu+0x58,0);f.put(menu,3);
     f.put<short>(scene+0x44,2);f.put(scene+0x178,preview_list);f.put(scene+0x17c,monitor);
     require(vendor.purchase_scene_weapon()==monitor,"the confirmed purchase owns the scene weapon after a cursor change");
     f.put(menu,1);vendor.remember_weapon_purchase(vendor.weapon_purchase_key(18));f.put(menu,3);
     require(vendor.purchase_scene_weapon()==0,"a subsequent stock purchase does not borrow the custom scene");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);const auto inventory=f.inventory();const auto stock_before=f.saved_stock();
     const auto metadata=f.get<std::array<std::uint8_t,24>>(price);
     f.rows({{10,1,0,12,101},{18,0,0,13,102},{15,0,0,14,103}},2);
     vendor.frame();vendor.after_build(0);
     require(f.get<int>(price)==15000,"normal price must be 15000");
     require(f.get<int>(price+4)==5000,"native discounted purchase price must remain untouched");
     const auto now=f.get<std::array<std::uint8_t,24>>(price);
     require(std::memcmp(metadata.data()+4,now.data()+4,20)==0,"ammo metadata must stay under NativeAmmunition ownership");
     require(f.count()==2&&f.row(0).item==10&&f.row(1).item==15,"before Hoven, hide only the unowned Revolverator row");
     require(f.row(1).unused==14&&f.row(1).sound==103,"unrelated row fields must survive compaction");
     require(f.get<int>(menu+0x58)==1,"selection must continue to identify the same retained item");
     require(f.inventory()==inventory&&f.saved_stock()==stock_before,"vendor display must not grant items or rewrite saved stock");
     vendor.reset();require(f.get<int>(price)==7500,"owned price patch must be restored");
     require(f.count()==3&&f.row(1).item==18&&f.get<int>(menu+0x58)==2,"unchanged transient list patch must restore on reset");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(0x13dd40+12,1);
     f.rows({{18,0,0,0,102},{15,0,0,0,103}});vendor.after_build(0);
     require(!vendor.unlocked()&&f.count()==1&&f.row(0).item==15,"map unlock alone must not count as visiting Hoven");}
    {Fixture f;current_level=12;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{15,0,0,0,103}});
     const auto inv=f.inventory();vendor.before_build();vendor.after_build(0);
     require(vendor.unlocked()&&f.count()==2&&f.row(1).item==18&&f.row(1).is_ammo==0,"first Hoven entry must offer the unowned weapon");
     require(f.row(1).sound==20888,"new row must use slot18 native vendor sound metadata");
     require(f.inventory()==inv,"Hoven availability must not auto-grant the weapon");}
    for(std::uint8_t visited:{std::uint8_t(1),std::uint8_t(2)}){
     Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put(visit,visited);f.rows({{15,0,0,0,103}});
     vendor.after_build(0);require(vendor.unlocked()&&f.count()==2&&f.row(1).item==18,"after visiting Hoven, other normal vendors must offer the weapon");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(visit,1);f.rows({{18,0,0,0,102},{10,1,0,0,101}});
     vendor.after_build(1);require(f.count()==1&&f.row(0).item==10,"ammo-only lists must not sell an unowned weapon even after Hoven");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(owned+18,1);f.rows({{10,1,0,0,101}});
     const auto inv=f.inventory();vendor.after_build(1);
     require(f.count()==2&&f.row(1).item==18&&f.row(1).is_ammo==1,"pre-existing ownership must retain ammo access before Hoven");
     require(f.inventory()==inv,"existing ownership and ammo counters must not be changed");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(owned+18,1);f.put<std::uint16_t>(price+8,0);f.rows({{10,1,0,0,101}});
     vendor.after_build(0);require(f.count()==1,"an owned weapon without ammo metadata must not get a fabricated purchase row");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(visit,1);
     f.rows({{18,1,0,0,102},{15,0,0,1,103},{18,0,0,2,104}},1);vendor.after_build(0);
     require(f.count()==2&&f.row(0).item==18&&f.row(0).is_ammo==0&&f.row(1).item==15,"deduplicate only slot18 and repair its unowned view");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.put<std::uint8_t>(visit,1);std::array<Row,16> rows{};
     for(int i=0;i<16;++i)rows[unsigned(i)]={100+i,0,0,i,200+i};
     f.put(menu+0xd0,rows);f.put(menu+0x210,16);vendor.after_build(0);
     require(f.count()==16&&std::memcmp(rows.data(),f.ram.data()+menu+0xd0,sizeof rows)==0,"full vendor list must not evict another item or overflow");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,0,0,0,102}});f.put(menu+0x210,17);vendor.after_build(0);
     require(f.count()==17&&f.row(0).item==18,"invalid native list count must fail closed");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);vendor.frame();f.put(price,20000);vendor.reset();
     require(f.get<int>(price)==20000,"reset must preserve a later price change made by another owner");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);vendor.frame();current_level=12;f.put(price,12345);vendor.frame();
     require(f.get<int>(price)==15000,"new level metadata must receive the new price");vendor.reset();
     require(f.get<int>(price)==12345,"new-level restoration must not use the old overlay's original price");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{10,1,0,0,101},{18,1,0,0,102}},1);f.previews();
     const auto inventory=f.inventory();const unsigned ammo=preview_list+0x200;
     const auto pclass=f.get<unsigned>(ammo+0x24),pvars=f.get<unsigned>(ammo+0x78),stamp=f.get<unsigned>(ammo+0x38);
     vendor.frame();require(f.get<int>(preview_model)==225,"ammunition hologram metadata must initialize native class225 in slot2");
     vendor.before_vendor_update();vendor.after_vendor_update();
     require(f.get<std::uint16_t>(ammo+0xa6)==32000,"selected Revolverator ammunition must show the companion cell model");
     require(f.get<std::uint16_t>(preview_list+0x100+0xa6)==32000,"the details monitor must show the cell as well as the separate world hologram");
     require(f.get<unsigned>(ammo+0x24)==pclass&&f.get<unsigned>(ammo+0x78)==pvars&&f.get<unsigned>(ammo+0x38)==stamp,"render substitution must retain native class data and record identity");
     require(f.get<std::uint16_t>(preview_list+0x300+0xa6)==180,"unowned weapon hologram must remain a weapon model");
     vendor.before_vendor_update();require(f.get<std::uint16_t>(ammo+0xa6)==225,"native class must be restored before native update/deletion");
     require(f.get<std::uint16_t>(preview_list+0x100+0xa6)==180,"monitor must regain its native weapon class before updates");
     vendor.after_vendor_update();vendor.frame();require(f.get<std::uint16_t>(ammo+0xa6)==225,"frame boundary must restore after deferred rendering and before the next tick");
     require(f.inventory()==inventory,"preview substitution must not change inventory or ammunition");
     vendor.reset();require(f.get<int>(preview_model)==0,"owned preview metadata must restore on unload");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);
     f.rows({{18,0,0,0,102},{18,0,0,0x52564f31,102}},1);f.previews();f.weapon_resources();
     vendor.frame();vendor.after_vendor_update();
     require(f.get<std::uint16_t>(monitor+0xa6)==2017&&f.get<std::uint16_t>(hologram+0xa6)==2017,
         "Revolverator purchase must show the drill on both vendor actors");
     const unsigned monitor_pose=f.get<unsigned>(monitor+0x64),hologram_pose=f.get<unsigned>(hologram+0x64);
     require(f.arena.owns(monitor_pose,64)&&f.arena.owns(hologram_pose,64)&&monitor_pose!=hologram_pose,
         "monitor and hologram use independent child poses to keep the head attached");
     require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"weapon purchase selection must not receive an ammunition model");
     vendor.before_vendor_update();
     require(f.get<std::uint16_t>(monitor+0xa6)==180&&f.get<std::uint16_t>(hologram+0xa6)==180,
         "native selection initialization receives canonical actors");
     require(f.get<unsigned>(monitor+0x64)==0&&f.get<unsigned>(hologram+0x64)==0,
         "native selection initialization does not see vendor pose overrides");
     vendor.after_vendor_update();f.put(menu+0x58,0);vendor.after_vendor_update();
     require(f.get<std::uint16_t>(monitor+0xa6)==180&&f.get<std::uint16_t>(hologram+0xa6)==180,
         "the regular Walloper row keeps its original previews despite sharing the carrier");
     f.put(menu+0x58,1);vendor.after_vendor_update();vendor.frame();
     require(f.get<std::uint16_t>(monitor+0xa6)==180&&f.get<std::uint16_t>(hologram+0xa6)==180,
         "the frame boundary releases both vendor preview actors");
     require(f.arena.used()==2*5*64,"repeated previews reuse their pose storage");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();
     const auto monitor=preview_list+0x100,hologram=preview_list+0x200;
     const std::array<float,3> origin{{10,20,30}},centre{{10*1024.f,20*1024.f,30*1024.f}};
     f.put(monitor+0x10,origin);f.put(monitor,centre);f.put(monitor+0x2c,.14583333f);f.put(hologram+0x2c,.027870022f);
     f.put(monitor+0xc0,std::array<float,3>{{0,1,0}});f.put(monitor+0xd0,std::array<float,3>{{-1,0,0}});f.put(monitor+0xe0,std::array<float,3>{{0,0,1}});
     vendor.frame();vendor.after_vendor_update();
     const auto at=f.get<std::array<float,3>>(monitor+0x10);const float scale=f.get<float>(monitor+0x2c);
     require(scale==f.get<float>(hologram+0x2c),"monitor uses native ammunition scale instead of the five-times-larger glove scale");
     require(std::abs(at[0]+.032292763f*scale-10)<.00001f&&std::abs(at[1]-20)<.00001f&&std::abs(at[2]+7.409395f*scale-30)<.00001f,
         "ammo mesh centre stays at the native monitor centre for a rotated actor");
     require(f.get<std::array<float,3>>(monitor)==centre,"fitting preserves the original culling centre");
     vendor.before_vendor_update();
     require(f.get<std::array<float,3>>(monitor+0x10)==origin&&f.get<float>(monitor+0x2c)==.14583333f,
         "next native update receives the original monitor position and scale");
     vendor.after_vendor_update();f.put(monitor+0x2c,.5f);f.put(monitor+0x10,std::array<float,3>{{9,8,7}});vendor.before_vendor_update();
     require(f.get<float>(monitor+0x2c)==.5f&&f.get<std::array<float,3>>(monitor+0x10)==std::array<float,3>{{9,8,7}},
         "a later owner's position or scale edit survives lease restoration");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{15,1,0,0,102}});f.previews();vendor.frame();vendor.after_vendor_update();
     require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"other weapons' ammunition previews must remain untouched");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();f.put<std::uint16_t>(preview_list+0x200+0xa6,214);
     vendor.frame();vendor.after_vendor_update();require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==214,"a previous selection's preview must not be claimed before native reinitialization");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();vendor.frame();vendor.after_vendor_update();
     const unsigned ammo=preview_list+0x200;f.put<std::uint8_t>(ammo+0x20,0xfd);vendor.before_vendor_update();
     require(f.get<std::uint16_t>(ammo+0xa6)==225,"an owned preview queued for deletion still needs its native class restored");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();vendor.frame();vendor.after_vendor_update();
     const unsigned ammo=preview_list+0x200;f.put(ammo+0x38,999u);vendor.before_vendor_update();
     require(f.get<std::uint16_t>(ammo+0xa6)==32000,"a reused record must not be restored through a stale preview lease");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();f.put<std::uint8_t>(preview_list+0x200+0x20,0xfe);
     vendor.frame();vendor.after_vendor_update();require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"dead preview records must not be substituted");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();f.put(0x15f5c4,0);vendor.frame();vendor.after_vendor_update();
     require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"stale vendor pointers outside vendor mode must not be touched");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();vendor.frame();vendor.after_vendor_update();
     vendor.reset();require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"unload must restore an active preview lease before native cleanup");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);vendor.frame();f.put(preview_model,214);vendor.reset();
     require(f.get<int>(preview_model)==214,"unload must preserve another owner's later preview-metadata change");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();vendor.frame();vendor.after_vendor_update();
     current_level=12;f.put<std::uint16_t>(preview_list+0x200+0xa6,180);f.put(preview_model,214);vendor.reset();
     require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==180&&f.get<int>(preview_model)==214,"overlay changes must invalidate old preview addresses and metadata ownership");}
    {Fixture f;NativeVendor vendor(&f.api,f.weapon_alias,f.arena);f.rows({{18,1,0,0,102}});f.previews();f.put(menu+0x20,0x1fffb00u);
     vendor.frame();vendor.after_vendor_update();
     require(f.get<std::uint16_t>(preview_list+0x200+0xa6)==225,"all six preview records must fit guest memory before substitution");}
    std::printf("vendor: %d checks passed (Hoven gating, native ownership/stock, list bounds, preview identity and lifecycle)\n",checks);
}

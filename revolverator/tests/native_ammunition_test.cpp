// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "native_ammunition.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using revolverator::NativeAmmunition;
using TextRecord=std::array<unsigned,4>;
using TextSnapshot=std::array<TextRecord,3>;
using VendorSnapshot=std::array<unsigned char,24>;
using DefinitionSnapshot=std::array<unsigned char,76>;
#define CHECK(expression) do { if(!(expression)) { \
    std::fprintf(stderr,"line %d: %s\n",__LINE__,#expression);std::exit(1); \
} } while(0)

namespace {
constexpr unsigned vendor=0x1c40b0+18*24, definition=0x179ac0+18*76;
constexpr unsigned resident=0x1863d0+18*76, ammo_at=0x13d428+18*4;
constexpr unsigned text_ptr=0x15f6a0,text_count=0x1996fc,bank=0x300000,hero=0x13f350;
struct Fixture;
Fixture* fixture;
struct Fixture {
    std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000,0);
    openrac_api api{};
    int level=1, allocations=0,takes=0,last_item=-1,last_amount=0;
    unsigned cursor=0x600000;
    bool fail_allocate=false,resident_alias=false;
    template<class T>T get(unsigned at)const {T v{};std::memcpy(&v,ram.data()+at,sizeof v);return v;}
    template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
    Fixture(){
        fixture=this;api.memory=ram.data();
        api.loaded_level=+[]{return fixture->level;};
        api.relocate_data=+[](unsigned at){return fixture->resident_alias&&at==0x1863d0?0x179ac0u:at;};
        api.relocate_level_data=+[](int,unsigned at){return at;};
        api.function=+[](const char* name)->void* {
            if(std::strcmp(name,"FUN_001ff288")==0)return reinterpret_cast<void*>(+[](unsigned n)->unsigned {
                auto& f=*fixture;++f.allocations;if(f.fail_allocate)return 0;
                const unsigned at=f.cursor;f.cursor=(f.cursor+n+15)&~15u;return at;
            });
            if(std::strcmp(name,"FUN_L00_00233db8")==0)return reinterpret_cast<void*>(+[](int item,int amount)->int {
                auto& f=*fixture;++f.takes;f.last_item=item;f.last_amount=amount;
                const int available=f.get<int>(ammo_at);
                if(available<amount)return 0;
                f.put(ammo_at,available-amount);return 1;
            });
            return nullptr;
        };
        put(definition+0x10,180);put(resident+0x10,180);
        put<std::uint16_t>(definition+0x3a,0xffff);put<std::uint16_t>(resident+0x3a,0xffff);
        put(text_ptr,bank);put(text_count,3);
        put(bank,std::array<unsigned,4>{0x310000,20037,7,9});
        put(bank+16,std::array<unsigned,4>{0x310100,20345,1,2});
        put(bank+32,std::array<unsigned,4>{0x310200,20346,3,4});
        put(hero+0x10b8,18);put(hero+0x10b4,2);put(hero+0x22a8,4);
        put(0x15f5c4,0);
    }
    unsigned pickup(unsigned m,unsigned vars,int item=18,unsigned stamp=7) {
        put(m+0x78,vars);put(m+0x38,stamp);put<std::uint16_t>(m+0xa6,225);
        put<unsigned char>(m+0x20,1);put(vars+8,item);return m;
    }
};
}

int main(){
    {
        Fixture f;NativeAmmunition ammunition(&f.api);ammunition.frame();
        CHECK(f.get<std::uint16_t>(vendor+8)==25&&f.get<std::uint16_t>(vendor+10)==25);
        CHECK(f.get<std::uint16_t>(vendor+12)==2&&f.get<std::uint16_t>(vendor+14)==10);
        CHECK(f.get<std::uint16_t>(vendor+18)==10);
        CHECK(f.get<std::uint16_t>(definition+0x3a)==225&&f.get<std::uint16_t>(resident+0x3a)==225);
        CHECK(f.get<int>(ammo_at)==0); // Loading an existing empty save does not refill it.
        CHECK(ammunition.maximum()==10&&ammunition.count()==0);
        CHECK(f.get<int>(text_count)==5&&f.allocations==1);
        const auto copy=f.get<unsigned>(text_ptr);
        CHECK(copy!=bank&&f.get<TextRecord>(copy)==f.get<TextRecord>(bank));
        CHECK(f.get<int>(copy+3*16+4)==f.get<std::uint16_t>(definition+0x34));
        CHECK(f.get<int>(copy+4*16+4)==f.get<std::uint16_t>(definition+0x36));
        for(int i=0;i<100;++i)ammunition.frame();CHECK(f.allocations==1);
        f.put(ammo_at,10);CHECK(ammunition.begin_attack());
        CHECK(f.last_item==18&&f.last_amount==1&&f.takes==1&&ammunition.count()==9);
        f.put(0x15f5c4,3);CHECK(!ammunition.begin_attack()&&f.takes==1);
        f.put(0x15f5c4,0);f.put(hero+0x10b8,10);CHECK(!ammunition.begin_attack()&&f.takes==1);
        f.put(hero+0x10b8,18);f.put(hero+0x22a8,0);CHECK(!ammunition.begin_attack()&&f.takes==1);
        f.put(hero+0x22a8,4);f.put(ammo_at,0);CHECK(!ammunition.begin_attack()&&f.takes==2);
        CHECK(ammunition.count()==0);
        // Preserve a different mod's edit to a pre-existing record in our clone.
        f.put(copy+16,0x320000u);
        // Preserve an independently edited native capacity field on teardown.
        f.put<std::uint16_t>(vendor+14,35);
        ammunition.reset();
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<int>(text_count)==3);
        CHECK(f.get<unsigned>(bank+16)==0x320000);
        CHECK(f.get<std::uint16_t>(vendor+8)==0&&f.get<std::uint16_t>(vendor+14)==35);
        CHECK(f.get<std::uint16_t>(definition+0x3a)==0xffff);
        CHECK(f.get<std::uint16_t>(resident+0x3a)==0xffff);
    }
    {
        Fixture f;NativeAmmunition ammunition(&f.api);ammunition.frame();
        const auto ours=f.pickup(0x400000,0x410000),other=f.pickup(0x400100,0x410100,17);
        ammunition.before_update(ours);CHECK(f.get<std::uint16_t>(ours+0xa6)==225);
        ammunition.after_update(ours);CHECK(f.get<std::uint16_t>(ours+0xa6)==32000);
        ammunition.before_update(other);ammunition.after_update(other);
        CHECK(f.get<std::uint16_t>(other+0xa6)==225);
        ammunition.before_update(ours);CHECK(f.get<std::uint16_t>(ours+0xa6)==225);
        ammunition.after_update(ours);ammunition.reset();
        CHECK(f.get<std::uint16_t>(ours+0xa6)==225);
    }
    {
        Fixture f;NativeAmmunition ammunition(&f.api);ammunition.frame();
        const auto m=f.pickup(0x400000,0x410000);
        ammunition.before_update(m);f.put<unsigned char>(m+0x20,0xfe);
        ammunition.after_update(m);CHECK(f.get<std::uint16_t>(m+0xa6)==225);
        f.put<unsigned char>(m+0x20,1);ammunition.before_update(m);
        // The slot was recycled inside the original callback: do not paint it.
        f.put(m+0x38,8u);f.put<std::uint16_t>(m+0xa6,1006);
        ammunition.after_update(m);CHECK(f.get<std::uint16_t>(m+0xa6)==1006);
        ammunition.reset();CHECK(f.get<std::uint16_t>(m+0xa6)==1006);
    }
    {
        Fixture f;f.fail_allocate=true;
        f.put<std::uint16_t>(definition+0x34,321);f.put<std::uint16_t>(definition+0x36,322);
        f.put<std::uint16_t>(resident+0x34,621);f.put<std::uint16_t>(resident+0x36,622);
        f.put(ammo_at,3);
        const auto original_vendor=f.get<VendorSnapshot>(vendor);
        const auto original_definition=f.get<DefinitionSnapshot>(definition);
        const auto original_resident=f.get<DefinitionSnapshot>(resident);
        const auto original_text=f.get<TextSnapshot>(bank);
        NativeAmmunition ammunition(&f.api);ammunition.frame();
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<int>(text_count)==3);
        CHECK(f.get<TextSnapshot>(bank)==original_text);
        CHECK(ammunition.maximum()==10&&ammunition.count()==3&&f.takes==0);
        CHECK(f.get<std::uint16_t>(vendor+8)==25&&f.get<std::uint16_t>(vendor+12)==2&&f.get<std::uint16_t>(vendor+18)==10);
        CHECK(f.get<std::uint16_t>(definition+0x3a)==225&&f.get<std::uint16_t>(resident+0x3a)==225);
        CHECK(f.get<std::uint16_t>(definition+0x34)==321&&f.get<std::uint16_t>(definition+0x36)==322);
        CHECK(f.get<std::uint16_t>(resident+0x34)==621&&f.get<std::uint16_t>(resident+0x36)==622);
        CHECK(ammunition.begin_attack()&&f.takes==1&&f.last_item==18&&f.last_amount==1&&ammunition.count()==2);
        // Optional labels do not force another allocation or disable later shots.
        f.fail_allocate=false;ammunition.frame();CHECK(f.allocations==1&&ammunition.maximum()==10);
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<int>(text_count)==3);
        ammunition.reset();
        CHECK(f.get<VendorSnapshot>(vendor)==original_vendor);
        CHECK(f.get<DefinitionSnapshot>(definition)==original_definition);
        CHECK(f.get<DefinitionSnapshot>(resident)==original_resident);
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<TextSnapshot>(bank)==original_text&&f.get<int>(ammo_at)==2);
    }
    {
        Fixture f;revolverator::GuestArena guest;
        guest.reserve(0x600000,32);CHECK(guest.allocate(32)==0x600000&&guest.available()==0);
        f.put<std::uint16_t>(definition+0x34,401);f.put<std::uint16_t>(definition+0x36,402);
        f.put<std::uint16_t>(resident+0x34,701);f.put<std::uint16_t>(resident+0x36,702);
        f.put(ammo_at,3);
        const auto original_definition=f.get<DefinitionSnapshot>(definition);
        const auto original_resident=f.get<DefinitionSnapshot>(resident);
        const auto original_text=f.get<TextSnapshot>(bank);
        NativeAmmunition ammunition(&f.api,&guest,false);ammunition.frame();
        CHECK(f.allocations==0&&guest.used()==32&&guest.available()==0);
        CHECK(ammunition.maximum()==10&&ammunition.count()==3);
        CHECK(f.get<std::uint16_t>(vendor+8)==25&&f.get<std::uint16_t>(vendor+12)==2);
        CHECK(f.get<std::uint16_t>(definition+0x3a)==225&&f.get<std::uint16_t>(resident+0x3a)==225);
        CHECK(f.get<std::uint16_t>(definition+0x34)==401&&f.get<std::uint16_t>(definition+0x36)==402);
        CHECK(f.get<std::uint16_t>(resident+0x34)==701&&f.get<std::uint16_t>(resident+0x36)==702);
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<int>(text_count)==3&&f.get<TextSnapshot>(bank)==original_text);
        CHECK(ammunition.begin_attack()&&f.takes==1&&ammunition.count()==2&&f.allocations==0);
        const auto pickup=f.pickup(0x400000,0x410000);
        ammunition.before_update(pickup);ammunition.after_update(pickup);
        CHECK(f.get<std::uint16_t>(pickup+0xa6)==32000);
        ammunition.reset();
        CHECK(f.get<std::uint16_t>(pickup+0xa6)==225&&f.get<std::uint16_t>(vendor+14)==0);
        CHECK(f.get<DefinitionSnapshot>(definition)==original_definition&&f.get<DefinitionSnapshot>(resident)==original_resident);
        CHECK(f.get<unsigned>(text_ptr)==bank&&f.get<int>(text_count)==3&&f.get<TextSnapshot>(bank)==original_text);
        CHECK(f.get<int>(ammo_at)==2&&f.allocations==0);
    }
    {
        Fixture f;f.resident_alias=true;NativeAmmunition ammunition(&f.api);ammunition.frame();
        CHECK(f.get<std::uint16_t>(definition+0x3a)==225);ammunition.reset();
        CHECK(f.get<std::uint16_t>(definition+0x3a)==0xffff);
    }
    {
        Fixture f;f.put(bank+4,32700);f.put(bank+20,32701);
        NativeAmmunition ammunition(&f.api);ammunition.frame();
        CHECK(f.get<std::uint16_t>(definition+0x34)==32702);
    }
    {
        Fixture f;NativeAmmunition ammunition(&f.api);ammunition.frame();
        const auto clone=f.get<unsigned>(text_ptr);
        f.put(text_ptr,0x500000u);f.put(text_count,12); // Another bank owner took over.
        ammunition.reset();CHECK(f.get<unsigned>(text_ptr)==0x500000&&f.get<int>(text_count)==12);
        CHECK(clone!=bank);
    }
    {
        Fixture f;NativeAmmunition ammunition(&f.api);ammunition.frame();
        // If a caller missed the pre-unload reset, never restore into a new level.
        f.level=2;f.put<std::uint16_t>(vendor+8,88);f.put(definition+0x10,71);
        ammunition.frame();CHECK(f.get<std::uint16_t>(vendor+8)==88);
    }
    std::puts("native ammunition adapter: metadata, native spending, empty/save guards, text ownership, pickups, stale handles and teardown passed");
}

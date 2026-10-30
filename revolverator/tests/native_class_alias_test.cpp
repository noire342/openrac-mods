// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_class_alias.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace revolverator;
namespace {
int loaded_level=1,checks=0;unsigned delta=0;
int loaded(){return loaded_level;}
unsigned relocate(unsigned at){return at+delta;}
void require(bool condition,const char* message){++checks;if(!condition){std::fprintf(stderr,"alias: %s\n",message);std::exit(1);}}
constexpr unsigned map=0x1b3ac0,resources=0x1b3200,ids=0x1b3900,updates=0x1b3580,count=0x15ff00;
struct Fixture {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x2000000);
    openrac_api api{};
    Fixture(){loaded_level=1;delta=0;api.memory=ram.data();api.loaded_level=loaded;api.relocate_data=relocate;api.game="rac1";api.version="ntsc";seed();}
    template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
    void seed(std::uint8_t slot=5,int total=12){
        std::memset(ram.data()+relocate(map),0xff,0x800);
        put<std::uint8_t>(relocate(map)+225,slot);put(relocate(count),total);
        if(slot<224){
            put<std::int16_t>(relocate(ids)+slot*2,225);put(relocate(resources)+slot*4,0x1f20000u);
            put(relocate(updates)+slot*4,0x2c7380u);
        }
    }
    unsigned alias()const{return get<std::uint8_t>(relocate(map)+2016);}
    void preview(unsigned at,unsigned stamp=42){
        put<std::uint8_t>(at+0x20,0);put<std::uint16_t>(at+0xa6,225);
        const auto slot=get<std::uint8_t>(relocate(map)+225);
        put(at+0x24,get<unsigned>(relocate(resources)+unsigned(slot)*4));
        put(at+0x78,0x1f28000u);put(at+0x38,stamp);
    }
    unsigned klass(unsigned at)const{return get<std::uint16_t>(at+0xa6);}
};
}
int main(){
    {Fixture f;NativeAmmoAlias alias(&f.api);const auto original=f.ram;require(!alias.ready(),"unpublished alias starts unavailable");alias.frame();
     require(alias.ready()&&f.alias()==5,"free ID2016 must share the native225 slot");
     unsigned differences=0;for(unsigned i=0;i<f.ram.size();++i)if(f.ram[i]!=original[i]){++differences;require(i==map+2016,"no pclass/callback/count/other mapping may change");}
     require(differences==1,"registration must change exactly one byte");alias.frame();require(alias.ready()&&f.get<int>(count)==12,"frame must be idempotent and retain slot count");
     alias.reset();require(!alias.ready()&&f.ram==original,"reset must restore the sole owned byte");}
    for(std::uint8_t occupied:{std::uint8_t(3),std::uint8_t(5)}){
     Fixture f;f.put(map+2016,occupied);const auto original=f.ram;NativeAmmoAlias alias(&f.api);alias.frame();alias.reset();
     require(!alias.ready()&&f.ram==original,"any pre-existing mapping belongs to another owner, even when it shares225");}
    {Fixture f;f.seed(223,224);NativeAmmoAlias alias(&f.api);alias.frame();require(alias.ready()&&f.alias()==223&&f.get<int>(count)==224,"sharing a class slot needs no spare resident slot");}
    for(int invalid_count:{0,-1,225}){
     Fixture f;f.put(count,invalid_count);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"invalid class counts must fail closed");}
    for(std::uint8_t invalid_slot:{std::uint8_t(255),std::uint8_t(224),std::uint8_t(12)}){
     Fixture f;f.put(map+225,invalid_slot);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"invalid or unregistered source slots must fail closed");}
    {Fixture f;f.put<std::int16_t>(ids+5*2,214);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"reverse slot ID must still identify225");}
    {Fixture f;f.put(resources+5*4,0x1ffffd0u);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"native class header must fit guest RAM");}
    {Fixture f;f.put(resources+5*4,0x1ffffb8u);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"header validation must include the sequence pointer at+0x48");}
    {Fixture f;f.put<std::uint8_t>(map+225,255);NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready(),"unloaded native225 cannot be aliased");
     f.seed();alias.frame();require(alias.ready(),"retry when the native class becomes available");}
    {Fixture f;NativeAmmoAlias alias(&f.api);alias.frame();f.put(resources+5*4,0x1f21000u);
     require(alias.ready(),"native resource replacement in the same225 slot must propagate automatically");}
    {Fixture f;NativeAmmoAlias alias(&f.api);alias.frame();f.put<std::uint8_t>(map+2016,7);alias.frame();alias.reset();
     require(f.alias()==7&&!alias.ready(),"another owner's later mapping must survive frame and reset");}
    {Fixture f;NativeAmmoAlias alias(&f.api);alias.frame();delta=0x200000;loaded_level=12;f.seed(7,18);alias.frame();
     require(alias.ready()&&f.alias()==7&&f.get<std::uint8_t>(map+2016)==5,"overlay changes must resolve new bases without writing old tables");
     alias.reset();require(f.alias()==255,"new overlay alias must restore independently");}
    {Fixture f;NativeAmmoAlias alias(&f.api);alias.frame();loaded_level=-1;alias.frame();require(!alias.ready()&&f.alias()==5,"no old table writes after leaving its overlay");}
    {Fixture f;f.api.version="pal";NativeAmmoAlias alias(&f.api);alias.frame();require(!alias.ready()&&f.alias()==255,"unsupported versions must remain unchanged");}
    constexpr unsigned first=0x1f24000,second=first+256,ordinary=second+256;
    {Fixture f;f.preview(first);f.preview(second,43);f.preview(ordinary,44);const auto original=f.ram;
     NativeAmmoAlias alias(&f.api);alias.frame();
     require(alias.apply_preview(first,true)&&alias.apply_preview(second,true),"two simultaneous native225 menu previews receive independent render aliases");
     require(f.klass(first)==2016&&f.klass(second)==2016&&f.klass(ordinary)==225,"only explicitly selected custom previews receive render2016");
     require(alias.apply_preview(first,true),"repeated render projection is idempotent");
     alias.canonicalize_preview(first);
     require(f.klass(first)==225&&f.klass(second)==2016,"native rebinding canonicalizes only its own preview");
     require(!alias.apply_preview(ordinary,false)&&f.klass(ordinary)==225,"ordinary ammo225 remains original when custom is false");
     require(alias.apply_preview(first,true),"canonicalized actor can acquire a fresh render lease");
     require(!alias.apply_preview(first,false)&&f.klass(first)==225&&f.klass(second)==2016,"logical identity change restores just the named owned preview");
     alias.frame();require(f.klass(first)==225&&f.klass(second)==225&&alias.ready(),"frame restores every preview before native updates while keeping the alias mapping");
     require(alias.apply_preview(first,true)&&alias.apply_preview(second,true),"preview leases are reusable on the next frame");
     alias.reset();require(f.ram==original,"reset restores all actor IDs and the sole owned mapping without resource writes");}
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);
     require(alias.apply_preview(first,true)&&alias.ready(),"preview can establish an alias after native225 streams during its original update");
     alias.restore_previews();require(f.klass(first)==225&&alias.ready(),"explicit preview restoration leaves the native slot mapping intact");}
    {Fixture f;f.preview(first);f.put<std::uint8_t>(map+225,255);NativeAmmoAlias alias(&f.api);alias.frame();
     require(!alias.ready()&&!alias.apply_preview(first,true)&&f.klass(first)==225,"an unavailable native source never relabels previews");
     f.seed();require(alias.apply_preview(first,true)&&f.klass(first)==2016,"late source streaming is retried by the post-update preview call");}
    {Fixture f;f.preview(first);f.put<std::uint8_t>(map+2016,5);NativeAmmoAlias alias(&f.api);
     require(!alias.apply_preview(first,true)&&f.klass(first)==225,"pre-existing alias mappings cannot be adopted even when they point at225");}
    {Fixture f;f.preview(first);f.put<std::uint16_t>(first+0xa6,2016);NativeAmmoAlias alias(&f.api);alias.frame();
     require(!alias.apply_preview(first,true),"an existing unowned render2016 actor is never adopted");
     alias.canonicalize_preview(first);alias.reset();require(f.klass(first)==2016,"unowned render aliases survive canonicalization and reset");}
    {Fixture f;f.preview(first);f.put(first+0x24,0x1f21000u);NativeAmmoAlias alias(&f.api);
     require(!alias.apply_preview(first,true)&&f.klass(first)==225,"source225 IDs with another resource header are rejected");}
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);require(alias.apply_preview(first,true),"resource-replacement fixture acquired");
     f.put(resources+5*4,0x1f21000u);require(!alias.apply_preview(first,true)&&f.klass(first)==225,"a replaced source header invalidates an earlier preview projection");}
    for(unsigned offset:{0x24u,0x78u,0x38u}){
     Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);require(alias.apply_preview(first,true),"recycled-actor fixture acquired");
     f.put(first+offset,f.get<unsigned>(first+offset)+16);alias.canonicalize_preview(first);alias.frame();
     require(f.klass(first)==2016,"pclass/pvars/stamp identity changes preserve every byte of a recycled actor");
    }
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);alias.apply_preview(first,true);f.put<std::uint16_t>(first+0xa6,777);alias.reset();
     require(f.klass(first)==777,"a third-party class edit wins over our preview restoration");}
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);alias.apply_preview(first,true);f.put<std::uint8_t>(first+0x20,0x80);alias.frame();
     require(f.klass(first)==2016,"inactive menu pool records are not rewritten during restoration");}
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);alias.apply_preview(first,true);loaded_level=-1;alias.frame();
     require(f.klass(first)==2016,"leaving an overlay never canonicalizes its stale preview memory");}
    {Fixture f;f.preview(first);NativeAmmoAlias alias(&f.api);alias.apply_preview(first,true);f.put<std::uint8_t>(map+2016,7);alias.frame();
     require(f.alias()==7&&f.klass(first)==225&&!alias.ready(),"later foreign mappings survive while our own actor projection is restored");}
    {Fixture f;NativeAmmoAlias alias(&f.api);alias.frame();
     for(unsigned i=0;i<32;++i){f.preview(first+i*256,100+i);require(alias.apply_preview(first+i*256,true),"bounded preview lease is available");}
     constexpr unsigned extra=first+32*256;f.preview(extra,132);
     require(!alias.apply_preview(extra,true)&&f.klass(extra)==225,"preview ownership is capped at32 without rewriting the excess actor");
     alias.canonicalize_preview(first);require(alias.apply_preview(extra,true),"canonicalization releases a bounded ownership slot");
     alias.frame();for(unsigned i=0;i<33;++i)require(f.klass(first+i*256)==225,"every bounded preview is canonical before the next update");}
    {Fixture f;NativeAmmoAlias alias(&f.api);require(!alias.apply_preview(0,true)&&!alias.apply_preview(0x1ffff80,true),"invalid/truncated actor pointers are rejected");}
    std::printf("alias: %d checks passed (one-byte mapping, bounded previews, class/token ownership, rebinding and frame restore)\n",checks);
}

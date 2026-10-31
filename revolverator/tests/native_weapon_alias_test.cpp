// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_alias.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace revolverator;
namespace {
unsigned delta=0;int loaded=1,checks=0;
constexpr unsigned map=0x1b3ac0,resources=0x1b3200,updates=0x1b3580,ids=0x1b3900,count=0x15ff00;
constexpr unsigned part=0x400000,first=0x200000,second=0x200100;
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"weapon alias: %s\n",why);std::exit(1);}}
struct Fixture {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0x2000000);openrac_api api{};
    template<class T>void put(unsigned at,T value){std::memcpy(bytes.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T v{};std::memcpy(&v,bytes.data()+at,sizeof v);return v;}
    Fixture(){delta=0;loaded=1;api.memory=bytes.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=+[]{return loaded;};api.relocate_data=+[](unsigned at){return at+delta;};seed();}
    void seed(unsigned slot=5){std::memset(bytes.data()+map+delta,255,2048);put<int>(count+delta,224);
        put<std::uint8_t>(map+delta+180,std::uint8_t(slot));put(resources+delta+slot*4,part);
        put(updates+delta+slot*4,0x2bb758u);put<std::int16_t>(ids+delta+slot*2,180);put<std::uint8_t>(part+8,5);
        moby(first);moby(second);}
    void moby(unsigned at){put<std::uint8_t>(at+0x20,2);put(at+0x24,part);put(at+0x38,37u);
        put(at+0x78,at+0x100000);put<std::uint16_t>(at+0xa6,180);}
};
}
int main(){
    {Fixture f;const auto before=f.bytes;NativeWeaponAlias alias(&f.api);alias.frame();
     require(alias.ready()&&f.get<std::uint8_t>(map+2017)==5,"a distinct render ID shares the existing native180 class slot");
     unsigned changed=0;for(unsigned i=0;i<f.bytes.size();++i)if(f.bytes[i]!=before[i]){++changed;require(i==map+2017||i==map+2018,"class mapping never edits resources/callbacks/counts");}
     require(changed==2,"base and Golden registration occupy only two independent map bytes");
     require(!alias.apply(first,false)&&f.get<std::uint16_t>(first+0xa6)==180,"normal Walloper retains native appearance");
     require(alias.apply(first,true)&&alias.owns(first)&&f.get<std::uint16_t>(first+0xa6)==2017,"selected Revolverator receives its render identity");
     require(f.get<std::uint16_t>(second+0xa6)==180,"another native Walloper is unaffected");
     require(alias.apply(first,true)&&alias.owns(first),"pre-render application is idempotent");
     require(alias.apply(first,true,true)&&alias.owns(first)&&f.get<std::uint16_t>(first+0xa6)==2018,"Golden selects its own material without changing the native actor");
     require(alias.apply(first,true,false)&&f.get<std::uint16_t>(first+0xa6)==2017,"variant switches preserve the ownership lease");
     alias.frame();require(!alias.owns(first)&&f.get<std::uint16_t>(first+0xa6)==180,"next frame restores canonical class before native logic");
     alias.apply(first,true);alias.apply(second,true);
     require(f.get<std::uint16_t>(first+0xa6)==180&&f.get<std::uint16_t>(second+0xa6)==2017,"switching the held actor restores only the old owned alias");
     alias.apply(second,false);require(f.get<std::uint16_t>(second+0xa6)==180,"switching logical identity restores Walloper immediately");
     alias.reset();require(f.bytes==before,"reset restores every changed byte");}
    for(unsigned field:{0x24u,0x38u,0x78u}){
     Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();alias.apply(first,true);f.put(first+field,0x600000u);
     alias.frame();require(f.get<std::uint16_t>(first+0xa6)==2017,"recycled actor identity is never written through");}
    {Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();alias.apply(first,true);f.put<std::uint16_t>(first+0xa6,185);
     alias.frame();require(f.get<std::uint16_t>(first+0xa6)==185,"another owner's later actor edit survives cleanup");}
    {Fixture f;f.put<std::uint8_t>(map+2017,5);const auto before=f.bytes;NativeWeaponAlias alias(&f.api);alias.frame();alias.reset();
     require(!alias.ready()&&f.bytes==before,"pre-existing alias mapping belongs to another owner even if it shares180");}
    {Fixture f;f.put<std::uint8_t>(map+2018,7);const auto before=f.bytes;NativeWeaponAlias alias(&f.api);alias.frame();alias.reset();
     require(!alias.ready()&&f.bytes==before,"a foreign Golden class mapping is never overwritten or partially claimed");}
    {Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();f.put<std::uint8_t>(map+2017,7);alias.frame();alias.reset();
     require(f.get<std::uint8_t>(map+2017)==7&&!alias.ready(),"later mapping replacement survives reset");}
    {Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();alias.apply(first,true);loaded=2;delta=0x600000;f.seed(9);alias.frame();
     require(alias.ready()&&f.get<std::uint8_t>(map+delta+2017)==9,"overlay relocation resolves the new class table");
     require(f.get<std::uint8_t>(map+2017)==5,"cleanup never writes unloaded class tables");}
    {Fixture f;f.put<std::int16_t>(ids+5*2,181);NativeWeaponAlias alias(&f.api);alias.frame();
     require(!alias.ready()&&!alias.apply(first,true),"source reverse identity must be native180");}
    {Fixture f;f.put<std::uint8_t>(part+8,17);NativeWeaponAlias alias(&f.api);alias.frame();
     require(!alias.ready()&&!alias.apply(first,true),"a streamed buffer containing another weapon's skeleton cannot be used as native180");}
    {Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();f.put(first+0x24,part+0x100);require(!alias.apply(first,true),"unrelated pclass cannot be re-labelled");}
    {Fixture f;constexpr unsigned preview1=0x210000,preview2=0x210100,normal=0x210200;
     f.moby(preview1);f.moby(preview2);f.moby(normal);NativeWeaponAlias alias(&f.api);alias.frame();
     require(alias.apply(first,true)&&alias.apply_preview(preview1,true)&&alias.apply_preview(preview2,true),
             "the held actor and two simultaneous previews have independent ownership leases");
     require(alias.owns(first)&&alias.owns(preview1)&&alias.owns(preview2)&&f.get<std::uint16_t>(normal+0xa6)==180,
             "other normal Walloper mobys remain unchanged while custom previews render");
     require(alias.apply_preview(preview1,true)&&alias.owns(first)&&alias.owns(preview2),"repeated preview projection preserves every other lease");
     require(!alias.apply_preview(first,true)&&alias.owns(first),"a preview call cannot steal the held actor's ownership lease");
     alias.apply_preview(preview1,false);
     require(f.get<std::uint16_t>(preview1+0xa6)==180&&alias.owns(first)&&alias.owns(preview2),"clearing one preview leaves held and other preview identities intact");
     alias.frame();require(!alias.owns(first)&&!alias.owns(preview2)&&f.get<std::uint16_t>(first+0xa6)==180&&f.get<std::uint16_t>(preview2+0xa6)==180,
             "frame canonicalizes all native actors before next update, not just the last preview");
     alias.apply(first,true);alias.apply_preview(preview1,true);alias.apply_preview(preview2,true);
     f.put(preview1+0x78,0x700000u);f.put<std::uint16_t>(preview2+0xa6,185);alias.reset();
     require(f.get<std::uint16_t>(preview1+0xa6)==2017&&f.get<std::uint16_t>(preview2+0xa6)==185&&f.get<std::uint16_t>(first+0xa6)==180,
             "recycled preview identities and later third-party edits survive reset while the held actor restores");}
    {Fixture f;NativeWeaponAlias alias(&f.api);alias.frame();
     for(unsigned i=0;i<33;++i){const unsigned at=0x210000+i*0x100;f.moby(at);
        require(alias.apply_preview(at,true)==(i<32),"preview ownership has a fixed bounded capacity");}
     require(f.get<std::uint16_t>(0x210000+32*0x100+0xa6)==180,"capacity exhaustion cannot leave an untracked alias behind");
     alias.frame();bool canonical=true;for(unsigned i=0;i<33;++i)canonical=canonical&&f.get<std::uint16_t>(0x210000+i*0x100+0xa6)==180;
     require(canonical,"all bounded previews restore on the next frame");}
    {Fixture f;NativeWeaponAlias alias(&f.api);f.put(resources+5*4,0u);alias.frame();
     require(!alias.ready(),"unloaded native streaming resource cannot be projected early");
     f.put(resources+5*4,part);require(alias.apply_preview(first,true)&&alias.ready(),"preview after native class loading can publish the mapping without waiting another tick");}
    {Fixture f;f.put<float>(first+0x2c,2);NativeWeaponAlias alias(&f.api);alias.frame();alias.apply_preview(first,true);
     require(alias.fit_preview(first,.65f)&&f.get<float>(first+0x2c)==1.3f,"compact preview fits the existing panel");
     require(alias.fit_preview(first,.65f)&&f.get<float>(first+0x2c)==1.3f,"multiple render passes never compound scale");
     alias.frame();require(f.get<float>(first+0x2c)==2,"native preview scale restores before the next update");
     alias.apply_preview(first,true);alias.fit_preview(first,.65f);f.put<float>(first+0x2c,3);alias.frame();
     require(f.get<float>(first+0x2c)==3,"another owner's later scale edit survives teardown");}
    std::printf("weapon alias: %d ownership, native-isolation and lifecycle checks passed\n",checks);
}

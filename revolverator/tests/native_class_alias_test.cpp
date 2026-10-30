// SPDX-License-Identifier: GPL-3.0-or-later
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
    std::printf("alias: %d checks passed (one-byte registration, collisions, resident bounds, relocation, ownership and restore)\n",checks);
}

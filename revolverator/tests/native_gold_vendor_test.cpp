// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_gold_vendor.hpp"
#include "native_weapon_alias.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
using namespace revolverator;
namespace {
int checks,current_level=1,created=0,removed=0,saves=0,queues=0,pushes=0,pops=0;
std::vector<int> classes,removed_classes;
std::string caption;
unsigned char* ram;
bool callbacks_valid=true,fade_hook_thunk=false;
unsigned stack_address=0x600000;
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"gold vendor: %s\n",why);std::exit(1);}}
template<class T>void put(unsigned address,T value){std::memcpy(ram+address,&value,sizeof value);}
template<class T>T get(unsigned address){T value{};std::memcpy(&value,ram+address,sizeof value);return value;}
int loaded(){return current_level;}
unsigned relocate(unsigned address){return address;}
unsigned relocate_level(int,unsigned address){return address;}
void native_init(unsigned){}void native_fade(unsigned){}void hooked_fade(unsigned){}int native_draw(unsigned){return 0;}
unsigned spawn(int klass){
 classes.push_back(klass);const auto index=unsigned(created++),address=0x400000+index*256,pvars=0x420000+index*128;
 put<unsigned char>(address+0x20,0);put<unsigned short>(address+0xa6,static_cast<unsigned short>(klass));
 put(address+0x24,klass==304?0x500000u:0x500100u);put(address+0x78,pvars);put(address+0x38,0u);
 put(address+0x74,0x2e1ac0u);put(address+0x2c,.14583333f);return address;
}
void remove(unsigned address){++removed;removed_classes.push_back(get<unsigned short>(address+0xa6));put<unsigned char>(address+0x20,0xfe);}
void basis(unsigned,unsigned){}
void copy(unsigned address){caption=reinterpret_cast<const char*>(ram+address);}
int queue(int channel,int id,unsigned a,unsigned b,unsigned c,unsigned context,int arg){
 require(channel==12&&id==0&&a==0x24c828&&b==0x24c878&&c==0x24c898&&context!=0&&arg==0,"uses the existing Gold/NPC HUD channel and native callbacks with owned context");return ++queues;
}
int keep(int serial,int){caption=reinterpret_cast<const char*>(ram+0x17e530);return serial<0?1:0;}
int autosave(int a,int b){require(a==0&&b==-1,"purchase requests the standard native autosave");++saves;return 1;}
unsigned stack_push(unsigned size,unsigned* saved){require(size==128,"prompt uses bounded temporary guest stack");++pushes;*saved=0x7000;return stack_address;}
void stack_pop(unsigned saved){require(saved==0x7000,"prompt restores original guest stack");++pops;}
void* function_at(unsigned address){if(!callbacks_valid)return nullptr;switch(address){case 0x24c828:return reinterpret_cast<void*>(native_init);case 0x24c878:return reinterpret_cast<void*>(native_fade);case 0x24c898:return reinterpret_cast<void*>(native_draw);default:return nullptr;}}
void* function(const char* name){
 if(!std::strcmp(name,"FUN_0020c4f8"))return reinterpret_cast<void*>(spawn);
 if(!std::strcmp(name,"FUN_0020c828"))return reinterpret_cast<void*>(remove);
 if(!std::strcmp(name,"FUN_001fa030"))return reinterpret_cast<void*>(basis);
 if(!std::strcmp(name,"FUN_001ff658"))return reinterpret_cast<void*>(copy);
 if(!std::strcmp(name,"FUN_001ff308"))return reinterpret_cast<void*>(queue);
 if(!std::strcmp(name,"FUN_L00_00235e18"))return reinterpret_cast<void*>(keep);
 if(!std::strcmp(name,"FUN_0020b178"))return reinterpret_cast<void*>(autosave);
 if(!std::strcmp(name,"FUN_L00_00237190"))return reinterpret_cast<void*>(native_init);
 if(!std::strcmp(name,"FUN_L00_002371e0"))return fade_hook_thunk?reinterpret_cast<void*>(hooked_fade):reinterpret_cast<void*>(native_fade);
 if(!std::strcmp(name,"FUN_L00_00237200"))return reinterpret_cast<void*>(native_draw);
 return nullptr;
}
struct Fixture {
 std::vector<unsigned char> memory=std::vector<unsigned char>(0x2200000);
 openrac_api api{};arsenal::Registry registry;NativeWeaponAlias alias;NativeGoldVendor vendor;
 Fixture():alias(&api),vendor(&api,nullptr,registry,alias){
  ram=memory.data();current_level=1;created=removed=saves=queues=pushes=pops=0;callbacks_valid=true;stack_address=0x600000;classes.clear();removed_classes.clear();caption.clear();
  api.memory=ram;api.game="rac1";api.version="ntsc";api.loaded_level=loaded;api.relocate_data=relocate;api.relocate_level_data=relocate_level;
  api.function=function;api.function_at=function_at;api.stack_push=stack_push;api.stack_pop=stack_pop;
  std::memset(ram+0x1b3ac0,0xff,0x800);put(0x15ff00,2);
  put<unsigned char>(0x1b3ac0+304,0);put<unsigned char>(0x1b3ac0+180,1);
  put<unsigned short>(0x1b3900,304);put<unsigned short>(0x1b3900+2,180);
  put(0x1b3200,0x500000u);put(0x1b3204,0x500100u);put<unsigned char>(0x500108,5);
  put(0x15ee20,1);put(0x15f5c4,0);put(0x13f350+0x22a8,4);put(0x15ed98,50000);put(0x15ed88,5);
  std::memset(ram+0x13e520,0x51,40);std::memset(ram+0x14bec0,0x37,80);
  require(registry.register_weapon({arsenal::RevolveratorKey,"noire342.revolverator",18,60037,20,15000,12}),"register only the base logical weapon");
  require(registry.set_state(arsenal::RevolveratorKey,{true,13,777}),"seed base ammo and XP");
  alias.frame();
 }
 void frame(){alias.frame();vendor.frame();}
 void near(){put(0x13f350+0x80,std::array<float,4>{{250.8f,123.6f,54.4966202f,1.f}});}
 std::array<unsigned char,120> gold()const{std::array<unsigned char,120> value{};std::memcpy(value.data(),ram+0x13e520,40);std::memcpy(value.data()+40,ram+0x14bec0,80);return value;}
};
}
int main(){
 {Fixture f;const auto native=f.gold();require(!f.vendor.update(0)&&!f.vendor.update(0x123400),"empty/foreign actors are never claimed");f.frame();
  require(created==2&&classes==std::vector<int>({304,304})&&f.vendor.ready(),"both actors use resident native304; no weapon or virtual display class is streamed");
  const auto dome=f.vendor.dome_moby(),preview=f.vendor.preview_moby();
  require(get<unsigned short>(preview+0xa6)==2019&&get<unsigned>(preview+0x24)==0x500000u,"static preview gets renderer2019 while keeping source304 resource");
  require(get<unsigned char>(0x1b3ac0+2019)==0&&get<unsigned short>(0x1b3900)==304&&get<int>(0x15ff00)==2,"display mapping changes only its unused byte, not reverse IDs or native class count");
  require(get<float>(preview+0x2c)==.1458333283662796f*2.1f,"Gold display matches the2.22m span of the larger native Gold weapons");
  const auto origin=get<std::array<float,4>>(preview+0x10);
  const float yaw=get<float>(preview+0x48),scale=get<float>(preview+0x2c);
  // Independent bounds measured from all meshes of the actual rigid GLB.
  const std::array<float,3> center{{(-.940316510649f+1.133600707758f)/2,
      (-6.195091765965f+1.041401924080f)/2,(-.653722701142f+2.493810474196f)/2}};
  const float x=origin[0]+scale*(center[0]*std::cos(yaw)-center[1]*std::sin(yaw));
  const float y=origin[1]+scale*(center[0]*std::sin(yaw)+center[1]*std::cos(yaw));
  const float z=origin[2]+scale*center[2];
  require(std::abs(x-250.8f)<.0001f&&std::abs(y-123.6f)<.0001f,
      "the complete weapon bounds are horizontally centered in the dome, not its grip");
  require(std::abs(z-(54.4966202f+1.775f))<.0001f,
      "visual centre matches the median height of the ten native Gold displays");
  const auto fixed_rotation=get<std::array<float,4>>(preview+0x40);
  for(unsigned i=0;i<120;++i)f.frame();
  require(get<std::array<float,4>>(preview+0x40)==fixed_rotation,"Gold display remains stationary like the original Gold weapons");
  require(get<unsigned>(preview+0x74)==0&&get<unsigned char>(dome+0x20)==3,"preview cannot run the held-weapon update and dome starts in safe native non-selling idle");
  require(get<unsigned>(get<unsigned>(dome+0x78)+0x40)==0&&get<unsigned>(get<unsigned>(dome+0x78)+0x44)==0,"native pvars remain untouched, without an assumed extension size");
  require(get<unsigned>(dome+0xb8)==0&&get<unsigned>(preview+0xb8)==0,"native parent pointers remain null, without introducing a bolt-source traversal cycle");
  put(dome+0x3c,0x808080u);f.frame();require(f.vendor.dome_moby()==dome&&created==2,"native lighting/color updates cannot invalidate ownership or duplicate cases");
  require(f.vendor.update(preview)&&saves==0,"preview callback is owned but cannot run the original native Gold sale");
  f.near();put(0x13cae4,0u);f.vendor.update(dome);require(caption.find("Rotatore d'oro")!=std::string::npos&&!f.registry.golden(arsenal::RevolveratorKey),"native prompt appears with Italian weapon name but does not auto-purchase");
  put(0x13cae4,0x10u);f.vendor.update(dome);
  require(f.registry.golden(arsenal::RevolveratorKey)&&get<int>(0x15ed98)==0&&saves==1,"Triangle purchases exactly once for50000 ordinary bolts");
  require(f.registry.state(arsenal::RevolveratorKey)->ammo==13&&f.registry.state(arsenal::RevolveratorKey)->xp==777,"upgrade preserves the same ammo and XP");
  require(f.gold()==native,"all native Gold ownership/collected-bolt bytes remain unchanged");
  require(!f.vendor.preview_moby()&&removed_classes.back()==304,"sold preview is restored to its native304 carrier before removal");
  require(f.vendor.poll_purchase()&&!f.vendor.poll_purchase(),"presentation event is delivered once");f.vendor.update(dome);
  require(get<int>(0x15ed98)==0&&saves==1,"replayed input never double charges or resaves");
  require(pushes==pops,"all prompt stack allocations are paired");f.vendor.reset();require(removed==2&&f.gold()==native,"reset removes only owned actors and preserves native progression");require(get<unsigned char>(0x1b3ac0+2019)==0xff,"owned display mapping restores on reset");}
 {Fixture f;put(0x15ee20,0);f.frame();require(created==0&&!f.vendor.ready(),"Novalis uses the native challenge-mode Gold shop access gate");}
 {Fixture f;const unsigned source=0x430000;
  put(0x15ff18,source);put(0x15ff20,source+256);put<unsigned short>(source+0xa6,304);
  put<unsigned char>(source+0x20,3);put(source+0x10,std::array<float,4>{{245.4f,125.0f,54.4966202f,1}});
  put(source+0x38,0x00000404u);put(source+0x3c,0x00111111u);put(source+0x90,0x00123456u);
  put<unsigned char>(source+0x73,24);put<unsigned short>(source+0x34,0x10);
  f.frame();const auto dome=f.vendor.dome_moby(),preview=f.vendor.preview_moby();
  require(dome&&get<unsigned>(dome+0x38)==0x00000404u&&get<unsigned>(dome+0x3c)==0x00111111u,
    "cupola inherits the native room light-set pair and ambient RGB instead of bright spawn defaults");
  require(get<unsigned>(preview+0x3c)==0x00111111u&&get<unsigned char>(preview+0x73)==24,
    "display falls back to room lighting if all original Gold weapon previews have been purchased");
  require(get<unsigned>(dome+0x90)==0x00123456u&&(get<unsigned short>(dome+0x34)&0x10),"native glow and metal-lighting presentation are retained");
  require(get<unsigned char>(dome+0xb0)==0xff&&get<unsigned short>(dome+0xb2)==0xffff,"lighting never borrows native mission IDs or save bits");
  for(unsigned i=0;i<60;++i)f.frame();require(created==2&&f.vendor.dome_moby()==dome,"inherited lighting does not duplicate or invalidate the owned cupola");}
 {Fixture f;f.frame();put(0x13f350+0x80,std::array<float,4>{{250.8f,126.95f,54.4966202f,1}});
  f.vendor.update(f.vendor.dome_moby());require(caption.find("Triangolo")!=std::string::npos,"Triangle prompt appears3.35m from the case centre, outside the glass");
  put(0x13cae4,0x10u);f.vendor.update(f.vendor.dome_moby());require(saves==1&&f.registry.golden(arsenal::RevolveratorKey),"purchase succeeds at the wider prompt distance");}
 {Fixture f;f.frame();put(0x13f350+0x80,std::array<float,4>{{250.8f,127.2f,54.4966202f,1}});put(0x13cae4,0x10u);
  f.vendor.update(f.vendor.dome_moby());require(caption.empty()&&saves==0&&!f.registry.golden(arsenal::RevolveratorKey),"the larger interaction region is still bounded at3.5m");}
 {Fixture f;f.registry.set_state(arsenal::RevolveratorKey,{});f.frame();f.near();put(0x13cae4,0x10u);f.vendor.update(f.vendor.dome_moby());
  require(!f.registry.golden(arsenal::RevolveratorKey)&&get<int>(0x15ed98)==50000&&saves==0,"unowned base weapon cannot buy Golden");}
 {Fixture f;f.frame();f.near();put(0x15ed98,49999);put(0x13cae4,0x10u);f.vendor.update(f.vendor.dome_moby());
  require(get<int>(0x15ed98)==49999&&!f.registry.golden(arsenal::RevolveratorKey),"insufficient funds do not mutate state or currency");}
 {Fixture f;f.frame();f.near();put(0x15f5c4,3);put(0x13cae4,0x10u);f.vendor.update(f.vendor.dome_moby());require(saves==0&&get<int>(0x15ed98)==50000,"pause/cutscene mode cannot buy");}
 {Fixture f;callbacks_valid=false;f.frame();require(created==0,"unverified native HUD callback addresses fail closed");}
 {Fixture f;fade_hook_thunk=true;f.frame();require(created==2&&f.vendor.ready(),"hookable fade thunk may differ from the verified guest callback entry");fade_hook_thunk=false;}
 {Fixture f;f.api.stack_push=nullptr;f.api.stack_pop=nullptr;std::memset(ram+0x19a440,0x37,64);put<unsigned char>(0x17e52f,0x55);put<unsigned char>(0x17e5b0,0x66);
  f.frame();f.near();f.vendor.update(f.vendor.dome_moby());
  require(!caption.empty()&&caption.find("Rotatore")!=std::string::npos,"prompt reaches the actual native overlay draw buffer without SDK-stack dependence");
  require(get<unsigned char>(0x17e52f)==0x55&&get<unsigned char>(0x17e5b0)==0x66,"prompt respects both128-byte buffer boundaries");
  bool unrelated=true;for(unsigned i=0;i<64;++i)unrelated=unrelated&&ram[0x19a440+i]==0x37;
  require(unrelated&&pushes==0&&pops==0,"unrelated resident shared text is preserved");}
 {Fixture f;put<unsigned char>(0x500108,2);f.frame();require(created==2&&f.vendor.ready()&&f.vendor.preview_moby(),"static preview remains visible even when source180 belongs to another held weapon");f.near();put(0x13cae4,0x10u);f.vendor.update(f.vendor.dome_moby());require(f.registry.golden(arsenal::RevolveratorKey),"purchase does not require the Rotatore to be equipped");}
 {Fixture f;put<unsigned char>(0x1b3ac0+2019,7);f.frame();require(created==1&&f.vendor.ready()&&!f.vendor.preview_moby()&&get<unsigned char>(0x1b3ac0+2019)==7,"foreign display mapping is never stolen");}
 {Fixture f;f.frame();put<unsigned char>(0x1b3ac0+2019,9);f.vendor.reset();require(get<unsigned char>(0x1b3ac0+2019)==9,"reset preserves a later foreign mapping change");}
 {Fixture f;put<unsigned char>(0x1b3ac0+304,0xff);f.frame();require(created==0,"missing native case resources fail closed");}
 {Fixture f;f.frame();const auto dome=f.vendor.dome_moby();put(dome+0x38,999u);require(!f.vendor.update(dome),"recycled actor cannot be claimed through an old pointer");f.vendor.reset();require(removed==1&&get<unsigned>(dome+0x38)==999,"reset preserves a foreign replacement actor");}
 {Fixture f;f.frame();const auto dome=f.vendor.dome_moby();put<unsigned char>(dome+0x20,0);require(!f.vendor.update(dome),"reinitialized slot with identical native zero stamp is not mistaken for our idle actor");f.vendor.reset();require(removed==1&&get<unsigned char>(dome+0x20)==0,"cleanup preserves the reinitialized foreign slot");}
 {Fixture f;current_level=13;f.frame();require(created==0&&!f.vendor.ready(),"other planets never receive the Novalis offer");}
 std::printf("native Gold vendor: %d actor, native-prompt, purchase and ownership checks passed\n",checks);
}

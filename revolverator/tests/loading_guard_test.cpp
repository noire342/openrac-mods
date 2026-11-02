// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "presentation.hpp"
#include "localization.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* message){
 ++checks;if(!condition){std::fprintf(stderr,"loading guard: %s\n",message);std::exit(1);}
}
constexpr unsigned Arena=0x1dcca40,TextBank=0x400000,Name=0x401000,Description=0x401100;
constexpr unsigned Defs=0x410000,Refs=0x411000,Images=0x412000,Palettes=0x413000,Header=0x414000;
struct Fixture;
Fixture* live;
struct Fixture{
 const RegionBindings& region;
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
 openrac_api api{};GuestArena arena;
 int current=2,notifications=0;
 explicit Fixture(const char* version):region(*RegionBindings::for_version(version)){
  live=this;api.memory=ram.data();api.version=version;api.game="rac1";
  api.loaded_level=+[]{return live->current;};
  // Deliberately relocate SDK keys; the production guard must use the
  // region's canonical mode binding, not a hardcoded PAL/NTSC RAM address.
  api.relocate_data=+[](unsigned key){return key+0x100000u;};
  api.relocate_level_data=+[](int,unsigned key){return key+0x200000u;};
  api.setting_bool=+[](openrac_mod*,const char*){return 1;};
  api.function=+[](const char* name)->void*{
   const auto is=[name](const char* canonical){return !std::strcmp(name,live->region.function(canonical));};
   if(is("FUN_001ffc30"))return reinterpret_cast<void*>(+[](int,int,int,int,int,int){});
   if(is("FUN_L00_0023a7a8"))return reinterpret_cast<void*>(+[](unsigned){return 0;});
   if(is("FUN_00200e08"))return reinterpret_cast<void*>(+[](int,int,int,int,std::uint64_t,int){});
   if(is("FUN_001f5450"))return reinterpret_cast<void*>(+[](int,int,int,int,int,int,int,int,std::uint64_t,std::uint64_t){});
   if(is("FUN_001f44b8"))return reinterpret_cast<void*>(+[](int)->std::uint64_t{return 0;});
   if(is("FUN_L00_002a5dd8"))return reinterpret_cast<void*>(+[](unsigned){});
   if(is("FUN_L00_00263d40"))return reinterpret_cast<void*>(+[](int,int){++live->notifications;});
   return nullptr;
  };
  arena.reserve(Arena,0x20000);
  put(g(0x15f6a0),TextBank);put<int>(g(0x1996fc),2);
  put(TextBank,std::array<unsigned,4>{{Name,20037,0,0}});
  put(TextBank+16,std::array<unsigned,4>{{Description,20345,0,0}});
  std::memcpy(ram.data()+Name,"Walloper",9);
  std::memcpy(ram.data()+Description,"Original description",21);
  const auto definition=g(0x1863d0)+18*76;
  put<int>(definition,20037);put<short>(definition+0x40,20345);
  put<int>(g(localization::native_language_address),5);
  put<int>(mode(),0);
  const auto state=g(0x19a3e8);
  put(state+0x18,Header);put(state+0x1c,Defs);put(state+0x20,Refs);
  put(state+0x24,Images);put(state+0x28,Palettes);
  put(Header+0x44,1u);put(Header+0x24,1u);
  put<unsigned short>(Defs,59901);put<unsigned short>(Defs+2,14);
  put<unsigned short>(Defs+4,0);put<unsigned short>(Defs+8,0xffff);
  for(unsigned i=0;i<14;++i)put(Refs+i*4,0u);
  put(Images,0x420000u);put<unsigned short>(Images+6,0x0505);
  put(Palettes,0x421000u);
  for(unsigned i=0;i<1024;++i)ram[0x420000+i]=static_cast<unsigned char>(i%3);
  for(unsigned i=1;i<256;++i){ram[0x421000+i*4]=static_cast<unsigned char>(i);ram[0x421000+i*4+3]=128;}
 }
 unsigned g(unsigned key)const{return region.data(key)+0x100000;}
 unsigned mode()const{return region.level(0,0x15f5c4)+0x200000;}
 template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
};
void run(const char* version){
 Fixture f(version);Memory memory(&f.api);Presentation p(&f.api,nullptr,&f.arena);
 require(!memory.level_transitioning(),"normal gameplay owns its level arena");
 p.select_identity(true);p.frame(false);
 const auto replacement=f.get<unsigned>(TextBank);
 require(f.arena.owns(replacement,512),"names are allocated in the actual old Aridia arena");
 require(!std::strcmp(reinterpret_cast<const char*>(f.ram.data()+replacement),"Rotatore"),"normal localized identity works before travel");
 const auto state=f.g(0x19a3e8),icon_refs=f.get<unsigned>(state+0x20);
 require(f.arena.owns(icon_refs,19*4)&&icon_refs!=Refs,"native icon extension is installed before loading");
 f.put<int>(f.mode(),6);
 require(memory.level_transitioning(),"canonical regional mode6 invalidates level ownership even with same loaded_level");
 auto before=f.ram;
 p.set_golden(true);p.names();p.frame(true);p.select_identity(false);
 require(f.ram==before,"mode6 preserves existing arena, name bindings and gold icon references byte-exactly");
 const auto live_arena=before;
 // The native loader reuses that exact old arena as incoming compressed WAD
 // input before LoadGameplay/before_load. Deliberately destroy every cached
 // owned byte: no reset/rebinding/icon cleanup may touch the incoming data.
 for(unsigned i=0;i<0x20000;++i)f.ram[Arena+i]=static_cast<unsigned char>((i*37+113)%251);
 const auto saved_bank=f.get<unsigned>(f.g(0x15f6a0));
 f.put(f.g(0x15f6a0),TextBank+0x200u); // changed bank must not trigger reset
 before=f.ram;
 p.frame(false);p.names();p.select_identity(true);p.set_golden(false);
 require(f.ram==before,"no byte of overlapping WAD or foreign/new text bank is changed in loading mode");
 // Restore the live native tables as a same-level resume control. Real native
 // travel rebuilds them through the gameplay load hook; no mode0 action is
 // suppressed by the loading guard.
 std::memcpy(f.ram.data()+Arena,live_arena.data()+Arena,0x20000);
 f.put(f.g(0x15f6a0),saved_bank);
 f.put<int>(f.mode(),0);
 p.names();
 require(!std::strcmp(reinterpret_cast<const char*>(f.ram.data()+replacement),"Rotatore"),"identity resumes when a live level returns to mode0");
 require(f.get<unsigned>(TextBank)==replacement,"normal identity rebinds after the mode6 hold");
 for(const int allowed:{3,5}){
  f.put<int>(f.mode(),allowed);require(!memory.level_transitioning(),"pause/menu and vendor modes retain normal ownership");
  p.set_golden(true);
  require(!std::strcmp(reinterpret_cast<const char*>(f.ram.data()+replacement),"Rotatore d'oro"),"gold localized identity remains active in existing native menus");
  p.set_golden(false);
 }
 f.put<int>(f.mode(),0);
 for(const int level:{0,18}){f.current=level;require(!memory.level_transitioning(),"both campaign boundary levels remain supported");}
 for(const int level:{-1,19}){f.current=level;before=f.ram;
  require(memory.level_transitioning(),"absent/out-of-campaign level refuses cached storage");
  p.names();p.frame(false);p.set_golden(true);require(f.ram==before,"out-of-level callers cannot write guest state");
 }
 f.current=2;f.put<int>(f.mode(),6); // reset checks must not see arbitrary random clones as owned
}
}
int main(){run("pal");run("ntsc");std::printf("loading guard: %u PAL/NTSC overlap, identity and menu checks passed\n",checks);}

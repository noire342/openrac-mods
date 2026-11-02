// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_suck_cannon.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
using namespace salvage;
namespace {
unsigned checks;
#define CHECK(value) do{++checks;if(!(value)){std::fprintf(stderr,"cannon %d: %s\n",__LINE__,#value);std::exit(1);}}while(false)
struct Fixture;Fixture* live;
struct Fixture final:CannonWorldPort {
 static constexpr unsigned Actor=0x1010000,Class=0x1b0000,Vars=0x1c0000,Collision=0x1d0000,Muzzle=0x1e0000,Velocity=Muzzle+16,Cannon=0x1020000;
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);openrac_api api{};GuestArena arena;World world;Catalog catalog;Inventory inventory;
 CannonNativeBody body{};bool mapped=true,claim_fail=false,capture_fail=false,cancel_fail=false,arrive=false,terminal=false,bad_guest_handler=false;
 unsigned claims{},cancels{},captures{},retires{},explosions{},deletes{},launches{};int current_level=1;
 std::unique_ptr<NativeSuckCannon> cannon;
 template<class T>T get(unsigned at)const{T out{};std::memcpy(&out,ram.data()+at,sizeof out);return out;}
 template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
 static unsigned relocate(unsigned at){return at;}static unsigned level_data(int,unsigned at){return at;}static int level(){return live->current_level;}
 static void matrix(unsigned actor){const auto pc=live->get<unsigned>(actor+0x24),sequence=live->get<unsigned>(pc+0x48);CHECK(sequence==Class+0x1000&&live->get<unsigned>(sequence)==0x11223344);const auto pos=live->get<Vec3>(actor+0x10);live->put(actor,std::array<float,4>{{pos.x*1024,pos.y*1024,pos.z*1024,256}});}
 static void remove(unsigned actor){++live->deletes;live->put<unsigned char>(actor+0x20,0xfd);}
 static int ticks(int value){return value;}
 static unsigned native_record(unsigned actor){
  if(live->cannon->handles_record(actor))return live->cannon->record(actor);
  if(live->get<unsigned char>(actor+0x20)>=0xfd)return 0;
  const auto pc=live->get<unsigned>(actor+0x24),table=live->get<unsigned>(pc+0x2c);const auto callback=live->get<unsigned>(table+16);
  CHECK(callback!=0x3054d8);return 0; // self callback must never fall through
 }
 static unsigned inline_record(unsigned actor){const auto pc=live->get<unsigned>(actor+0x24),table=live->get<unsigned>(pc+0x2c),callback=live->get<unsigned>(table+16);if(!callback)return 0;const auto getter=reinterpret_cast<unsigned(*)(unsigned)>(function_at(callback));CHECK(getter!=nullptr);return getter(actor);}
 static int generic_grab(unsigned actor,unsigned muzzle,unsigned cannon_actor){return live->cannon->handles_record(actor)?live->cannon->grab(actor,muzzle,cannon_actor):0;}static int generic_store(unsigned,unsigned){return 1;}static int generic_cancel(unsigned){return 0;}
 static int original_launch(unsigned actor,unsigned velocity,unsigned target,float height){
  ++live->launches;const auto record=live->cannon->record(actor);live->put(record+0x10,live->get<std::array<float,4>>(velocity));live->put<short>(record+0x68,5);live->put<int>(record+0x94,300);live->put(record+0x74,target);live->put(record+0x90,height);live->put<unsigned char>(actor+0x31,1);live->put<unsigned short>(actor+0x34,live->get<unsigned short>(actor+0x34)&~3u);return 2;
 }
 static void original_finalize(unsigned actor,unsigned record,int explode){const auto pc=live->get<unsigned>(actor+0x24),table=live->get<unsigned>(pc+0x2c);CHECK(live->get<unsigned>(table+20)==0);if(explode)++live->explosions;live->put<short>(record+0x68,0);remove(actor);}
 static int carried(unsigned actor,unsigned record){
  CHECK(inline_record(actor)==record);
  const auto state=live->get<short>(record+0x68);
  if(state==3&&live->arrive){const auto hero=NativeMemory::Hero;live->put<int>(hero+0x207c,live->get<int>(hero+0x207c)-1);live->put<int>(hero+0x2078,live->get<int>(hero+0x2078)+1);live->cannon->store(actor,Muzzle);
   const auto index=live->get<short>(record+0x6a);CHECK(index>=0);const auto slot=live->get<unsigned>(hero+0x2050+unsigned(index)*4);CHECK(slot==actor);live->put<unsigned short>(slot+0x34,live->get<unsigned short>(slot+0x34)&~0x1000u);
  }else if((state==5||state==6)&&live->terminal)live->cannon->finalize(actor,record,1,original_finalize);
  return 0;
 }
 static void* function(const char* name){
  if(!std::strcmp(name,"func_L00_002DDEA0"))return reinterpret_cast<void*>(&carried);
  if(!std::strcmp(name,"func_L00_002DCD40"))return reinterpret_cast<void*>(&native_record);
  if(!std::strcmp(name,"func_0020EEE8"))return reinterpret_cast<void*>(&matrix);
  if(!std::strcmp(name,"func_0020D678"))return reinterpret_cast<void*>(&remove);
  if(!std::strcmp(name,"func_001F9850"))return reinterpret_cast<void*>(&ticks);
  if(!std::strcmp(name,"func_L00_002DCDA8"))return reinterpret_cast<void*>(&generic_grab);
  if(!std::strcmp(name,"func_L00_002DCFD0"))return reinterpret_cast<void*>(&generic_store);
  if(!std::strcmp(name,"func_L00_002DD0E0"))return reinterpret_cast<void*>(&original_launch);
  if(!std::strcmp(name,"func_L00_002DD2D0"))return reinterpret_cast<void*>(&generic_cancel);return nullptr;
 }
 static void* function_at(unsigned at){if(live->bad_guest_handler)return nullptr;switch(at){case 0x3054d8:return function("func_L00_002DCD40");case 0x305540:return function("func_L00_002DCDA8");case 0x305768:return function("func_L00_002DCFD0");case 0x305878:return function("func_L00_002DD0E0");case 0x305a68:return function("func_L00_002DD2D0");default:return nullptr;}}
 Fixture(){
  live=this;api.size=sizeof api;api.version="pal";api.memory=ram.data();api.function=function;api.function_at=function_at;api.relocate_data=relocate;api.relocate_level_data=level_data;api.loaded_level=level;
  arena.reserve(0x1800000,0x20000);world.begin_visit(1);const auto instance=world.spawn(1234,{{3,0,1},{},{},{}});
  PartDescriptor part{};part.id=1234;part.stable_id="test.real.part";part.name_it="Parte";part.name_en="Part";part.source_it="Robot";part.source_en="Robot";CHECK(catalog.register_part(part)==Status::Ok);
  body.fragment=*world.find(instance);body.actor=Actor;body.pclass=Class;body.pvars=Vars;body.collision=Collision;body.update=0x256268;body.scale=1;
  put<unsigned>(Actor+0x24,Class);put<unsigned>(Actor+0x78,Vars);put<unsigned>(Actor+0x74,body.update);put<unsigned>(Actor+0x94,Collision);put<unsigned char>(Actor+0x20,1);put<float>(Actor+0x2c,1);put<unsigned char>(Actor+0x53,0);put<float>(Actor+0x58,1);put<unsigned short>(Actor+0xa6,1736);put<Vec3>(Actor+0x10,{3,0,1});put<float>(Actor+12,256);
  put<unsigned>(Class+0x10,0x123456);put<float>(Class+0x24,1);put<unsigned>(Class+0x2c,0x123000);put<unsigned>(Class+4,0x22334455);put<unsigned char>(Class+12,3);for(unsigned i=0;i<3;++i){put<unsigned>(Class+0x48+i*4,Class+0x1000+i*0x100);put<unsigned>(Class+0x1000+i*0x100,0x11223344+i);}
  put<std::array<float,4>>(Muzzle,{{0,0,1,0}});put<std::array<float,4>>(Velocity,{{.5f,.25f,.01f,0}});put<unsigned char>(Cannon+0x20,1);
  cannon=std::make_unique<NativeSuckCannon>(&api,arena,*this);
 }
 std::optional<CannonNativeBody> cannon_describe(unsigned actor)const override{if(actor!=Actor||!mapped||!world.find(body.fragment.id)||world.claimed(body.fragment.id))return {};auto copy=body;copy.fragment=*world.find(body.fragment.id);copy.lease={};return copy;}
 bool cannon_replace_class(unsigned actor,unsigned expected,unsigned replacement)override{if(!mapped||actor!=Actor||get<unsigned>(actor+0x24)!=expected||body.pclass!=expected)return false;put(actor+0x24,replacement);body.pclass=replacement;return true;}
 std::optional<CannonNativeBody> cannon_claim(unsigned actor)override{
  if(claim_fail||actor!=Actor||!mapped)return {};auto copy=body;copy.fragment=*world.find(body.fragment.id);if(world.claim(body.fragment.id,body.fragment.visit,copy.lease)!=Status::Ok)return {};mapped=false;++claims;return copy;
 }
 bool cannon_cancel(const CannonNativeBody& value,bool native_valid)override{if(cancel_fail||!world.owns(value.lease))return false;if(native_valid){body=value;body.lease={};mapped=true;}if(!world.release(value.lease))return false;++cancels;return true;}
 bool cannon_capture(const CannonNativeBody& value)override{if(capture_fail||!world.take(value.lease))return false;++captures;return true;}
 void cannon_retire(const CannonNativeBody& value)override{CHECK(value.actor==Actor&&value.collision==Collision);++retires;}
 int candidate(){return cannon->candidate(Cannon,0,Actor,cannon->record(Actor),Muzzle);}
 void capture(){CHECK(cannon->offer(Actor));CHECK(candidate()==2);arrive=true;CHECK(cannon->update(Actor));CHECK(world.all().empty()&&captures==1&&get<int>(NativeMemory::Hero+0x2078)==1);}
 void fire(){put<unsigned>(NativeMemory::Hero+0x2050,0);put<int>(NativeMemory::Hero+0x2078,0);CHECK(cannon->launch(Actor,Velocity,0,.5f,original_launch)==2);}
};
}
int main(){
 {Fixture f;const auto original=f.get<std::array<unsigned char,0x54>>(Fixture::Class);CHECK(f.cannon->offer(Fixture::Actor));CHECK(f.cannon->owns(Fixture::Actor)&&f.cannon->record(Fixture::Actor));CHECK(f.cannon->tracked()==1);const auto clone=f.get<unsigned>(Fixture::Actor+0x24);CHECK(clone!=Fixture::Class);for(unsigned i=0;i<3;++i)CHECK(f.get<unsigned>(clone+0x48+i*4)==Fixture::Class+0x1000+i*0x100);CHECK((f.get<std::array<unsigned char,0x54>>(Fixture::Class)==original));CHECK(f.get<unsigned short>(Fixture::Actor+0xa6)==1736);CHECK(f.world.all().size()==1&&!f.world.claimed(f.body.fragment.id));CHECK(!f.cannon->update(Fixture::Actor));CHECK(f.cannon->offer(Fixture::Actor)&&f.cannon->tracked()==1);f.cannon->reset();CHECK(f.get<unsigned>(Fixture::Actor+0x24)==Fixture::Class&&f.world.all().size()==1&&f.deletes==0);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));f.claim_fail=true;CHECK(f.candidate()==0);CHECK(!f.world.claimed(f.body.fragment.id)&&f.mapped&&f.get<int>(NativeMemory::Hero+0x207c)==0);f.claim_fail=false;CHECK(f.candidate()==2);CHECK(f.world.claimed(f.body.fragment.id)&&!f.mapped&&f.claims==1);CHECK(f.get<unsigned>(NativeMemory::Hero+0x2050)==Fixture::Actor&&f.get<int>(NativeMemory::Hero+0x207c)==1);CHECK(f.candidate()==2&&f.claims==1&&f.get<int>(NativeMemory::Hero+0x207c)==1);Events events;Collector collector(f.catalog,f.inventory,f.world,events);const auto blocked=collector.update({true,true,true,false,{3,0,1}},[](const Fragment&){return true;});CHECK(blocked.status==Status::NotFound&&f.inventory.count(1234)==0);CHECK(f.cannon->cancel(Fixture::Actor)==0);CHECK(!f.world.claimed(f.body.fragment.id)&&f.world.all().size()==1&&f.mapped&&f.cancels==1);CHECK(f.get<unsigned>(NativeMemory::Hero+0x2050)==0&&f.get<int>(NativeMemory::Hero+0x207c)==0);CHECK(f.get<float>(Fixture::Actor+0x2c)==1&&f.deletes==0);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));const auto record=Fixture::inline_record(Fixture::Actor);CHECK(record==f.cannon->record(Fixture::Actor)&&record!=0);const auto pc=f.get<unsigned>(Fixture::Actor+0x24),table=f.get<unsigned>(pc+0x2c);const auto grab=reinterpret_cast<int(*)(unsigned,unsigned,unsigned)>(Fixture::function_at(f.get<unsigned>(table)));CHECK(grab&&grab(Fixture::Actor,Fixture::Muzzle,Fixture::Cannon)==2);CHECK(f.get<short>(record+0x68)==3&&f.claims==1&&f.get<int>(NativeMemory::Hero+0x207c)==1);CHECK(grab(Fixture::Actor,Fixture::Muzzle,Fixture::Cannon)==2&&f.claims==1&&f.get<int>(NativeMemory::Hero+0x207c)==1);CHECK(f.cannon->cancel(Fixture::Actor)==0);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));CHECK(f.candidate()==2);f.cancel_fail=true;CHECK(f.cannon->cancel(Fixture::Actor)==0&&f.world.claimed(f.body.fragment.id));CHECK(f.get<unsigned>(NativeMemory::Hero+0x2050)==0);f.cancel_fail=false;f.cannon->frame();CHECK(!f.world.claimed(f.body.fragment.id)&&f.mapped&&f.cancels==1);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));CHECK(f.candidate()==2);f.put<unsigned>(NativeMemory::Hero+0x2050,0);f.put<int>(NativeMemory::Hero+0x207c,0);f.arrive=true;CHECK(f.cannon->update(Fixture::Actor));CHECK(!f.world.claimed(f.body.fragment.id)&&f.cancels==1&&f.captures==0&&f.world.all().size()==1);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));f.put<int>(NativeMemory::Hero+0x2078,5);CHECK(f.candidate()==0&&f.claims==0);f.put<int>(NativeMemory::Hero+0x2078,0);for(unsigned i=0;i<5;++i)f.put<unsigned>(NativeMemory::Hero+0x2050+i*4,100+i);CHECK(f.candidate()==0&&f.claims==0);f.put<unsigned char>(0x13e620+9,1);CHECK(f.candidate()==2&&f.get<unsigned>(NativeMemory::Hero+0x2050+5*4)==Fixture::Actor);CHECK(f.cannon->cancel(Fixture::Actor)==0&&f.cancels==1);}
 {Fixture f;f.capture();CHECK(f.cannon->owns(Fixture::Actor)&&f.get<unsigned short>(Fixture::Actor+0xa6)==1736);CHECK(f.get<short>(f.cannon->record(Fixture::Actor)+0x68)==4&&f.get<float>(Fixture::Actor+0x2c)==0);CHECK(f.cannon->store(Fixture::Actor,Fixture::Muzzle)==1&&f.captures==1);CHECK(f.cannon->cancel(Fixture::Actor)==1&&f.world.all().empty());f.fire();CHECK(f.launches==1&&f.world.all().empty()&&f.inventory.count(1234)==0);CHECK(f.cannon->launch(Fixture::Actor,Fixture::Velocity,0,0,Fixture::original_launch)==0&&f.launches==1);CHECK(f.get<unsigned short>(Fixture::Actor+0xa6)==1736);f.terminal=true;CHECK(f.cannon->update(Fixture::Actor));CHECK(f.explosions==1&&f.deletes==1&&f.retires==1&&f.cannon->tracked()==0);CHECK(f.cannon->owns(Fixture::Actor)&&!f.cannon->record(Fixture::Actor)&&!f.cannon->update(Fixture::Actor));f.cannon->frame();CHECK(f.cannon->owns(Fixture::Actor)&&f.retires==1);f.put<unsigned>(Fixture::Actor+0x78,Fixture::Vars+4);f.cannon->frame();CHECK(!f.cannon->owns(Fixture::Actor)&&f.retires==1&&f.world.all().empty());}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));CHECK(f.candidate()==2);f.capture_fail=true;f.arrive=true;CHECK(f.cannon->update(Fixture::Actor));CHECK(f.world.all().size()==1&&!f.world.claimed(f.body.fragment.id)&&f.cancels==1&&f.captures==0);CHECK(f.get<unsigned>(NativeMemory::Hero+0x2050)==0&&f.get<int>(NativeMemory::Hero+0x2078)==0&&f.get<int>(NativeMemory::Hero+0x207c)==0);CHECK(f.mapped&&f.get<float>(Fixture::Actor+0x2c)==1&&f.deletes==0);}
 {Fixture f;f.capture();f.cannon->reset();CHECK(f.world.all().empty()&&f.deletes==1&&f.explosions==0&&f.retires==1);CHECK(f.get<unsigned>(NativeMemory::Hero+0x2050)==0&&f.get<int>(NativeMemory::Hero+0x2078)==0);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));CHECK(f.candidate()==2);f.put<unsigned>(Fixture::Actor+0x78,Fixture::Vars+4);CHECK(!f.cannon->owns(Fixture::Actor));f.cannon->frame();CHECK(f.cannon->tracked()==0&&!f.world.claimed(f.body.fragment.id)&&f.world.all().size()==1&&f.deletes==0);}
 {Fixture f;CHECK(f.cannon->offer(Fixture::Actor));const auto old=f.body.fragment.id;const auto clone=f.get<unsigned>(Fixture::Actor+0x24);CHECK(f.world.erase(old));const auto next=f.world.spawn(1234,{{3,0,1},{},{},{}});f.body.fragment=*f.world.find(next);CHECK(next!=old&&f.get<unsigned>(Fixture::Actor+0x24)==clone&&f.get<unsigned>(Fixture::Actor+0x78)==Fixture::Vars);CHECK(!f.cannon->owns(Fixture::Actor)&&!f.cannon->record(Fixture::Actor));CHECK(f.cannon->handles_record(Fixture::Actor)&&Fixture::inline_record(Fixture::Actor)==0);f.cannon->frame();CHECK(f.cannon->tracked()==0&&f.deletes==0);CHECK(f.cannon->offer(Fixture::Actor)&&f.cannon->owns(Fixture::Actor));CHECK(f.candidate()==2&&f.world.claimed(next)&&!f.world.find(old));CHECK(f.cannon->cancel(Fixture::Actor)==0&&f.world.all().size()==1&&!f.world.claimed(next));}
 {Fixture f;f.capture();f.fire();f.terminal=true;CHECK(f.cannon->update(Fixture::Actor));CHECK(f.cannon->handles_record(Fixture::Actor)&&Fixture::inline_record(Fixture::Actor)==0);const auto pc=f.get<unsigned>(Fixture::Actor+0x24),table=f.get<unsigned>(pc+0x2c);f.cannon->reset();CHECK(f.get<unsigned>(table+16)==0&&Fixture::inline_record(Fixture::Actor)==0);}
 {Fixture f;f.bad_guest_handler=true;CHECK(!f.cannon->offer(Fixture::Actor)&&f.get<unsigned>(Fixture::Actor+0x24)==Fixture::Class);f.bad_guest_handler=false;f.arena.reserve(0x1800000,16);CHECK(!f.cannon->offer(Fixture::Actor)&&f.world.all().size()==1&&!f.world.claimed(f.body.fragment.id));}
 for(int state=0;state<4;++state){
  Fixture f;CHECK(f.cannon->offer(Fixture::Actor));
  if(state==1)CHECK(f.candidate()==2);
  if(state>=2){f.arrive=true;CHECK(f.candidate()==2);CHECK(f.cannon->update(Fixture::Actor));}
  if(state==3)f.fire();
  f.put<int>(0x15f6a8,6);const auto before=f.ram;const auto deletes=f.deletes,explosions=f.explosions;
  f.cannon->reset();
  CHECK(f.ram==before&&f.deletes==deletes&&f.explosions==explosions);
  CHECK(f.cannon->tracked()==0&&!f.world.claimed(f.body.fragment.id));
  CHECK(f.world.all().size()==(state<2?1u:0u));
 }
 std::printf("native cannon adapter host: %u checks passed\n",checks);
}

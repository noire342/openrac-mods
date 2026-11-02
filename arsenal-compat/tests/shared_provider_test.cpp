// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "shared_provider.hpp"
#include "legacy_facade.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
using namespace arsenal_compat;
namespace {
unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
std::vector<int> calls;
void native_prepare(std::int32_t a,std::int32_t b){calls.push_back(100+a+b);}
std::int32_t native_autosave(std::int32_t a,std::int32_t b){calls.push_back(200+a+b);return 40+a+b;}
float factory_scale{};std::array<std::uint32_t,9> factory_arguments{};
std::uint32_t native_fragment(std::uint32_t a,std::int32_t b,std::uint32_t c,std::uint32_t d,std::int32_t e,std::int32_t f,std::uint32_t g,std::uint32_t h,float scale,std::uint32_t j){
 factory_scale=scale;factory_arguments={a,std::uint32_t(b),c,d,std::uint32_t(e),std::uint32_t(f),g,h,j};return 0x1234;
}
void noop(){}
void native_hud(){calls.push_back(900);}
void(*next_hud_a)(){};void(*next_hud_b)(){};
void hud_a(){calls.push_back(11);next_hud_a();calls.push_back(-11);}
void hud_b(){calls.push_back(12);next_hud_b();calls.push_back(-12);}
struct ResolverFake:Resolver{
 std::string region="pal",digest=SupportedPalExecutable;unsigned lookups{};int missing=-1,unsafe=-1;
 const char* version()const noexcept override{return region.c_str();}
 bool executable_digest(std::string& out)override{out=digest;return true;}
 void* resolve(const char* symbol)override{const auto& specs=shared_hook_specs(region=="pal"?Region::Pal:Region::Ntsc);++lookups;for(std::size_t i=0;i<specs.size();++i)if(!std::strcmp(symbol,specs[i].symbol))return int(i)==missing?nullptr:target(i);return nullptr;}
 bool main_executable_target(void* p)override{return unsafe<0||p!=target(std::size_t(unsafe));}
 static void* target(std::size_t i){if(i==22)return reinterpret_cast<void*>(&native_prepare);if(i==23)return reinterpret_cast<void*>(&native_autosave);if(i==31)return reinterpret_cast<void*>(&native_fragment);if(i==45)return reinterpret_cast<void*>(&native_hud);return reinterpret_cast<void*>(std::uintptr_t(0x100000+i*256));}
};
struct BackendFake:Backend{
 struct Hook{void* entry;bool active{},queued{};};std::map<void*,Hook> hooks;int create_fail=-1,queue_fail=-1,create_calls{},queue_calls{},apply_fail{};void* disable_fail{};void* remove_fail{};
 int initialize()override{return 0;}
 int create(void* target,void* entry,void** original)override{if(create_calls++==create_fail)return 91;if(hooks.count(target))return 92;hooks[target]={entry};*original=target;return 0;}
 int queue_enable(void* t)override{if(queue_calls++==queue_fail)return 93;hooks.at(t).queued=true;return 0;}
 int queue_disable(void* t)override{hooks.at(t).queued=false;return 0;}
 int apply()override{for(auto& pair:hooks)pair.second.active=pair.second.queued;if(apply_fail){--apply_fail;return 94;}return 0;}
 int disable(void* t)override{if(t==disable_fail)return 95;hooks.at(t).active=hooks.at(t).queued=false;return 0;}
 int remove(void* t)override{if(t==remove_fail)return 96;CHECK(!hooks.at(t).active);hooks.erase(t);return 0;}
 void prepare(int a,int b){auto it=hooks.find(ResolverFake::target(22));if(it!=hooks.end()&&it->second.active)reinterpret_cast<void(*)(int,int)>(it->second.entry)(a,b);else native_prepare(a,b);}
 int autosave(int a,int b){auto it=hooks.find(ResolverFake::target(23));return it!=hooks.end()&&it->second.active?reinterpret_cast<int(*)(int,int)>(it->second.entry)(a,b):native_autosave(a,b);}
};
struct Modules:ModuleLifetime{std::map<void*,unsigned> refs;bool reject{};void* retain(void* p)override{if(reject)return nullptr;++refs[p];return p;}void release(void* p)noexcept override{auto it=refs.find(p);CHECK(it!=refs.end()&&it->second);if(!--it->second)refs.erase(it);}};
struct Fixture{ResolverFake resolver;BackendFake backend;Modules modules;SharedProvider shared{resolver,backend,modules};LegacyFacade legacy{shared};Fixture(){calls.clear();}void ready(){CHECK(shared.preflight()==0);}~Fixture(){legacy.rollback();}};
using Prepare=void(*)(int,int);using Autosave=int(*)(int,int);
Prepare next_a{},next_b{},next_c{},next_legacy{};Autosave next_save{};
using Factory=std::uint32_t(*)(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::int32_t,std::int32_t,std::uint32_t,std::uint32_t,float,std::uint32_t);
Factory next_factory{};
std::uint32_t factory(std::uint32_t a,std::int32_t b,std::uint32_t c,std::uint32_t d,std::int32_t e,std::int32_t f,std::uint32_t g,std::uint32_t h,float scale,std::uint32_t j){return next_factory(a,b,c,d,e,f,g,h,scale+1,j)+7;}

SharedProvider* reentry{};SharedProvider::Token reentry_token{};int nested_result{};
void a(int x,int y){calls.push_back(1);if(reentry)nested_result=reentry->release(reentry_token);next_a(x,y);calls.push_back(-1);}
void b(int x,int y){calls.push_back(2);next_b(x,y);calls.push_back(-2);}
void c(int x,int y){calls.push_back(3);next_c(x,y);calls.push_back(-3);}
void legacy_prepare(int x,int y){calls.push_back(4);next_legacy(x,y);calls.push_back(-4);}
int save(int x,int y){return next_save(x,y)+100;}
void bind_prepare(SharedProvider& p,SharedProvider::Token t,Prepare fn,Prepare& next){CHECK(p.bind(t,"rac1.save.prepare","void(i32,i32)",reinterpret_cast<void*>(fn),reinterpret_cast<void**>(&next))==0);CHECK(next);}
void bind_legacy(Fixture& f){const auto& specs=hook_specs(Region::Pal);for(std::size_t i=0;i<OPENRAC_ARSENAL_REQUIRED_HOOKS;++i){void* original{};CHECK(f.legacy.bind(specs[i].id,specs[i].signature,i==22?reinterpret_cast<void*>(&legacy_prepare):reinterpret_cast<void*>(&noop),&original)==0);if(i==22)next_legacy=reinterpret_cast<Prepare>(original);}}
}
int main(){
 {Fixture f;f.resolver.digest="wrong";CHECK(f.shared.preflight()==ARSENAL_HOOKS_BUILD);CHECK(f.resolver.lookups==0);}
 {Fixture f;f.resolver.unsafe=22;CHECK(f.shared.preflight()==ARSENAL_HOOKS_UNSAFE_TARGET);}
 {Fixture f;f.ready();SharedProvider::Token t=99;CHECK(f.shared.acquire("",&t)==ARSENAL_HOOKS_ARGUMENT&&t==0);CHECK(f.shared.acquire("first",&t)==0&&t);SharedProvider::Token duplicate{};CHECK(f.shared.acquire("first",&duplicate)==ARSENAL_HOOKS_CONSUMER);CHECK(f.shared.commit(t)==ARSENAL_HOOKS_INCOMPLETE);void* output=reinterpret_cast<void*>(1);CHECK(f.shared.bind(t,"missing","void()",reinterpret_cast<void*>(&noop),&output)==ARSENAL_HOOKS_UNKNOWN&&output==nullptr);CHECK(f.shared.bind(t,"rac1.save.prepare","void()",reinterpret_cast<void*>(&a),&output)==ARSENAL_HOOKS_SIGNATURE);CHECK(f.modules.refs.empty());bind_prepare(f.shared,t,a,next_a);CHECK(f.shared.commit(t)==0);CHECK(f.shared.commit(t)==ARSENAL_HOOKS_COMMITTED);CHECK(f.shared.release(t)==0);CHECK(f.modules.refs.empty());CHECK(f.shared.release(t)==ARSENAL_HOOKS_NOT_CLAIMED);}
 for(unsigned remove=0;remove<3;++remove){Fixture f;f.ready();SharedProvider::Token ta{},tb{},tc{};CHECK(f.shared.acquire("a",&ta)==0);CHECK(f.shared.acquire("b",&tb)==0);CHECK(f.shared.acquire("c",&tc)==0);bind_prepare(f.shared,ta,a,next_a);bind_prepare(f.shared,tb,b,next_b);bind_prepare(f.shared,tc,c,next_c);CHECK(f.backend.hooks.size()==1);CHECK(f.shared.commit(ta)==0);CHECK(f.shared.commit(tb)==0);CHECK(f.shared.commit(tc)==0);f.backend.prepare(4,5);CHECK((calls==std::vector<int>{3,2,1,109,-1,-2,-3}));calls.clear();CHECK(f.shared.release(remove==0?ta:remove==1?tb:tc)==0);f.backend.prepare(4,5);if(remove==0)CHECK((calls==std::vector<int>{3,2,109,-2,-3}));if(remove==1)CHECK((calls==std::vector<int>{3,1,109,-1,-3}));if(remove==2)CHECK((calls==std::vector<int>{2,1,109,-1,-2}));if(remove!=0)CHECK(f.shared.release(ta)==0);if(remove!=1)CHECK(f.shared.release(tb)==0);if(remove!=2)CHECK(f.shared.release(tc)==0);CHECK(f.modules.refs.empty()&&f.backend.hooks.empty());calls.clear();f.backend.prepare(1,2);CHECK((calls==std::vector<int>{103}));}
 {Fixture f;f.ready();CHECK(f.legacy.claim("revolverator")==0);CHECK(f.legacy.claim("second-legacy")==ARSENAL_HOOKS_CONSUMER);CHECK(f.legacy.commit()==ARSENAL_HOOKS_INCOMPLETE);bind_legacy(f);CHECK(f.legacy.commit()==0);arsenal_hooks_diagnostics_v1 d{};d.size=sizeof d;CHECK(f.legacy.diagnostics(&d)==0&&d.version==1&&d.required==30&&d.bound==30);SharedProvider::Token t{};CHECK(f.shared.acquire("robot-salvage",&t)==0);bind_prepare(f.shared,t,a,next_a);CHECK(f.shared.commit(t)==0);f.backend.prepare(1,2);CHECK((calls==std::vector<int>{1,4,103,-4,-1}));CHECK(f.legacy.rollback()==0);calls.clear();f.backend.prepare(1,2);CHECK((calls==std::vector<int>{1,103,-1}));CHECK(f.shared.release(t)==0);CHECK(f.modules.refs.empty()&&f.backend.hooks.empty());}
 {Fixture f;f.ready();SharedProvider::Token t{};CHECK(f.shared.acquire("salvage",&t)==0);bind_prepare(f.shared,t,a,next_a);CHECK(f.shared.commit(t)==0);CHECK(f.legacy.claim("revolverator")==0);bind_legacy(f);CHECK(f.legacy.commit()==0);f.backend.prepare(1,2);CHECK((calls==std::vector<int>{4,1,103,-1,-4}));CHECK(f.shared.release(t)==0);calls.clear();f.backend.prepare(1,2);CHECK((calls==std::vector<int>{4,103,-4}));CHECK(f.legacy.rollback()==0);CHECK(f.modules.refs.empty());}
 {Fixture f;f.ready();SharedProvider::Token t{};CHECK(f.shared.acquire("result",&t)==0);CHECK(f.shared.bind(t,"rac1.save.request_autosave","i32(i32,i32)",reinterpret_cast<void*>(&save),reinterpret_cast<void**>(&next_save))==0);CHECK(f.shared.commit(t)==0);CHECK(f.backend.autosave(1,2)==143);CHECK(f.shared.release(t)==0);CHECK(f.backend.autosave(1,2)==43);}
 {Fixture f;f.ready();SharedProvider::Token t{};CHECK(f.shared.acquire("reentry",&t)==0);bind_prepare(f.shared,t,a,next_a);CHECK(f.shared.commit(t)==0);reentry=&f.shared;reentry_token=t;f.backend.prepare(0,0);reentry=nullptr;CHECK(nested_result==ARSENAL_HOOKS_BUSY&&f.shared.active(t));int result=0;std::thread other([&]{SharedProvider::Token out{};result=f.shared.acquire("thread",&out);});other.join();CHECK(result==ARSENAL_HOOKS_THREAD);CHECK(f.shared.release(t)==0);}
 for(unsigned mode=0;mode<2;++mode){Fixture f;f.ready();SharedProvider::Token existing{},t{};CHECK(f.shared.acquire("existing",&existing)==0);bind_prepare(f.shared,existing,a,next_a);CHECK(f.shared.commit(existing)==0);CHECK(f.shared.acquire("failure",&t)==0);CHECK(f.shared.bind(t,"rac1.save.request_autosave","i32(i32,i32)",reinterpret_cast<void*>(&save),reinterpret_cast<void**>(&next_save))==0);if(mode==0)f.backend.queue_fail=f.backend.queue_calls;else f.backend.apply_fail=1;CHECK(f.shared.commit(t)==ARSENAL_HOOKS_BACKEND);calls.clear();f.backend.prepare(1,2);CHECK((calls==std::vector<int>{1,103,-1}));CHECK(f.shared.release(t)==0);CHECK(f.shared.release(existing)==0);CHECK(f.backend.hooks.empty()&&f.modules.refs.empty());}
 for(bool remove_failure:{false,true}){Fixture f;f.ready();SharedProvider::Token t{};CHECK(f.shared.acquire("retry",&t)==0);bind_prepare(f.shared,t,a,next_a);CHECK(f.shared.commit(t)==0);if(remove_failure)f.backend.remove_fail=ResolverFake::target(22);else f.backend.disable_fail=ResolverFake::target(22);CHECK(f.shared.release(t)==ARSENAL_HOOKS_ROLLBACK);CHECK(!f.modules.refs.empty()&&f.shared.claimed());f.backend.disable_fail=f.backend.remove_fail=nullptr;CHECK(f.shared.release(t)==0);SharedProvider::Token newer{};CHECK(f.shared.acquire("retry",&newer)==0&&newer!=t);CHECK(f.shared.release(t)==ARSENAL_HOOKS_NOT_CLAIMED);CHECK(f.shared.release(newer)==0);}
 for(bool pal:{false,true}){Fixture f;if(!pal){f.resolver.region="ntsc";f.resolver.digest=SupportedExecutables[1];}f.ready();SharedProvider::Token t{};CHECK(f.shared.acquire("factory",&t)==0);CHECK(f.shared.bind(t,"rac1.moby.spawn_break_fragment","u32(u32,i32,u32,u32,i32,i32,u32,u32,f32,u32)",reinterpret_cast<void*>(&factory),reinterpret_cast<void**>(&next_factory))==0);CHECK(f.shared.commit(t)==0);auto callback=reinterpret_cast<Factory>(f.backend.hooks.at(ResolverFake::target(31)).entry);CHECK(callback(11,-22,33,44,-55,-66,77,88,.25f,99)==0x123b);CHECK(factory_scale==1.25f);CHECK((factory_arguments==std::array<std::uint32_t,9>{11,std::uint32_t(-22),33,44,std::uint32_t(-55),std::uint32_t(-66),77,88,99}));CHECK(f.shared.release(t)==0);}
 {Fixture f;f.ready();std::array<SharedProvider::Token,SharedConsumers> tokens{};for(std::size_t i=0;i<tokens.size();++i)CHECK(f.shared.acquire(("consumer-"+std::to_string(i)).c_str(),&tokens[i])==0);SharedProvider::Token full{};CHECK(f.shared.acquire("overflow",&full)==ARSENAL_HOOKS_CONSUMER);CHECK(f.shared.bind(tokens.back(),"rac1.save.request_autosave","i32(i32,i32)",reinterpret_cast<void*>(&save),reinterpret_cast<void**>(&next_save))==0);CHECK(f.shared.commit(tokens.back())==0);CHECK(f.backend.autosave(1,2)==143);for(const auto token:tokens)CHECK(f.shared.release(token)==0);CHECK(f.modules.refs.empty());}
 {Fixture f;f.ready();CHECK(f.legacy.claim("legacy")==0);void* original{};CHECK(f.legacy.claim("")==ARSENAL_HOOKS_ARGUMENT);CHECK(f.legacy.bind(nullptr,"void()",reinterpret_cast<void*>(&noop),&original)==ARSENAL_HOOKS_ARGUMENT);int result=0;std::thread thread([&]{result=f.legacy.rollback();});thread.join();CHECK(result==ARSENAL_HOOKS_THREAD);CHECK(f.legacy.rollback()==0);}
 for(bool pal:{false,true}){
  Fixture f;if(!pal){f.resolver.region="ntsc";f.resolver.digest=SupportedExecutables[1];}f.ready();
  const auto& spec=shared_hook_specs(pal?Region::Pal:Region::Ntsc).back();
  CHECK(!std::strcmp(spec.id,"rac1.hud.render")&&!std::strcmp(spec.signature,"void()"));
  SharedProvider::Token first{},second{};CHECK(f.shared.acquire("hud-a",&first)==0);CHECK(f.shared.acquire("hud-b",&second)==0);
  CHECK(f.shared.bind(first,spec.id,spec.signature,reinterpret_cast<void*>(&hud_a),reinterpret_cast<void**>(&next_hud_a))==0);
  CHECK(f.shared.bind(second,spec.id,spec.signature,reinterpret_cast<void*>(&hud_b),reinterpret_cast<void**>(&next_hud_b))==0);
  CHECK(f.shared.commit(first)==0&&f.shared.commit(second)==0);calls.clear();
  auto draw=reinterpret_cast<void(*)()>(f.backend.hooks.at(ResolverFake::target(45)).entry);draw();
  CHECK((calls==std::vector<int>{12,11,900,-11,-12}));
  CHECK(f.shared.release(first)==0);calls.clear();draw();CHECK((calls==std::vector<int>{12,900,-12}));
  CHECK(f.shared.release(second)==0&&f.backend.hooks.empty()&&f.modules.refs.empty());
 }
 std::cout<<"shared native hooks: "<<checks<<" checks passed\n";
}

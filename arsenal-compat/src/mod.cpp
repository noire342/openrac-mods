// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <openrac/openrac_mod.h>
#include "windows_backend.hpp"
#include "shared_provider.hpp"
#include "legacy_facade.hpp"
#include "pause_registry.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
namespace {
struct WindowsModules final:arsenal_compat::ModuleLifetime{
 void* retain(void* replacement)override{
  MEMORY_BASIC_INFORMATION info{};
  if(!replacement||VirtualQuery(replacement,&info,sizeof info)!=sizeof info||info.Type!=MEM_IMAGE||
     info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return nullptr;
  const auto protection=info.Protect&0xffu;
  if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return nullptr;
  HMODULE image{};return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(replacement),&image)?image:nullptr;
 }
 void release(void* image)noexcept override{if(image)FreeLibrary(static_cast<HMODULE>(image));}
};
struct Session{
 const openrac_api& api;openrac_mod* mod;
 arsenal_compat::WindowsResolver resolver;
 arsenal_compat::MinHookBackend backend;
 WindowsModules modules;
 arsenal_compat::SharedProvider provider;
 arsenal_compat::LegacyFacade legacy;
 arsenal_compat::PauseRegistry pause;
 Session(const openrac_api& a,openrac_mod* m):api(a),mod(m),resolver(a.function,a.version),provider(resolver,backend,modules),legacy(provider),pause(modules){}
 void log(int level)noexcept{arsenal_hooks_diagnostics_v1 d{};d.size=sizeof d;if(api.log&&!legacy.diagnostics(&d))api.log(mod,level,d.message[0]?d.message:"Verified native shared hook provider ready");}
};
std::unique_ptr<Session> session;
template<class F>int32_t invoke(F&& f)noexcept{if(!session)return ARSENAL_HOOKS_NOT_READY;try{return f(*session);}catch(...){return ARSENAL_HOOKS_BACKEND;}}
int32_t claim(const char* name){return invoke([&](auto& s){return s.legacy.claim(name);});}
int32_t bind(const char* id,const char* signature,void* replacement,void** original){return invoke([&](auto& s){return s.legacy.bind(id,signature,replacement,original);});}
int32_t commit(){return invoke([](auto& s){return s.legacy.commit();});}
int32_t rollback(){return invoke([](auto& s){return s.legacy.rollback();});}
int32_t diagnostics(arsenal_hooks_diagnostics_v1* out){return invoke([&](auto& s){return s.legacy.diagnostics(out);});}
int32_t owns_id(const char* id){return arsenal_compat::Provider::owns_id(id)?1:0;}
int32_t acquire_v2(const char* name,arsenal_hooks_consumer_v2* out){return invoke([&](auto& s){return s.provider.acquire(name,out);});}
int32_t bind_v2(arsenal_hooks_consumer_v2 token,const char* id,const char* signature,void* replacement,void** original){return invoke([&](auto& s){return s.provider.bind(token,id,signature,replacement,original);});}
int32_t commit_v2(arsenal_hooks_consumer_v2 token){return invoke([&](auto& s){return s.provider.commit(token);});}
int32_t release_v2(arsenal_hooks_consumer_v2 token){return invoke([&](auto& s){return s.provider.release(token);});}
int32_t owns_id_v2(const char* id){return arsenal_compat::SharedProvider::owns_id(id)?1:0;}
int32_t diagnostics_v2(arsenal_hooks_consumer_v2 token,arsenal_hooks_diagnostics_v1* out){return invoke([&](auto& s){return s.provider.diagnostics(token,out);});}
const arsenal_hooks_api_v1 service{sizeof(service),1,claim,bind,commit,rollback,diagnostics,owns_id};
const arsenal_hooks_api_v2 service_v2{sizeof(service_v2),2,acquire_v2,bind_v2,commit_v2,release_v2,owns_id_v2,diagnostics_v2};
template<class F>int32_t invoke_pause(F&& f)noexcept{if(!session)return ARSENAL_PAUSE_NOT_READY;try{return f(session->pause);}catch(...){return ARSENAL_PAUSE_BACKEND;}}
int32_t pause_acquire(const char* name,arsenal_pause_owner_v1* out){return invoke_pause([&](auto& p){return p.acquire(name,out);});}
int32_t pause_release(arsenal_pause_owner_v1 owner){return invoke_pause([&](auto& p){return p.release(owner);});}
int32_t pause_register(arsenal_pause_owner_v1 owner,const arsenal_pause_entry_v1* entry,arsenal_pause_open_v1 callback,uint64_t context,arsenal_pause_entry_token_v1* out){return invoke_pause([&](auto& p){return p.register_entry(owner,entry,callback,context,out);});}
int32_t pause_unregister(arsenal_pause_owner_v1 owner,arsenal_pause_entry_token_v1 entry){return invoke_pause([&](auto& p){return p.unregister_entry(owner,entry);});}
int32_t pause_update(arsenal_pause_owner_v1 owner,arsenal_pause_entry_token_v1 entry,const arsenal_pause_entry_v1* value){return invoke_pause([&](auto& p){return p.update_entry(owner,entry,value);});}
int32_t pause_count(uint32_t* count,uint64_t* revision){return invoke_pause([&](auto& p){return p.entry_count(count,revision);});}
int32_t pause_entry(uint32_t index,arsenal_pause_entry_copy_v1* out){return invoke_pause([&](auto& p){return p.entry_at(index,out);});}
int32_t pause_claim_presenter(arsenal_pause_owner_v1 owner,const arsenal_pause_capacity_v1* capacity,arsenal_pause_presenter_v1* out){return invoke_pause([&](auto& p){return p.claim_presenter(owner,capacity,out);});}
int32_t pause_release_presenter(arsenal_pause_owner_v1 owner,arsenal_pause_presenter_v1 presenter){return invoke_pause([&](auto& p){return p.release_presenter(owner,presenter);});}
int32_t pause_compose(arsenal_pause_presenter_v1 presenter,const arsenal_pause_stock_v1* stock,uint32_t count){return invoke_pause([&](auto& p){return p.compose(presenter,stock,count);});}
int32_t pause_focus(arsenal_pause_presenter_v1 presenter,uint64_t id){return invoke_pause([&](auto& p){return p.set_focus(presenter,id);});}
int32_t pause_snapshot(arsenal_pause_presenter_v1 presenter,arsenal_pause_state_v1* out,uint64_t* ids,uint32_t capacity,uint32_t* count){return invoke_pause([&](auto& p){return p.snapshot(presenter,out,ids,capacity,count);});}
int32_t pause_activate(arsenal_pause_presenter_v1 presenter,uint64_t id){return invoke_pause([&](auto& p){return p.activate(presenter,id);});}
const arsenal_pause_api_v1 pause_service{sizeof(pause_service),1,ARSENAL_PAUSE_CAP_REGISTRY|ARSENAL_PAUSE_CAP_PRESENTER_LEASE,uint32_t(arsenal_compat::PauseRegistry::MaxOwners),uint32_t(arsenal_compat::PauseRegistry::MaxEntries),pause_acquire,pause_release,pause_register,pause_unregister,pause_update,pause_count,pause_entry,pause_claim_presenter,pause_release_presenter,pause_compose,pause_focus,pause_snapshot,pause_activate};
void pin_provider()noexcept{HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&service),&pinned);}
}
extern "C" {OPENRAC_MOD_DEFINE("arsenal-compat","0.1.0")}
extern "C" OPENRAC_MOD_EXPORT const arsenal_hooks_api_v1* openrac_arsenal_hooks_query(uint32_t version){
 if(!session||!session->provider.ready())return nullptr;
 if(version==1)return &service;
 if(version==2){pin_provider();return reinterpret_cast<const arsenal_hooks_api_v1*>(&service_v2);}
 return nullptr;
}
extern "C" OPENRAC_MOD_EXPORT const arsenal_pause_api_v1* openrac_arsenal_pause_query(uint32_t version){
 if(version!=OPENRAC_ARSENAL_PAUSE_ABI_V1||!session||!session->provider.ready())return nullptr;
 pin_provider();return &pause_service;
}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api,openrac_mod* mod){
 try{
  if(!api||api->size<offsetof(openrac_api,function)+sizeof(api->function)||!api->function||!api->game||!api->version||!api->platform||std::strcmp(api->game,"rac1")||(std::strcmp(api->version,"ntsc")&&std::strcmp(api->version,"pal"))||std::strcmp(api->platform,"windows-x86_64")){
   if(api&&api->fail)api->fail(mod,"arsenal-compat requires stock OpenRAC R&C1 PAL or NTSC-U on Windows x64");return 1;
  }
  if(session){if(api->fail)api->fail(mod,"arsenal-compat is already loaded");return 1;}
  auto ready=std::make_unique<Session>(*api,mod);
  if(ready->provider.preflight()){if(api->fail)api->fail(mod,"Shared compatibility preflight rejected this executable/disc or native target table");return 1;}
  session=std::move(ready);session->log(OPENRAC_LOG_INFO);return 0;
 }catch(...){if(api&&api->fail)api->fail(mod,"arsenal-compat could not initialize its shared typed hook provider");return 1;}
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){
 if(!session)return;
 // A private loader may leave while a different mod still consumes this DLL.
 // Do not detach its hooks or destroy its session. V2 pins the broker itself;
 // replacement modules remain separately retained until their token releases.
 if(session->provider.claimed()||session->pause.claimed()){pin_provider();return;}
 session.reset();
}

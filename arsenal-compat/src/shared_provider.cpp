// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "shared_provider.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
namespace arsenal_compat {
namespace {
template<std::size_t N>void copy(char(&out)[N],const char* text){if(!text)text="";std::size_t n=0;for(;n+1<N&&text[n];++n)out[n]=text[n];out[n]=0;}
bool bounded(const char* text,std::size_t max){if(!text||!*text)return false;for(std::size_t i=0;i<=max;++i)if(!text[i])return true;return false;}
struct Busy{bool& busy;Busy(bool& v):busy(v){busy=true;}~Busy(){busy=false;}};
}
SharedProvider* SharedProvider::current_{};
SharedProvider::SharedProvider(Resolver& r,Backend& b,ModuleLifetime& l):resolver_(r),backend_(b),lifetime_(l),thread_(std::this_thread::get_id()){current_=this;}
SharedProvider::~SharedProvider(){for(std::size_t i=0;i<consumers_.size();++i)if(consumers_[i].token)release(consumers_[i].token);if(current_==this)current_=nullptr;}
int SharedProvider::check()const{if(std::this_thread::get_id()!=thread_)return ARSENAL_HOOKS_THREAD;if(busy_||dispatch_depth_)return ARSENAL_HOOKS_BUSY;return 0;}
int SharedProvider::owner(Token token)const{if(!token)return -1;for(std::size_t i=0;i<consumers_.size();++i)if(consumers_[i].token==token)return int(i);return -1;}
int SharedProvider::fail(int slot,int status,const char* text,const char* hook){if(slot>=0){auto& c=consumers_[std::size_t(slot)];c.status=status;copy(c.message,text);copy(c.hook,hook);}return status;}
int SharedProvider::preflight(){
 if(auto r=check())return r;
 Busy lock(busy_);if(ready_)return ARSENAL_HOOKS_NOT_READY;
 const auto* version=resolver_.version();if(!version||(std::strcmp(version,"pal")&&std::strcmp(version,"ntsc")))return ARSENAL_HOOKS_BUILD;
 region_=!std::strcmp(version,"pal")?Region::Pal:Region::Ntsc;
 std::string digest;if(!resolver_.executable_digest(digest))return ARSENAL_HOOKS_BUILD;
 const bool supported=region_==Region::Pal?digest==SupportedPalExecutable:std::any_of(SupportedExecutables.begin(),SupportedExecutables.end(),[&](auto x){return digest==x;});
 if(!supported)return ARSENAL_HOOKS_BUILD;
 copy(digest_,digest.c_str());
 const auto& specs=shared_hook_specs(region_);
 for(std::size_t i=0;i<specs.size();++i){
  void* target=resolver_.resolve(specs[i].symbol);
  if(!target){if(i<OPENRAC_ARSENAL_REQUIRED_HOOKS)return ARSENAL_HOOKS_UNRESOLVED;continue;}
  if(!resolver_.main_executable_target(target))return ARSENAL_HOOKS_UNSAFE_TARGET;
  for(std::size_t j=0;j<i;++j)if(targets_[j].address==target)return ARSENAL_HOOKS_UNSAFE_TARGET;
  targets_[i].address=target;
 }
 if(backend_.initialize())return ARSENAL_HOOKS_BACKEND;
 ready_=true;return 0;
}
int SharedProvider::acquire(const char* name,Token* out){
 if(out)*out=0;
 if(auto r=check())return r;
 Busy lock(busy_);
 if(!out||!bounded(name,63))return ARSENAL_HOOKS_ARGUMENT;
 if(!ready_)return ARSENAL_HOOKS_NOT_READY;
 for(const auto& c:consumers_)if(c.token&&!std::strcmp(c.name,name))return ARSENAL_HOOKS_CONSUMER;
 auto it=std::find_if(consumers_.begin(),consumers_.end(),[](const auto& c){return !c.token;});
 if(it==consumers_.end()||generation_==std::numeric_limits<Token>::max())return ARSENAL_HOOKS_CONSUMER;
 *it={};it->token=++generation_;copy(it->name,name);copy(it->message,"Consumer acquired; subset bindings remain inactive");*out=it->token;return 0;
}
int SharedProvider::bind(Token token,const char* id,const char* signature,void* replacement,void** original){
 if(original)*original=nullptr;
 if(auto r=check())return r;
 Busy lock(busy_);const int slot=owner(token);
 if(slot<0)return ARSENAL_HOOKS_NOT_CLAIMED;
 if(!bounded(id,95)||!bounded(signature,95)||!replacement||!original)return fail(slot,ARSENAL_HOOKS_ARGUMENT,"Invalid binding arguments");
 if(consumers_[slot].committed)return fail(slot,ARSENAL_HOOKS_COMMITTED,"Release a committed token before rebinding",id);
 const auto& specs=shared_hook_specs(region_);const auto spec=std::find_if(specs.begin(),specs.end(),[&](const auto& s){return !std::strcmp(s.id,id);});
 if(spec==specs.end())return ARSENAL_HOOKS_UNKNOWN;
 if(std::strcmp(spec->signature,signature))return fail(slot,ARSENAL_HOOKS_SIGNATURE,"Exact typed signature does not match",id);
 const auto index=std::size_t(spec-specs.begin());auto& target=targets_[index];auto& node=target.nodes[slot];
 if(!target.address)return fail(slot,ARSENAL_HOOKS_UNRESOLVED,"Optional native target is unavailable",id);
 if(node.replacement)return fail(slot,ARSENAL_HOOKS_DUPLICATE,"Consumer already bound this target",id);
 if(replacement==target.address||replacement==entrypoint(index))return fail(slot,ARSENAL_HOOKS_ARGUMENT,"Replacement cannot be target or broker",id);
 void* module=lifetime_.retain(replacement);if(!module)return fail(slot,ARSENAL_HOOKS_UNSAFE_TARGET,"Cannot retain replacement module",id);
 if(!target.created){
  void* native{};const auto status=backend_.create(target.address,entrypoint(index),&native);
  if(status||!native){lifetime_.release(module);return fail(slot,ARSENAL_HOOKS_BACKEND,"Cannot create typed broker detour",id);}
  target.native=native;target.created=true;
 }
 node={replacement,module,false,0};*original=continuation(index,std::size_t(slot));return fail(slot,0,"Typed continuation prepared",id);
}
int SharedProvider::commit(Token token){
 if(auto r=check())return r;
 Busy lock(busy_);const int slot=owner(token);if(slot<0)return ARSENAL_HOOKS_NOT_CLAIMED;
 if(consumers_[slot].committed)return fail(slot,ARSENAL_HOOKS_COMMITTED,"Consumer already committed");
 if(order_==std::numeric_limits<std::uint64_t>::max())return fail(slot,ARSENAL_HOOKS_NOT_READY,"Dispatch sequence exhausted");
 bool any=false;std::array<bool,SharedTargets> enabling{};
 for(std::size_t i=0;i<targets_.size();++i){auto& t=targets_[i];if(!t.nodes[slot].replacement)continue;any=true;if(!t.enabled){enabling[i]=true;if(backend_.queue_enable(t.address)){
   for(std::size_t j=0;j<=i;++j)if(enabling[j]){backend_.queue_disable(targets_[j].address);backend_.disable(targets_[j].address);}
   backend_.apply();
   return fail(slot,ARSENAL_HOOKS_BACKEND,"Subset activation queue failed; existing consumers retained",shared_hook_specs(region_)[i].id);
  }}}
 if(!any)return fail(slot,ARSENAL_HOOKS_INCOMPLETE,"Commit requires at least one binding");
 if(backend_.apply()){
  for(std::size_t i=0;i<targets_.size();++i)if(enabling[i]){backend_.queue_disable(targets_[i].address);backend_.disable(targets_[i].address);}
  backend_.apply();
  return fail(slot,ARSENAL_HOOKS_BACKEND,"Subset activation failed; existing consumers retained");
 }
 const auto order=++order_;
 for(auto& t:targets_)if(t.nodes[slot].replacement){t.enabled=true;t.nodes[slot].active=true;t.nodes[slot].order=order;}
 consumers_[slot].committed=true;return fail(slot,0,"Consumer subset active; other consumers retained");
}
int SharedProvider::cleanup(std::size_t slot){
 for(auto& target:targets_){auto& node=target.nodes[slot];if(!node.replacement)continue;
  const bool other=std::any_of(target.nodes.begin(),target.nodes.end(),[&](const Node& n){return &n!=&node&&n.replacement;});
  if(!other&&target.created){
   if(backend_.disable(target.address))return ARSENAL_HOOKS_ROLLBACK;
   target.enabled=false;
   if(backend_.remove(target.address))return ARSENAL_HOOKS_ROLLBACK;
   target.created=false;target.native=nullptr;
  }
  node.active=false;if(node.module)lifetime_.release(node.module);node={};
 }
 return 0;
}
int SharedProvider::release(Token token){
 if(auto r=check())return r;
 Busy lock(busy_);const int slot=owner(token);if(slot<0)return ARSENAL_HOOKS_NOT_CLAIMED;
 const auto result=cleanup(std::size_t(slot));if(result)return fail(slot,result,"Broker teardown failed; retain consumer token/module and retry");
 consumers_[slot]={};return 0;
}
bool SharedProvider::bound(Token token,std::size_t index)const{const int slot=owner(token);return slot>=0&&index<targets_.size()&&targets_[index].nodes[slot].replacement;}
bool SharedProvider::active(Token token)const{const int slot=owner(token);return slot>=0&&consumers_[slot].committed;}
bool SharedProvider::claimed()const{return std::any_of(consumers_.begin(),consumers_.end(),[](const auto& c){return c.token!=0;});}
bool SharedProvider::owns_id(const char* id)noexcept{if(!bounded(id,95))return false;const auto& specs=shared_hook_specs(Region::Ntsc);return std::any_of(specs.begin(),specs.end(),[&](const auto& s){return !std::strcmp(s.id,id);});}
int SharedProvider::diagnostics(Token token,arsenal_hooks_diagnostics_v1* out)const{
 if(auto r=check())return r;
 if(!out||out->size<sizeof(*out))return ARSENAL_HOOKS_ARGUMENT;
 const int slot=owner(token);if(token&&slot<0)return ARSENAL_HOOKS_NOT_CLAIMED;
 *out={};out->size=sizeof(*out);out->version=2;out->resolved=OPENRAC_ARSENAL_REQUIRED_HOOKS;copy(out->executable_sha256,digest_);
 out->stage=ready_?ARSENAL_HOOKS_READY:ARSENAL_HOOKS_COLD;
 if(slot>=0){const auto& c=consumers_[slot];copy(out->consumer,c.name);copy(out->message,c.message);copy(out->hook_id,c.hook);out->last_status=c.status;
  out->stage=c.committed?ARSENAL_HOOKS_ACTIVE:ARSENAL_HOOKS_COLLECTING;
  for(const auto& t:targets_){out->bound+=t.nodes[slot].replacement?1u:0u;out->active+=t.nodes[slot].active?1u:0u;}}
 return 0;
}
}

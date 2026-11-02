// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "provider.hpp"
#include "arsenal_hooks_api_v2.h"
#include <array>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>
namespace arsenal_compat {
inline constexpr std::size_t SharedConsumers=16, SharedTargets=HookCatalogSize+15;
const std::array<HookSpec,SharedTargets>& shared_hook_specs(Region region) noexcept;
class ModuleLifetime {
public:
 virtual ~ModuleLifetime()=default;
 virtual void* retain(void* replacement)=0;
 virtual void release(void* module)noexcept=0;
};
class SharedProvider final {
public:
 using Token=arsenal_hooks_consumer_v2;
 SharedProvider(Resolver&,Backend&,ModuleLifetime&);
 ~SharedProvider();
 SharedProvider(const SharedProvider&)=delete;
 SharedProvider& operator=(const SharedProvider&)=delete;
 int preflight();
 int acquire(const char*,Token*);
 int bind(Token,const char*,const char*,void*,void**);
 int commit(Token);
 int release(Token);
 int diagnostics(Token,arsenal_hooks_diagnostics_v1*)const;
 bool bound(Token,std::size_t)const;
 bool active(Token)const;
 bool ready()const{return ready_;}
 bool claimed()const;
 int access_status()const{return check();}
 static bool owns_id(const char*)noexcept;
 // Signature-specialized generated thunks only. No untyped argument packing.
 static void* entrypoint(std::size_t);
 static void* continuation(std::size_t,std::size_t);
 static SharedProvider* current()noexcept{return current_;}
 template<std::size_t Target,class R,class... A>R dispatch(std::size_t after,A... args){
  if(Target>=targets_.size())return empty<R>();
  auto& target=targets_[Target];
  std::uint64_t before=~std::uint64_t{};
  if(after<SharedConsumers){
   const auto& own=target.nodes[after];
   before=own.active?own.order:0;
  }
  Node* selected=nullptr;
  for(auto& node:target.nodes)if(node.active&&node.order<before&&(!selected||node.order>selected->order))selected=&node;
  void* fn=selected?selected->replacement:target.native;
  if(!fn)return empty<R>();
  struct Guard{unsigned& value;Guard(unsigned& v):value(v){++value;}~Guard(){--value;}} guard(dispatch_depth_);
  return reinterpret_cast<R(*)(A...)>(fn)(args...);
 }
private:
 struct Node{void* replacement{};void* module{};bool active{};std::uint64_t order{};};
 struct Target{void* address{};void* native{};bool created{},enabled{};std::array<Node,SharedConsumers> nodes{};};
 struct Consumer{Token token{};bool committed{};char name[64]{};int status{};char message[256]{},hook[96]{};};
 Resolver& resolver_;Backend& backend_;ModuleLifetime& lifetime_;
 Region region_=Region::Ntsc;
 std::array<Target,SharedTargets> targets_{};
 std::array<Consumer,SharedConsumers> consumers_{};
 std::thread::id thread_;
 std::uint64_t generation_{},order_{};
 unsigned dispatch_depth_{};
 bool busy_{},ready_{};
 char digest_[65]{};
 static SharedProvider* current_;
 template<class R>static R empty(){if constexpr(!std::is_void_v<R>)return R{};}
 int check()const;
 int owner(Token)const;
 int fail(int,int,const char*,const char* = "");
 int cleanup(std::size_t);
};
}

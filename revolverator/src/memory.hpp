// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <openrac/openrac_mod.h>
#include "native_arena.hpp"
#include "region_bindings.hpp"
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace revolverator {
class Memory {
public:
 explicit Memory(const openrac_api* api,GuestArena* arena=nullptr):api_(api),guest_arena_(arena),bindings_(profile(api)){}
 static bool valid(std::uint32_t at,std::size_t n){return n<=0x2000000&&at>=0x10000&&at<=0x2000000-n;}
 template<class T>T read(std::uint32_t at)const{T v{};if(valid(at,sizeof v))std::memcpy(&v,api_->memory+at,sizeof v);return v;}
 template<class T>void write(std::uint32_t at,const T& v)const{if(valid(at,sizeof v))std::memcpy(api_->memory+at,&v,sizeof v);}
 const RegionBindings& bindings()const noexcept{return *bindings_;}
 std::uint32_t global(std::uint32_t at)const{return api_->relocate_data(bindings().data(at));}
 std::uint32_t level(std::uint32_t at)const{return level(0,at);}
 std::uint32_t level(unsigned overlay,std::uint32_t at)const{return api_->relocate_level_data(int(overlay),bindings().level(overlay,at));}
 std::uint32_t guest_callback(unsigned overlay,std::uint32_t at)const{return bindings().callback(overlay,at);}
 void* find_function(const char* name)const noexcept{
  if(!api_||!api_->function)return nullptr;
  const auto mapped=bindings().function(name);
  return mapped?api_->function(mapped):nullptr;
 }
 template<class T>T function(const char* name)const{
  const auto mapped=bindings().function(name);
  if(!mapped)throw std::runtime_error(std::string("Revolverator has no ")+bindings().version()+" binding for game function "+(name?name:"<null>"));
  auto f=reinterpret_cast<T>(api_&&api_->function?api_->function(mapped):nullptr);
  if(!f)throw std::runtime_error(std::string("Revolverator needs ")+bindings().version()+" game function "+mapped+" (canonical "+(name?name:"<null>")+")");
  return f;
 }
protected:
 const openrac_api* api_;
 unsigned allocate_guest(unsigned bytes)const{
  if(guest_arena_){
   const auto at=guest_arena_->allocate(bytes);
   if(at||!hud_fallback_)return at;
  }
  // Test/standalone adapters may use the original allocator. Runtime sessions
  // always inject their explicitly reserved level arena.
  return function<unsigned(*)(unsigned)>("FUN_001ff288")(bytes);
 }
 GuestArena* guest_arena_;
 bool hud_fallback_=false;
private:
 const RegionBindings* bindings_;
 static const RegionBindings* profile(const openrac_api* api){
  // Existing host fakes omit version. Runtime preflight requires a real disc.
  if(!api||!api->version)return &RegionBindings::ntsc();
  const auto* result=RegionBindings::for_version(api->version);
  if(!result)throw std::runtime_error(std::string("Revolverator has no region bindings for game version ")+api->version);
  return result;
 }
};
}

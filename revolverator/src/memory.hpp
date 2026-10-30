// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openrac/openrac_mod.h>
#include "native_arena.hpp"
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace revolverator {
class Memory {
public:
 explicit Memory(const openrac_api* api,GuestArena* arena=nullptr):api_(api),guest_arena_(arena){}
 static bool valid(std::uint32_t at,std::size_t n){return n<=0x2000000&&at>=0x10000&&at<=0x2000000-n;}
 template<class T>T read(std::uint32_t at)const{T v{};if(valid(at,sizeof v))std::memcpy(&v,api_->memory+at,sizeof v);return v;}
 template<class T>void write(std::uint32_t at,const T& v)const{if(valid(at,sizeof v))std::memcpy(api_->memory+at,&v,sizeof v);}
 std::uint32_t global(std::uint32_t at)const{return api_->relocate_data(at);}
 std::uint32_t level(std::uint32_t at)const{return api_->relocate_level_data(0,at);}
 template<class T>T function(const char* name)const{auto f=reinterpret_cast<T>(api_->function(name));if(!f)throw std::runtime_error(std::string("Revolverator needs game function ")+name);return f;}
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
};
}

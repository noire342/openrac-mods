// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <openrac/openrac_mod.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace salvage {
// Storage returned by the native level loader is reserved through its returned
// heap cursor. A second mod receives the adjusted cursor, never overlapping us.
class GuestArena final {
public:
 static constexpr std::uint32_t ceiling=0x1ff8000;
 std::uint32_t reserve(std::uint32_t origin,std::size_t bytes){
  reset();if(origin<0x10000||origin>ceiling||!bytes||bytes>ceiling)return origin;
  const auto begin=(origin+15u)&~15u;
  const auto size=(bytes+15u)&~std::size_t(15u);
  if(begin>ceiling||size>ceiling-begin)return origin;
  begin_=cursor_=begin;end_=begin+std::uint32_t(size);return end_;
 }
 std::uint32_t allocate(std::size_t bytes){
  if(!begin_||!bytes||bytes>available())return 0;
  const auto size=(bytes+15u)&~std::size_t(15u);if(size>available())return 0;
  const auto result=cursor_;cursor_+=std::uint32_t(size);return result;
 }
 bool owns(std::uint32_t at,std::size_t bytes)const{return begin_&&at>=begin_&&at<=end_&&bytes<=end_-at;}
 std::uint32_t begin()const{return begin_;}
 std::uint32_t end()const{return end_;}
 std::uint32_t used()const{return cursor_-begin_;}
 std::uint32_t available()const{return end_-cursor_;}
 void reset(){begin_=cursor_=end_=0;}
private:
 std::uint32_t begin_{},cursor_{},end_{};
};
// PAL-specific host boundary. Business rules never include this file.
class NativeMemory {
public:
 static constexpr unsigned Hero=0x13f450,Pad=0x13ca40,Delta=0x15ee7c,
  Language=0x15ee88,MenuSystem=0x1d5f70,Memcard=0x13d390,
  MobyBegin=0x160018,MobyEnd=0x160020;
 explicit NativeMemory(const openrac_api* api):api_(api){}
 static bool valid(std::uint32_t at,std::size_t bytes){return bytes<=0x2000000&&at>=0x10000&&at<=0x2000000-bytes;}
 template<class T>T read(std::uint32_t at)const{T value{};if(api_&&api_->memory&&valid(at,sizeof value))std::memcpy(&value,api_->memory+at,sizeof value);return value;}
 template<class T>void write(std::uint32_t at,const T& value)const{if(api_&&api_->memory&&valid(at,sizeof value))std::memcpy(api_->memory+at,&value,sizeof value);}
 unsigned g(unsigned pal)const{return api_->relocate_data(pal);}
 unsigned l(int overlay,unsigned pal)const{return api_->relocate_level_data(overlay,pal);}
 unsigned mode()const{return l(0,0x15f6a8);}
 // The disc loader reuses level-owned arenas as compressed input before the
 // gameplay-load callback runs. Do not reset or write those arenas in mode6.
 bool level_transitioning()const{
  if(!api_||!api_->memory||!api_->loaded_level||!api_->relocate_level_data)return true;
  const int current=api_->loaded_level();
  return current<0||current>18||read<int>(mode())==6;
 }
 template<class T>T fn(const char* name)const{
  auto result=reinterpret_cast<T>(api_->function(name));
  if(!result)throw std::runtime_error(std::string("Robot Salvage: native PAL function unavailable: ")+name);
  return result;
 }
protected:
 const openrac_api* api_;
};
}

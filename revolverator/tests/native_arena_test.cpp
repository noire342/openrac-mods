// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arena.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
using namespace revolverator;
namespace {int checks=0;void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"arena: %s\n",message);std::exit(1);}}}
int main(){
 GuestArena arena;
 require(!arena.allocate(16)&&arena.available()==0,"unreserved storage must fail closed");
 require(arena.reserve(0x1f81ac0,128*1024)==0x1fa1ac0,"native cursor must advance past the complete owned range");
 const auto arm=arena.allocate(512),rotor=arena.allocate(320);
 require(arm==0x1f81ac0&&rotor==arm+512,"arm and rotor must get disjoint persistent blocks");
 require(arena.owns(arm,512)&&arena.owns(rotor,320)&&arena.used()==832,"arena must track its entire live allocation range");
 require(!arena.owns(arm-1,1)&&!arena.owns(arena.end(),1),"ownership checks must reject either boundary overflow");
 const auto before=arena.available();require(!arena.allocate(std::size_t(before)+1)&&arena.available()==before,"failed allocation must not consume storage");
 require(!arena.allocate(0)&&!arena.allocate(std::numeric_limits<std::size_t>::max()),"zero and overflowing requests must fail");
 arena.reset();require(arena.begin()==0&&arena.end()==0&&!arena.allocate(16),"reset must invalidate all reservations");
 require(arena.reserve(0x10001,17)==0x10030,"reservation must align both start and extent");
 require(arena.allocate(1)==0x10010&&arena.allocate(1)==0x10020&&!arena.allocate(1),"allocation must align and respect exact remaining capacity");
 require(arena.reserve(GuestArena::ceiling-32,32)==GuestArena::ceiling&&arena.allocate(32)==GuestArena::ceiling-32,"the native heap ceiling is an exclusive storage boundary");
 for(auto origin:{0u,0xffffu,0xffffffffu,GuestArena::ceiling+1})
  require(arena.reserve(origin,16)==origin&&arena.begin()==0,"invalid native cursors must remain unchanged");
 require(arena.reserve(GuestArena::ceiling-16,32)==GuestArena::ceiling-16&&arena.begin()==0,"insufficient native headroom must preserve the loader cursor");
 require(arena.reserve(0x10000,std::numeric_limits<std::size_t>::max())==0x10000&&arena.begin()==0,"overflowing reservation must fail without wraparound");
 std::printf("arena: %d checks passed (native cursor, alignment, bounds, exhaustion and reset)\n",checks);
}

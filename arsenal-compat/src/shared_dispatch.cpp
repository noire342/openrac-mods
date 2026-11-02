// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "shared_provider.hpp"
#include <algorithm>
namespace arsenal_compat {
namespace {
template<std::size_t Target,class>struct Dispatch;
template<std::size_t Target,class R,class... A>struct Dispatch<Target,R(A...)>{
 static R entry(A... args){auto* p=SharedProvider::current();if(p)return p->template dispatch<Target,R,A...>(SharedConsumers,args...);if constexpr(!std::is_void_v<R>)return R{};}
 template<std::size_t Slot>static R next(A... args){auto* p=SharedProvider::current();if(p)return p->template dispatch<Target,R,A...>(Slot,args...);if constexpr(!std::is_void_v<R>)return R{};}
 template<std::size_t... I>static auto continuations(std::index_sequence<I...>){return std::array<void*,sizeof...(I)>{{reinterpret_cast<void*>(&next<I>)...}};}
 static void* continuation(std::size_t slot){static const auto list=continuations(std::make_index_sequence<SharedConsumers>{});return slot<list.size()?list[slot]:nullptr;}
};
using Target0=Dispatch<0,std::int32_t(std::uint32_t)>;
using Target1=Dispatch<1,std::int32_t(std::uint32_t)>;
using Target2=Dispatch<2,std::int32_t(std::uint32_t)>;
using Target3=Dispatch<3,std::int32_t(std::uint32_t)>;
using Target4=Dispatch<4,std::int32_t(std::uint32_t)>;
using Target5=Dispatch<5,std::int32_t(std::uint32_t)>;
using Target6=Dispatch<6,std::int32_t(std::uint32_t)>;
using Target7=Dispatch<7,std::int32_t(std::int32_t)>;
using Target8=Dispatch<8,std::int32_t(std::uint32_t)>;
using Target9=Dispatch<9,std::int32_t(std::uint32_t)>;
using Target10=Dispatch<10,std::int32_t(std::uint32_t)>;
using Target11=Dispatch<11,std::int32_t(std::uint32_t)>;
using Target12=Dispatch<12,std::int32_t(std::uint32_t)>;
using Target13=Dispatch<13,std::int32_t(std::uint32_t)>;
using Target14=Dispatch<14,void(std::uint32_t)>;
using Target15=Dispatch<15,void(std::uint32_t)>;
using Target16=Dispatch<16,void(std::uint32_t)>;
using Target17=Dispatch<17,std::int32_t(std::uint32_t)>;
using Target18=Dispatch<18,void()>;
using Target19=Dispatch<19,void()>;
using Target20=Dispatch<20,std::int32_t(std::uint32_t,std::uint32_t)>;
using Target21=Dispatch<21,std::uint32_t(std::uint32_t,std::int32_t,std::int32_t,std::int32_t)>;
using Target22=Dispatch<22,void(std::int32_t,std::int32_t)>;
using Target23=Dispatch<23,std::int32_t(std::int32_t,std::int32_t)>;
using Target24=Dispatch<24,void(std::uint32_t)>;
using Target25=Dispatch<25,std::int32_t(std::uint32_t,std::int32_t,std::uint32_t)>;
using Target26=Dispatch<26,void()>;
using Target27=Dispatch<27,void(std::int32_t)>;
using Target28=Dispatch<28,void(std::uint32_t)>;
using Target29=Dispatch<29,void()>;
using Target30=Dispatch<30,void()>;
using Target31=Dispatch<31,std::uint32_t(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::int32_t,std::int32_t,std::uint32_t,std::uint32_t,float,std::uint32_t)>;
using Target32=Dispatch<32,void()>;
using Target33=Dispatch<33,std::uint32_t(std::uint32_t,float,std::uint32_t)>;
using Target34=Dispatch<34,std::uint32_t(std::uint32_t,std::uint32_t)>;
using Target35=Dispatch<35,std::uint32_t(std::uint32_t,std::uint32_t,std::int32_t)>;
using Target36=Dispatch<36,std::uint32_t(std::uint32_t,std::uint32_t,float)>;
using Target37=Dispatch<37,std::uint32_t(std::uint32_t)>;
using Target38=Dispatch<38,std::int32_t(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)>;
using Target39=Dispatch<39,std::int32_t(std::uint32_t,std::uint32_t)>;
using Target40=Dispatch<40,std::int32_t(std::uint32_t,std::uint32_t,std::uint32_t,float)>;
using Target41=Dispatch<41,std::int32_t(std::uint32_t)>;
using Target42=Dispatch<42,void(std::uint32_t,std::uint32_t,std::int32_t)>;
using Target43=Dispatch<43,void(std::int32_t)>;
using Target44=Dispatch<44,std::int32_t(std::uint32_t,std::uint32_t,std::uint32_t)>;
using Target45=Dispatch<45,void()>;
}
void* SharedProvider::entrypoint(std::size_t target){switch(target){
case 0:return reinterpret_cast<void*>(&Target0::entry);
case 1:return reinterpret_cast<void*>(&Target1::entry);
case 2:return reinterpret_cast<void*>(&Target2::entry);
case 3:return reinterpret_cast<void*>(&Target3::entry);
case 4:return reinterpret_cast<void*>(&Target4::entry);
case 5:return reinterpret_cast<void*>(&Target5::entry);
case 6:return reinterpret_cast<void*>(&Target6::entry);
case 7:return reinterpret_cast<void*>(&Target7::entry);
case 8:return reinterpret_cast<void*>(&Target8::entry);
case 9:return reinterpret_cast<void*>(&Target9::entry);
case 10:return reinterpret_cast<void*>(&Target10::entry);
case 11:return reinterpret_cast<void*>(&Target11::entry);
case 12:return reinterpret_cast<void*>(&Target12::entry);
case 13:return reinterpret_cast<void*>(&Target13::entry);
case 14:return reinterpret_cast<void*>(&Target14::entry);
case 15:return reinterpret_cast<void*>(&Target15::entry);
case 16:return reinterpret_cast<void*>(&Target16::entry);
case 17:return reinterpret_cast<void*>(&Target17::entry);
case 18:return reinterpret_cast<void*>(&Target18::entry);
case 19:return reinterpret_cast<void*>(&Target19::entry);
case 20:return reinterpret_cast<void*>(&Target20::entry);
case 21:return reinterpret_cast<void*>(&Target21::entry);
case 22:return reinterpret_cast<void*>(&Target22::entry);
case 23:return reinterpret_cast<void*>(&Target23::entry);
case 24:return reinterpret_cast<void*>(&Target24::entry);
case 25:return reinterpret_cast<void*>(&Target25::entry);
case 26:return reinterpret_cast<void*>(&Target26::entry);
case 27:return reinterpret_cast<void*>(&Target27::entry);
case 28:return reinterpret_cast<void*>(&Target28::entry);
case 29:return reinterpret_cast<void*>(&Target29::entry);
case 30:return reinterpret_cast<void*>(&Target30::entry);
case 31:return reinterpret_cast<void*>(&Target31::entry);
case 32:return reinterpret_cast<void*>(&Target32::entry);
case 33:return reinterpret_cast<void*>(&Target33::entry);
case 34:return reinterpret_cast<void*>(&Target34::entry);
case 35:return reinterpret_cast<void*>(&Target35::entry);
case 36:return reinterpret_cast<void*>(&Target36::entry);
case 37:return reinterpret_cast<void*>(&Target37::entry);
case 38:return reinterpret_cast<void*>(&Target38::entry);
case 39:return reinterpret_cast<void*>(&Target39::entry);
case 40:return reinterpret_cast<void*>(&Target40::entry);
case 41:return reinterpret_cast<void*>(&Target41::entry);
case 42:return reinterpret_cast<void*>(&Target42::entry);
case 43:return reinterpret_cast<void*>(&Target43::entry);
case 44:return reinterpret_cast<void*>(&Target44::entry);
case 45:return reinterpret_cast<void*>(&Target45::entry);
default:return nullptr;}}
void* SharedProvider::continuation(std::size_t target,std::size_t slot){switch(target){
case 0:return Target0::continuation(slot);
case 1:return Target1::continuation(slot);
case 2:return Target2::continuation(slot);
case 3:return Target3::continuation(slot);
case 4:return Target4::continuation(slot);
case 5:return Target5::continuation(slot);
case 6:return Target6::continuation(slot);
case 7:return Target7::continuation(slot);
case 8:return Target8::continuation(slot);
case 9:return Target9::continuation(slot);
case 10:return Target10::continuation(slot);
case 11:return Target11::continuation(slot);
case 12:return Target12::continuation(slot);
case 13:return Target13::continuation(slot);
case 14:return Target14::continuation(slot);
case 15:return Target15::continuation(slot);
case 16:return Target16::continuation(slot);
case 17:return Target17::continuation(slot);
case 18:return Target18::continuation(slot);
case 19:return Target19::continuation(slot);
case 20:return Target20::continuation(slot);
case 21:return Target21::continuation(slot);
case 22:return Target22::continuation(slot);
case 23:return Target23::continuation(slot);
case 24:return Target24::continuation(slot);
case 25:return Target25::continuation(slot);
case 26:return Target26::continuation(slot);
case 27:return Target27::continuation(slot);
case 28:return Target28::continuation(slot);
case 29:return Target29::continuation(slot);
case 30:return Target30::continuation(slot);
case 31:return Target31::continuation(slot);
case 32:return Target32::continuation(slot);
case 33:return Target33::continuation(slot);
case 34:return Target34::continuation(slot);
case 35:return Target35::continuation(slot);
case 36:return Target36::continuation(slot);
case 37:return Target37::continuation(slot);
case 38:return Target38::continuation(slot);
case 39:return Target39::continuation(slot);
case 40:return Target40::continuation(slot);
case 41:return Target41::continuation(slot);
case 42:return Target42::continuation(slot);
case 43:return Target43::continuation(slot);
case 44:return Target44::continuation(slot);
case 45:return Target45::continuation(slot);
default:return nullptr;}}
const std::array<HookSpec,SharedTargets>& shared_hook_specs(Region region)noexcept{
 static const auto ntsc=[] {std::array<HookSpec,SharedTargets> out{};const auto& v1=hook_specs(Region::Ntsc);std::copy(v1.begin(),v1.end(),out.begin());
 out[HookCatalogSize]={"rac1.moby.spawn_break_fragment","u32(u32,i32,u32,u32,i32,i32,u32,u32,f32,u32)","FUN_L00_00263fd8"};
 out[HookCatalogSize+1]={"rac1.render.dispatch_callbacks","void()","FUN_001f4650"};
 out[33]={"rac1.moby.spawn_flying_debris","u32(u32,f32,u32)","FUN_L14_002bb310"};
 out[34]={"rac1.moby.spawn_plasmabot_death_debris","u32(u32,u32)","FUN_L14_002dea98"};
 out[35]={"rac1.moby.spawn_path_or_ballistic_debris","u32(u32,u32,i32)","FUN_L14_002e0170"};
 out[36]={"rac1.moby.spawn_bomber_jet_debris","u32(u32,u32,f32)","FUN_L01_0030be70"};
 out[37]={"rac1.suck_cannon.get_record","u32(u32)","FUN_L00_002db890"};
 out[38]={"rac1.suck_cannon.try_claim","i32(u32,u32,u32,u32,u32)","FUN_L00_002da058"};
 out[39]={"rac1.suck_cannon.store","i32(u32,u32)","FUN_L00_002dbb20"};
 out[40]={"rac1.suck_cannon.launch","i32(u32,u32,u32,f32)","FUN_L00_002dbc30"};
 out[41]={"rac1.suck_cannon.cancel","i32(u32)","FUN_L00_002dbe20"};
 out[42]={"rac1.suck_cannon.finalize","void(u32,u32,i32)","FUN_L00_002dc938"};
 out[43]={"rac1.menu.pause.render","void(i32)","FUN_L00_002772c0"};
 out[44]={"rac1.suck_cannon.grab","i32(u32,u32,u32)","FUN_L00_002db8f8"};
 out[45]={"rac1.hud.render","void()","FUN_001ff780"};return out;}();
 static const auto pal=[] {auto out=ntsc;const auto& v1=hook_specs(Region::Pal);std::copy(v1.begin(),v1.end(),out.begin());out[HookCatalogSize].symbol="func_L00_00265050";out[HookCatalogSize+1].symbol="func_001F4A00";
 out[33].symbol="func_L14_002BC560";out[34].symbol="func_L14_002DFE98";out[35].symbol="func_L14_002E1570";out[36].symbol="func_L01_0030D248";
 out[37].symbol="func_L00_002DCD40";out[38].symbol="func_L00_002DB508";out[39].symbol="func_L00_002DCFD0";out[40].symbol="func_L00_002DD0E0";out[41].symbol="func_L00_002DD2D0";out[42].symbol="func_L00_002DDDE8";out[43].symbol="func_L00_002781D8";out[44].symbol="func_L00_002DCDA8";out[45].symbol="func_001FFFB8";return out;}();
 return region==Region::Pal?pal:ntsc;
}
}

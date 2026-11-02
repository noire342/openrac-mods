// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace spacevoyages {
// Process-local QA only. There is no product setting or input binding for this
// cancellation. The Session enables it only with trace and the player launcher
// clears every inherited SPACE_VOYAGES_QA_* variable before starting a game.
class DiagnosticSceneControl final {
public:
 struct Cancel {std::uint32_t trip{},tick{};};
 using EnvironmentReader=const char*(*)(const char*);
 static constexpr std::uint32_t maximum_trip=64,maximum_tick=36000;
 static constexpr const char* trip_variable="SPACE_VOYAGES_QA_CANCEL_TRIP";
 static constexpr const char* tick_variable="SPACE_VOYAGES_QA_CANCEL_TICK";

 DiagnosticSceneControl()=default;
 explicit DiagnosticSceneControl(Cancel requested)noexcept:
  cancel_(valid(requested)?requested:Cancel{}){}

 static Cancel parse(const char* trip,const char* tick)noexcept{
  Cancel result;
  if(!number(trip,maximum_trip,result.trip)||!number(tick,maximum_tick,result.tick))return {};
  return result;
 }
 static DiagnosticSceneControl from_environment(bool trace,EnvironmentReader reader=nullptr)noexcept{
  if(!trace)return {};
  if(reader)return DiagnosticSceneControl(parse(reader(trip_variable),reader(tick_variable)));
#ifdef _MSC_VER
  // Bounded reads avoid both allocations and MSVC's deprecated getenv path.
  char trip[12]{},tick[12]{};std::size_t trip_size{},tick_size{};
  if(getenv_s(&trip_size,trip,sizeof trip,trip_variable)||getenv_s(&tick_size,tick,sizeof tick,tick_variable)||
     trip_size>sizeof trip||tick_size>sizeof tick)return {};
  return DiagnosticSceneControl(parse(trip,tick));
#else
  return DiagnosticSceneControl(parse(std::getenv(trip_variable),std::getenv(tick_variable)));
#endif
 }

 // Called only immediately before a real transient_run invocation, never on
 // boarding, origin inspection or synthetic phase notifications.
 void begin_scene()noexcept{
  if(scenes_!=std::numeric_limits<std::uint32_t>::max())++scenes_;
  ticks_=0;
 }
 // One call per native scene tick. Does not touch the voyage, host, input,
 // tokens, native memory or audio. The Session owns the normal close path.
 bool cancel_tick()noexcept{
  if(!enabled()||!scenes_)return false;
  if(ticks_!=std::numeric_limits<std::uint32_t>::max())++ticks_;
  if(!fired_&&scenes_==cancel_.trip&&ticks_==cancel_.tick){fired_=true;return true;}
  return false;
 }
 bool enabled()const noexcept{return cancel_.trip!=0;}
 bool fired()const noexcept{return fired_;}
 std::uint32_t scene_count()const noexcept{return scenes_;}
 std::uint32_t tick_count()const noexcept{return ticks_;}
 Cancel request()const noexcept{return cancel_;}
private:
 static bool valid(Cancel c)noexcept{return c.trip&&c.trip<=maximum_trip&&c.tick&&c.tick<=maximum_tick;}
 static bool number(const char* text,std::uint32_t maximum,std::uint32_t& out)noexcept{
  out=0;if(!text||!*text)return false;std::uint32_t value=0;
  std::size_t n=0;for(;n<10&&text[n];++n){
   if(text[n]<'0'||text[n]>'9')return false;
   const auto digit=static_cast<std::uint32_t>(text[n]-'0');
   if(value>maximum/10||(value==maximum/10&&digit>maximum%10))return false;
   value=value*10+digit;
  }
  if(!n||text[n]||!value)return false;out=value;return true;
 }
 Cancel cancel_{};
 std::uint32_t scenes_{},ticks_{};
 bool fired_{};
};
}

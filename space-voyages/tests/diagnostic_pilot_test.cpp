// SPDX-License-Identifier: GPL-3.0-or-later
#include "diagnostic_pilot.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
using namespace spacevoyages;
namespace {bool watch_allocations=false;std::size_t allocations{};}
void* operator new(std::size_t size){if(watch_allocations)++allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks{};std::uint64_t flight_ticks{};double maximum_apply_ms{};
void require(bool condition,const char* message){++checks;if(!condition){std::fprintf(stderr,"diagnostic_pilot: %s\n",message);std::exit(1);}}
std::string environment(const char* name){
#ifdef _WIN32
 char* raw=nullptr;std::size_t size{};_dupenv_s(&raw,&size,name);std::string value=raw?raw:"";std::free(raw);return value;
#else
 const auto* raw=std::getenv(name);return raw?raw:"";
#endif
}
void set_environment(const char* name,const std::string& value){
#ifdef _WIN32
 _putenv_s(name,value.c_str());
#else
 if(value.empty())unsetenv(name);else setenv(name,value.c_str(),1);
#endif
}
struct EnvironmentGuard {
 std::string target=environment("SPACE_VOYAGES_QA_TARGET"),rock=environment("SPACE_VOYAGES_QA_ASTEROID");
 std::string handoff=environment("SPACE_VOYAGES_QA_HANDOFF_TICK");
 std::string repeat=environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS");
 ~EnvironmentGuard(){set_environment("SPACE_VOYAGES_QA_TARGET",target);set_environment("SPACE_VOYAGES_QA_ASTEROID",rock);set_environment("SPACE_VOYAGES_QA_HANDOFF_TICK",handoff);set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS",repeat);}
};
void mode(const char* target,const char* rock){set_environment("SPACE_VOYAGES_QA_TARGET",target);set_environment("SPACE_VOYAGES_QA_ASTEROID",rock);set_environment("SPACE_VOYAGES_QA_HANDOFF_TICK","");set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS","");}
bool same_input(const FlightInput& a,const FlightInput& b){return a.horizontal==b.horizontal&&a.vertical==b.vertical&&a.boost==b.boost;}
std::array<bool,19> coordinates(){std::array<bool,19> result{};result.fill(true);return result;}
void launch(VoyageController& voyage,int origin,const std::array<bool,19>& available){
 require(voyage.begin(origin,available)&&voyage.begin_space_entry(),"native entry can start");
 for(int frame=0;frame<105;++frame)voyage.tick(1./60,{},false);
 require(voyage.phase()==Phase::FreeFlight,"original1.75second entry reaches free flight");
}
void input(DiagnosticPilot& pilot,const VoyageController& voyage,FlightInput& controls,bool& triangle){
 const auto began=std::chrono::steady_clock::now();watch_allocations=true;
 pilot.apply(voyage,controls,triangle);watch_allocations=false;
 maximum_apply_ms=std::max(maximum_apply_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
 require(allocations==0,"QA planning and steering allocate nothing in apply");
 require(std::isfinite(controls.horizontal)&&std::isfinite(controls.vertical)&&std::abs(controls.horizontal)<=1&&std::abs(controls.vertical)<=1,"QA emits finite native normalized input");
}
int complete(VoyageController& voyage,DiagnosticPilot& pilot,int maximum=18000){
 int frames=0;
 while(voyage.phase()!=Phase::Arrival&&frames<maximum){
  FlightInput controls{};bool triangle=false;input(pilot,voyage,controls,triangle);voyage.tick(1./60,controls,triangle);++frames;++flight_ticks;
 }
 if(voyage.phase()!=Phase::Arrival){const auto& state=voyage.flight();std::fprintf(stderr,"timeout origin=%d position=(%.2f,%.2f,%.2f) yaw=%.3f contacts=%llu\n",voyage.origin(),state.position.x,state.position.y,state.position.z,state.yaw,static_cast<unsigned long long>(voyage.asteroid_contacts().total_contacts));}
 require(voyage.phase()==Phase::Arrival,"QA steering converges to a real original arrival");return frames;
}
}
int main(){
 EnvironmentGuard guard;
 const auto& catalog=galaxy_catalog();const auto layout=build_asteroid_layout(catalog.data(),catalog.size());
 require(layout.complete()&&layout.asteroids().size()==176,"same176 physical asteroids as the shipped extension");
 VoyageController voyage;voyage.set_asteroids(layout,true);launch(voyage,3,coordinates());
 const FlightInput human{.31f,-.72f,false};
 {
  mode("","");DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  input(pilot,voyage,controls,triangle);require(!pilot.active()&&same_input(controls,human)&&triangle,"absent QA environment preserves every player control");
 }
 for(const char* invalid:{"-1","bad","1tail","18446744073709551616","176","999999"}){
  mode("1",invalid);DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  input(pilot,voyage,controls,triangle);require(same_input(controls,human)&&triangle,"invalid explicit asteroid index never consumes controls or falls back to planet QA");
 }
 {
  mode("1","0");DiagnosticPilot pilot;VoyageController no_asteroids;launch(no_asteroids,3,coordinates());FlightInput controls=human;bool triangle=true;
  input(pilot,no_asteroids,controls,triangle);require(same_input(controls,human)&&triangle,"asteroid QA without a layout leaves player input untouched");
 }
 for(bool rock_mode:{false,true}){
  VoyageController handoff;handoff.set_asteroids(layout,true);launch(handoff,3,coordinates());
  mode(rock_mode?"":"1",rock_mode?"0":"");set_environment("SPACE_VOYAGES_QA_HANDOFF_TICK",std::to_string(handoff.flight().ticks+3));DiagnosticPilot pilot;
  for(int frame=0;frame<3;++frame){FlightInput controls=human;bool triangle=true;input(pilot,handoff,controls,triangle);require(controls.boost&&!triangle,"handoff never releases QA before the exact native tick threshold");handoff.tick(1./60,controls,triangle);}
  for(int frame=0;frame<3;++frame){FlightInput controls=human;bool triangle=true;const auto before=handoff.flight().position;input(pilot,handoff,controls,triangle);require(same_input(controls,human)&&triangle,"handoff preserves every raw player input and Triangle at and after threshold");require(length_squared(handoff.flight().position-before)==0,"handoff does not modify a pose");handoff.tick(1./60,controls,triangle);}
  VoyageController fresh;fresh.set_asteroids(layout,true);launch(fresh,3,coordinates());FlightInput raw=human;bool held_triangle=true;pilot.completed();input(pilot,fresh,raw,held_triangle);require(same_input(raw,human)&&held_triangle,"handoff remains raw after a local flight tick reset or completed callback");
 }
 for(const char* invalid:{"0","-1","1tail","bad","18446744073709551616"}){
  mode("","0");set_environment("SPACE_VOYAGES_QA_HANDOFF_TICK",invalid);DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  input(pilot,voyage,controls,triangle);require(controls.boost&&!triangle,"invalid optional handoff is ignored rather than truncating diagnostic input");
 }
 {
  mode("","");set_environment("SPACE_VOYAGES_QA_HANDOFF_TICK","1");DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  input(pilot,voyage,controls,triangle);require(!pilot.active()&&same_input(controls,human)&&triangle,"handoff configuration alone never activates QA");
 }
 for(const char* asteroid:{"","bad","176"}){
  mode("",asteroid);set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS","1");DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  input(pilot,voyage,controls,triangle);require(same_input(controls,human)&&triangle,"repeat flag alone or with invalid asteroid index never activates input automation");
 }
 {
  mode("1","0");DiagnosticPilot pilot;FlightInput controls=human;bool triangle=true;
  const auto before=voyage.flight().position;input(pilot,voyage,controls,triangle);
  const auto delta=layout.asteroids()[0].position-before;
  const float horizontal=std::clamp(-wrap_angle(yaw_of(delta)-voyage.flight().yaw)*2.3f,-1.f,1.f);
  const float vertical=std::clamp(-(pitch_of(delta)-voyage.flight().pitch)*2.3f,-1.f,1.f);
  require(controls.horizontal==horizontal&&controls.vertical==vertical&&controls.boost&&!triangle,"asteroid QA aims at the chosen real center with boost and never enters a planet");
  require(length_squared(voyage.flight().position-before)==0,"applying diagnostic input never writes a ship pose");
  for(int frame=0;frame<6000&&voyage.asteroid_contacts().total_contacts==0;++frame){input(pilot,voyage,controls,triangle);voyage.tick(1./60,controls,triangle);require(!triangle&&voyage.phase()==Phase::FreeFlight,"deliberate impact bypasses detours and planet entry");}
  require(voyage.asteroid_contacts().total_contacts>0,"boosted native input produces an actual swept asteroid contact without teleporting");
 }
 {
  mode("","0");set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS","true");DiagnosticPilot invalid_word;
  set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS","01");DiagnosticPilot invalid_leading_zero;
  set_environment("SPACE_VOYAGES_QA_REPEAT_IMPACTS","1");DiagnosticPilot pilot;VoyageController crashes;crashes.set_asteroids(layout,true);launch(crashes,3,coordinates());
  unsigned acknowledgements=0,retreat_crossings=0;std::uint32_t events=0;bool retreat_pending=false,saw_retreat_command=false;const auto& rock=layout.asteroids()[0];
  int frame=0;
  for(;frame<20000&&crashes.ship_damage().respawns<2;++frame){
   FlightInput controls=human;bool triangle=true;const auto before=crashes.flight().position;input(pilot,crashes,controls,triangle);
   require(!triangle&&length_squared(crashes.flight().position-before)==0,"repeat impact QA uses inputs only and never selects a destination or writes pose");
   if(crashes.ship_damage().destroyed)require(same_input(controls,{})&&crashes.take_destination()==-1,"destroyed repeated-impact QA waits neutrally for owner presentation");
   if(!crashes.ship_damage().destroyed){
    const auto delta=rock.position-before;const auto& state=crashes.flight();
    const FlightInput direct{std::clamp(-wrap_angle(yaw_of(delta)-state.yaw)*2.3f,-1.f,1.f),std::clamp(-(pitch_of(delta)-state.pitch)*2.3f,-1.f,1.f),true};
    for(auto* invalid:{&invalid_word,&invalid_leading_zero}){FlightInput original=human;bool held=true;input(*invalid,crashes,original,held);require(same_input(original,direct)&&!held,"only exact repeat flag1 enables retreat; true and01 remain ordinary asteroid aiming");}
   }
   if(crashes.ship_damage().damage_events>events){
    events=crashes.ship_damage().damage_events;
    if(!crashes.ship_damage().destroyed){
     const auto& state=crashes.flight();const auto outward=unit(before-rock.position,-direction(state.yaw,state.pitch));const auto retreat=rock.position+outward*(rock.radius+AsteroidContacts::ship_radius+65.f)-before;
     const FlightInput expected{std::clamp(-wrap_angle(yaw_of(retreat)-state.yaw)*2.3f,-1.f,1.f),std::clamp(-(pitch_of(retreat)-state.pitch)*2.3f,-1.f,1.f),true};
     require(same_input(controls,expected),"each nonlethal new damage event steers to the exact65unit-clearance retreat point");retreat_pending=true;saw_retreat_command=true;
    }else retreat_pending=false;
   }
   if(retreat_pending&&crashes.phase()==Phase::FreeFlight&&!crashes.ship_damage().destroyed&&length(before-rock.position)>rock.radius+AsteroidContacts::ship_radius+50.f){++retreat_crossings;retreat_pending=false;}
   if(crashes.ship_damage().respawn_ready){require(crashes.acknowledge_destruction_frame(),"CPU test explicitly supplies owner black-frame acknowledgement; not GPU proof");++acknowledgements;}
   crashes.tick(1./60,controls,triangle);
  }
  std::printf("repeat impacts: %d ticks, damage_events=%u respawns=%u ack=%u hull=%d\n",frame,crashes.ship_damage().damage_events,crashes.ship_damage().respawns,acknowledgements,crashes.ship_damage().hull);
  require(crashes.ship_damage().respawns==2&&acknowledgements==2&&crashes.ship_damage().damage_events>=8,"normalized repeated impact controls complete two real damage/death/respawn cycles");
  require(saw_retreat_command&&retreat_crossings>=6,"repeated impacts genuinely detach beyond50units between nonlethal contacts");
  require(crashes.origin()==3&&crashes.phase()==Phase::Departure,"repeated crashes retain origin and return to local intro, not a native arrival");
 }
 {
  mode("18,17,2,1,4,6,5,7,8,9,10,11,12,13,14,15,16,3","");DiagnosticPilot pilot;VoyageController tour;tour.set_asteroids(layout,true);int origin=3;
  constexpr int targets[]={18,17,2,1,4,6,5,7,8,9,10,11,12,13,14,15,16,3};
  for(int target:targets){launch(tour,origin,coordinates());const int frames=complete(tour,pilot);require(tour.take_destination()==target,"full tour chooses the intended unlocked destination");std::printf("trip %d -> %d: %d ticks, %llu contacts\n",origin,target,frames,static_cast<unsigned long long>(tour.asteroid_contacts().total_contacts));require(tour.arrival_complete(),"native Ground acknowledgement completes the trip");pilot.completed();origin=target;}
  require(tour.phase()==Phase::Ground,"all18 diagnostic legs finish with solid asteroids");
 }
 {
  // Force a direct source/target ray through one physical rock. Its field's
  // envelope is a waypoint hint, not an impassable sphere or a pose rewrite.
  AsteroidFieldDefinition field{99,0x87651234u,{0,0,0},{55,40,35},{0,1,0},0,24};
  auto cloud=build_asteroid_layout(nullptr,0,&field,1,{});require(cloud.complete(),"contact-field fixture is complete");
  const auto rock=cloud.asteroids()[0];std::array<Destination,2> planets{{{1,"Source",{-300,rock.position.y,rock.position.z},10},{3,"Target",{300,rock.position.y,rock.position.z},10}}};
  VoyageController head_on(planets.data(),planets.size());head_on.set_asteroids(cloud,true);launch(head_on,1,coordinates());
  for(int frame=0;frame<1500;++frame)head_on.tick(1./60,{0,0,true},false);
  require(head_on.asteroid_contacts().total_contacts>0&&length(head_on.flight().position-planets[1].position)>100,"fixture reproduces a real persistent boosted head-on obstruction");
  mode("3","");DiagnosticPilot pilot;VoyageController crossing(planets.data(),planets.size());crossing.set_asteroids(cloud,true);launch(crossing,1,coordinates());
  require(length(cross(planets[1].position-crossing.flight().position,rock.position-crossing.flight().position))/length(planets[1].position-crossing.flight().position)<.001f,"unchanged departure ray would hit the chosen blocker head-on");
  complete(crossing,pilot);require(crossing.take_destination()==3,"detour completes the formerly blocked real contact-field route");
  require(crossing.asteroid_contacts().total_contacts==0,"anticipatory QA steering avoids the reproduced head-on contact");
 }
 {
  // A field envelope is not its solid geometry. The destination and the
  // subsequent origin are both inside this envelope's native planet hole.
  std::array<Destination,2> planets{{{1,"Outside",{-400,0,0},10},{3,"Inside envelope",{0,0,0},10}}};
  AsteroidFieldDefinition field{199,0x15326478u,{0,0,0},{180,160,140},{1,0,0},0,48};
  auto cloud=build_asteroid_layout(planets.data(),planets.size(),&field,1,{});require(cloud.complete(),"planet-hole field fixture is complete");
  require(length(planets[1].position-cloud.fields()[0].center)+planets[1].radius<cloud.fields()[0].radius,"fixture actually embeds the planet in the conservative field envelope");
  mode("3,1","");DiagnosticPilot pilot;VoyageController holes(planets.data(),planets.size());holes.set_asteroids(cloud,true);
  launch(holes,1,coordinates());complete(holes,pilot);require(holes.take_destination()==3,"planner reaches the actual empty destination hole inside a field bound");
  require(holes.arrival_complete(),"inside-envelope destination returns Ground");pilot.completed();
  launch(holes,3,coordinates());complete(holes,pilot);require(holes.take_destination()==1,"planner also exits a source hole inside the field envelope");
 }
 {
  mode("1","");DiagnosticPilot pilot;VoyageController blocked;blocked.set_asteroids(layout,true);auto available=coordinates();available[1]=false;launch(blocked,3,available);
  for(int frame=0;frame<3600;++frame){FlightInput controls{};bool triangle=true;input(pilot,blocked,controls,triangle);blocked.tick(1./60,controls,triangle);require(!triangle&&blocked.phase()==Phase::FreeFlight,"QA never enters a locked requested planet");}
 }
 std::printf("PASS %u checks; flight_ticks=%llu; max_apply_ms=%.3f; apply_allocations=%zu\n",checks,static_cast<unsigned long long>(flight_ticks),maximum_apply_ms,allocations);
}

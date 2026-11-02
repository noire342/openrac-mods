// SPDX-License-Identifier: GPL-3.0-or-later
// CPU-only Session state audit. No native game, GL initialization, disc or saves.
#include "../src/mod.cpp"
#include <cstdlib>
#include <vector>
namespace {
int fake_level=3;
std::vector<unsigned char> fake_memory(0x2000000);
openrac_api fake_api{};
unsigned checks{};
Session* watched_session{};
bool watch_abort_reads{},stale_abort_read{};
unsigned executable_reads{},level_reads{};
bool fade_available=true;
unsigned fade_calls{},transient_calls{},original_calls{},callback_pumps{};
int last_fade_frames{},expected_destination=1,observed_destination=-1;
bool guarded_reentry{},freeze_during_fade{},fade_before_transient{},preserve_during_original{};
std::string last_lookup;
void require(bool pass,const char* message){++checks;if(!pass){std::fprintf(stderr,"session_audit: %s\n",message);std::exit(1);}}
template<class T>void put(unsigned at,T value){std::memcpy(fake_memory.data()+at,&value,sizeof value);}
void reset_world(const disc::Binding& disc=disc::ntsc_u){
 std::fill(fake_memory.begin(),fake_memory.end(),static_cast<unsigned char>(0));fake_level=3;
 put<float>(disc.frame_scale.value,1);put<float>(disc.frame_scale_squared.value,1);put<float>(disc.frame_time.value,1.f/60);
 put<int>(disc.cinematic_mode.value,6);put<unsigned>(disc.shared_ship.value,0x20000);put<unsigned short>(0x200a6,531);
 put<unsigned char>(disc.coordinates.value+1,1);put<unsigned char>(disc.coordinates.value+3,1);
 fade_available=true;fade_calls=transient_calls=original_calls=callback_pumps=0;last_fade_frames=0;
 guarded_reentry=freeze_during_fade=fade_before_transient=preserve_during_original=false;
 expected_destination=1;observed_destination=-1;last_lookup.clear();
}
Session make_session(){return Session(&fake_api,nullptr,disc::ntsc_u,SceneAssets{});}
void fake_fade(int frames){
 ++fade_calls;last_fade_frames=frames;
 auto& s=*watched_session;
 const auto memory_before=fake_memory;
 const unsigned frames_before=s.frames,ticks_before=s.scene_ticks;
 const auto physical_before=s.voyage.flight(),visual_before=s.voyage.presentation_flight();
 const float entry_before=s.voyage.space_entry_progress();const auto phase_before=s.voyage.phase();
 const unsigned reads_before=executable_reads+level_reads;
 guarded_reentry=s.in_scene;
 // Native FadeToBlack(12) yields14 times. Model only the Session callback:
 // actual native counters/rendering and other mods are outside this CPU audit.
 for(int i=0;i<frames+2;++i){++callback_pumps;s.frame();}
 guarded_reentry=guarded_reentry&&reads_before==executable_reads+level_reads&&s.frames==frames_before;
 const auto physical_after=s.voyage.flight(),visual_after=s.voyage.presentation_flight();
 freeze_during_fade=fake_memory==memory_before&&s.scene_ticks==ticks_before&&s.voyage.phase()==phase_before&&s.voyage.space_entry_progress()==entry_before&&
  physical_before.ticks==physical_after.ticks&&length(physical_before.position-physical_after.position)==0&&
  visual_before.ticks==visual_after.ticks&&length(visual_before.position-visual_after.position)==0&&length(visual_before.camera.position-visual_after.camera.position)==0;
}
void* fake_function(const char* name){
 last_lookup=name?name:"";
 if(fade_available&&name&&(!std::strcmp(name,"FUN_001f4a58")||!std::strcmp(name,"func_001F4E08")))return reinterpret_cast<void*>(&fake_fade);
 return nullptr;
}
int fake_transient(openrac_mod*,const openrac_transient_source* source){
 ++transient_calls;
 fade_before_transient=fade_calls==1&&last_fade_frames==12&&guarded_reentry&&freeze_during_fade;
 require(source&&source->user==watched_session&&watched_session->in_scene,"scene acquisition follows the guarded native fade and receives this Session");
 // The mock selects a previously unlocked destination without any renderer.
 watched_session->entered=expected_destination;watched_session->arrival_presented=true;return OPENRAC_HOOK_OK;
}
void fake_original(){
 ++original_calls;preserve_during_original=watched_session->preserving_ship;
 observed_destination=watched_session->read<int>(watched_session->binding.pending_destination.value);
}
}
int main(){
 fake_api.memory=fake_memory.data();
 const auto track_read=[](){
  if(watch_abort_reads&&watched_session){const auto& s=*watched_session;
   if(s.armed||s.origin!=-1||s.entered!=-1||s.arrival_presented||s.boarding_exterior_sought||s.boarding_tail_finished||s.voyage.phase()!=Phase::Ground||s.voyage.origin()!=-1)stale_abort_read=true;
  }
 };
 static const auto track_native_read=track_read;
 fake_api.relocate_data=+[](unsigned at){++executable_reads;track_native_read();return at;};
 fake_api.relocate_level_data=+[](int,unsigned at){++level_reads;track_native_read();return at;};fake_api.loaded_level=+[](){return fake_level;};
 fake_api.setting_bool=+[](openrac_mod*,const char*){return 0;};fake_api.log=+[](openrac_mod*,int,const char*){};
 fake_api.function=fake_function;fake_api.transient_run=fake_transient;original=fake_original;
 reset_world();auto s=make_session();s.frame();require(s.armed&&s.voyage.phase()==Phase::Departure,"stock eligible boarding arms Departure");
 s.voyage.departure_complete();s.in_scene=true;
 s.previous_visual=s.voyage.presentation_flight();s.previous_visual_phase=s.voyage.phase();
 const auto before=s.voyage.presentation_flight();
 require(s.tick(1.f/60,0,nullptr)==1,"free flight advances without native callbacks");
 const auto moved=s.voyage.presentation_flight();require(moved.ticks==before.ticks+1,"one Session callback advances one native step");
 require(s.tick(1.f/60,8,nullptr)==1&&s.paused,"Start edge pauses");
 const auto paused=s.voyage.presentation_flight();
 for(int i=0;i<10;++i)s.tick(1.f/60,0,nullptr);
 const auto paused_after=s.voyage.presentation_flight();
 require(paused_after.ticks==paused.ticks&&length(paused_after.position-paused.position)==0&&length(paused_after.camera.position-paused.camera.position)==0,"paused snapshots remain complete and do not run springs or movement");
 require(s.tick(1.f/60,8,nullptr)==1&&!s.paused,"released and pressed Start resumes");
 require(s.voyage.presentation_flight().ticks==paused.ticks+1,"resume advances one step instead of applying paused elapsed time");
 s.tick(1.f/60,0,nullptr);s.tick(1.f/60,0x1000,nullptr);
 require(s.voyage.phase()==Phase::Approach,"origin return can enter the normal approach");
 for(int i=0;i<65&&s.voyage.phase()!=Phase::Arrival;++i)s.tick(1.f/60,0,nullptr);
 require(s.voyage.phase()==Phase::Arrival&&s.entered==3&&!s.arrival_presented,"reaching Arrival does not itself acknowledge a rendered frame");
 const auto arrival=s.voyage.presentation_flight();
 for(int i=0;i<6;++i)require(s.tick(1.f/60,0,nullptr)==1,"catch-up ticks cannot release Arrival before a draw");
 require(s.voyage.presentation_flight().ticks==arrival.ticks&&length(s.voyage.presentation_flight().position-arrival.position)==0,"unpresented Arrival catch-up ticks keep the complete final visual state");
 s.arrival_presented=true;require(s.tick(1.f/60,0,nullptr)==0,"acknowledged Arrival yields once to the native loader");
 for(int interruption:{4,-1,19}){
  reset_world();auto interrupted=make_session();interrupted.frame();require(interrupted.armed&&interrupted.voyage.phase()==Phase::Departure,"abort fixture begins with an armed original departure");
  interrupted.entered=1;interrupted.arrival_presented=true;interrupted.boarding_exterior_sought=interrupted.boarding_tail_finished=true;
  fake_level=interruption;put<int>(0x15f5c4,0);const auto native_before=fake_memory;
  watched_session=&interrupted;watch_abort_reads=true;stale_abort_read=false;executable_reads=level_reads=0;
  interrupted.frame();watch_abort_reads=false;watched_session=nullptr;
  require(!interrupted.armed&&interrupted.origin==-1&&interrupted.entered==-1&&!interrupted.arrival_presented&&!interrupted.boarding_exterior_sought&&!interrupted.boarding_tail_finished,"unexpected level change clears all departure and presentation bookkeeping");
  require(interrupted.voyage.phase()==Phase::Ground&&interrupted.voyage.origin()==-1,"unexpected level change cancels the core instead of leaving Departure stuck");
  require(!stale_abort_read,"old departure state is canceled before any new-level global relocation");
  require(fake_memory==native_before,"abort cleanup never writes the old departure's metadata into a new level or frontend");
  if(interruption<0||interruption>18)require(executable_reads==0&&level_reads==0,"frontend and unsupported levels return without touching native globals after cleanup");
  fake_level=4;put<int>(0x15f5c4,6);interrupted.frame();
  require(interrupted.armed&&interrupted.origin==4&&interrupted.voyage.phase()==Phase::Departure&&interrupted.voyage.origin()==4,"next eligible boarding works after a planet change or a frontend round trip");
 }
 // Both source-confirmed names must resolve with their own regional binding.
 for(const auto* selected:{&disc::ntsc_u,&disc::pal}){
  reset_world(*selected);Session handoff(&fake_api,nullptr,*selected,SceneAssets{});handoff.frame();
  require(handoff.armed&&last_lookup==selected->fade_to_black,"both regional native fade capabilities are checked before arming");
  put<int>(selected->pending_destination.value,1);watched_session=&handoff;handoff.transition();watched_session=nullptr;
  require(fade_calls==1&&last_fade_frames==12&&callback_pumps==14,"one handoff calls the native12-frame helper once and models all14 yielding points");
  require(guarded_reentry,"fade-pumped Session callbacks return before native relocation or world frame bookkeeping");
  require(freeze_during_fade,"fade-pumped callbacks advance neither native progress nor physical/visual flight or intro time");
  require(fade_before_transient&&transient_calls==1,"native fade completes before exclusive scene acquisition");
  require(original_calls==1&&observed_destination==1&&preserve_during_original,"successful handoff loads the chosen destination once inside ship-preservation scope");
  require(!handoff.armed&&!handoff.in_scene&&!handoff.preserving_ship,"successful handoff releases its guards and restores the previous ship-preservation flag");
  reset_world(*selected);fade_available=false;Session unavailable(&fake_api,nullptr,*selected,SceneAssets{});const auto native_before=fake_memory;unavailable.frame();
  require(!unavailable.armed&&unavailable.voyage.phase()==Phase::Ground&&last_lookup==selected->fade_to_black,"missing fade capability leaves native boarding unarmed in both regions");
  require(fake_memory==native_before&&fade_calls==0&&transient_calls==0&&original_calls==0,"missing capability does not alter native memory or invoke a partial handoff");
 }
 for(int failure:{0,1,2}){
  reset_world();auto fallback=make_session();fallback.frame();require(fallback.armed,"fallback fixture begins with a valid capability and native timing");
  put<int>(disc::ntsc_u.pending_destination.value,1);
  if(failure==0)fade_available=false;
  if(failure==1)put<float>(disc::ntsc_u.frame_scale.value,1.2f);
  if(failure==2)fallback.voyage.cancel();
  const auto native_before=fake_memory;watched_session=&fallback;fallback.transition();watched_session=nullptr;
  require(fade_calls==0&&transient_calls==0,"lost capability, timing mismatch or invalid core phase cannot partially enter a transient scene");
  require(original_calls==1&&observed_destination==3&&preserve_during_original,"each late handoff failure discards staging and invokes original travel once toward the origin with its ship preserved");
  require(!fallback.armed&&!fallback.in_scene&&!fallback.preserving_ship&&fallback.entered==-1&&!fallback.arrival_presented&&fallback.voyage.phase()==Phase::Ground,"each fallback cancels the core and restores all handoff guards");
  auto allowed=native_before;const int origin=3;std::memcpy(allowed.data()+disc::ntsc_u.pending_destination.value,&origin,sizeof origin);
  require(fake_memory==allowed,"fallback changes only the pending native destination from staging to origin");
 }
 fake_api.setting_bool=+[](openrac_mod*,const char* key)->int{return !std::strcmp(key,"asteroids_enabled")||!std::strcmp(key,"asteroid_contacts");};
 reset_world();SceneAssets old_pack;old_pack.full_galaxy=true;
 Session fallback_pack(&fake_api,nullptr,disc::ntsc_u,std::move(old_pack));
 require(!fallback_pack.voyage.asteroid_layout(),"older full-galaxy resource packs retain ordinary free flight instead of failing the mod");
 fallback_pack.frame();require(fallback_pack.armed,"missing optional asteroid models do not block an eligible original boarding");
 reset_world();SceneAssets rock_pack;rock_pack.full_galaxy=true;
 MeshAsset rock0,rock1;rock0.id="asteroid_0";rock1.id="asteroid_1";rock_pack.meshes.push_back(std::move(rock0));rock_pack.meshes.push_back(std::move(rock1));
 Session enabled_pack(&fake_api,nullptr,disc::ntsc_u,std::move(rock_pack));
 require(enabled_pack.voyage.asteroid_layout()&&enabled_pack.voyage.asteroid_layout()->asteroids().size()==176,"the new optional pack installs fixed scene geometry without GL or native memory writes");
 enabled_pack.frame();require(enabled_pack.armed,"enabled asteroid geometry preserves the normal native boarding capability checks");
 for(int ship_class:{531,532,533}){
  reset_world();put<unsigned short>(0x200a6,static_cast<unsigned short>(ship_class));
  fake_api.setting_bool=+[](openrac_mod*,const char* key)->int{return std::strcmp(key,"trace")&&std::strcmp(key,"direct_vertical");};
  SceneAssets scene;scene.full_galaxy=true;scene.health_meter.indices={0};
  for(const char* name:{"asteroid_0","asteroid_1","projectile_laser"}){MeshAsset mesh;mesh.id=name;scene.meshes.push_back(std::move(mesh));}
  for(unsigned i=1;i<3;++i){auto& weapon=scene.combat.ships[i];weapon.armed=true;weapon.count=2;weapon.muzzles[0]={{.6f,.3f,0},{1,0,0}};weapon.muzzles[1]={{.6f,-.3f,0},{1,0,0}};}
  Session guns(&fake_api,nullptr,disc::ntsc_u,std::move(scene));guns.frame();
  require(guns.ship==ship_class&&guns.voyage.phase()==Phase::Departure,"Session uses the actually boarded class before configuring scene guns");
  require(guns.voyage.combat().state().armed==(ship_class!=531),"the green ship remains unarmed while the two declared armed ships receive their own gun state");
  require(guns.voyage.asteroid_damage_enabled(),"ship damage requires both physical contacts and the visible native health arc");
  require(guns.voyage.departure_complete(),"CPU gun fixture explicitly completes its intro");guns.in_scene=true;guns.snapshot_visual();
  const auto native_before=fake_memory;
  for(int tick=0;tick<60;++tick)guns.tick(1.f/60,0x8000,nullptr);
  require(guns.voyage.combat().state().shots==(ship_class==531?0:15),"SDK Square fires at native15Hz only for the actually boarded armed ships");
  const auto shots=guns.voyage.combat().state().shots;guns.tick(1.f/60,8,nullptr);
  for(int tick=0;tick<90;++tick)guns.tick(1.f/60,0x8400,nullptr);
  require(guns.paused&&guns.voyage.combat().state().shots==shots,"held Square/L1 cannot fire, accumulate time or consume anything while paused");
  require(fake_memory==native_before&&original_calls==0&&transient_calls==0,"scene gun, damage and pause paths change no bytes of native32MiB and invoke no native loader");
 }
 std::printf("Session CPU checks:%u passed; abort/rearm, pause/catchup/arrivalack, regional fade reentry/freeze/order, optional scene geometry and staging-safe fallback\n",checks);
 return 0;
}

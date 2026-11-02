// SPDX-License-Identifier: GPL-3.0-or-later
#include <openrac/openrac_mod.h>
#include "renderer.hpp"
#include "space_voyages_api.h"
#include "diagnostic_pilot.hpp"
#include "diagnostic_scene_control.hpp"
#include "input_adapter.hpp"
#include "travel_policy.hpp"
#include "flight_presentation.hpp"
#include "disc_binding.hpp"
#include "boarding_timeline.hpp"
#include "scene_audio_sdk.hpp"
#include "destination_client.hpp"
#include "garage_provider.hpp"
#include "ship_identity.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace {
using namespace spacevoyages;
using Transition=void(*)();Transition original{};
Transition original_initialize{};
using SaveData=int(*)(int,int);SaveData original_save{};
struct FlagScope {
 bool& flag;bool previous;
 explicit FlagScope(bool& value):flag(value),previous(value){flag=true;}
 ~FlagScope(){flag=previous;}
};
std::filesystem::path library_assets(){
#ifdef _WIN32
 HMODULE module{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&library_assets),&module))return {};
 wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return {};
 return std::filesystem::path(path).parent_path().parent_path().parent_path()/"assets";
#else
 Dl_info info{};if(!dladdr(reinterpret_cast<void*>(&library_assets),&info))return {};
 return std::filesystem::path(info.dli_fname).parent_path().parent_path().parent_path()/"assets";
#endif
}
struct Listener {space_voyages_listener_v1 fn{};void* user{};unsigned token{};};
struct Session {
 const openrac_api* api;openrac_mod* mod;const disc::Binding& binding;SceneAssets assets;SceneRenderer renderer;
 VoyageController voyage;DestinationClient destination;GarageProvider garage;
 std::array<Listener,16> listeners{};unsigned next_token=1;
 SceneAudioSdk audio_sdk;SceneAudio audio;bool audio_enabled{},audio_started{};
 DiagnosticPilot diagnostics;
 DiagnosticSceneControl diagnostic_scene;
 bool armed{},in_scene{},paused{},last_start{},trace{},direct_vertical{},engine_flames{},preparing_departure{},preserving_ship{},timing_fallback_logged{},fade_fallback_logged{};int origin=-1,ship=531;
 bool boarding_exterior_sought{},boarding_tail_finished{};
 unsigned frames{},scene_ticks{};int entered=-1;std::string error;
 std::chrono::steady_clock::time_point previous_tick{};
 double frame_ms{},frame_max_ms{},draw_ms{},draw_max_ms{};unsigned timed_frames{},timed_draws{};
 FlightState previous_visual{};float previous_entry{},previous_approach{};
 double previous_scene_seconds{};
 ShipDamageState previous_damage{};
 Phase previous_visual_phase=Phase::Ground;
 bool arrival_presented{};
 int last_prompt_candidate=-2;Phase last_prompt_phase=Phase::Ground;
 bool diagnostic_subtractive_blend{};
 ShipIdentity identity=ShipIdentity::Green;
 bool awaiting_fleet{};int awaiting_fleet_origin=-1;
 DestinationRoute entered_route{};bool native_only_departure{};int native_only_origin=-1;
 DestinationRoute progress_origin{};
 explicit Session(const openrac_api* a,openrac_mod* m,const disc::Binding& selected,SceneAssets data):api(a),mod(m),binding(selected),assets(std::move(data)),renderer(assets),voyage(assets.full_galaxy?galaxy_catalog().data():prototype_catalog().data(),assets.full_galaxy?galaxy_catalog().size():prototype_catalog().size()),destination(a,m),garage(!std::strcmp(selected.version,"pal")?GarageRegion::Pal:GarageRegion::Ntsc){
  trace=api->setting_bool(mod,"trace")!=0;direct_vertical=api->setting_bool(mod,"direct_vertical")!=0;
  diagnostic_scene=DiagnosticSceneControl::from_environment(trace);
#ifdef _WIN32
  char poisoned[2]{};
  diagnostic_subtractive_blend=trace&&GetEnvironmentVariableA("SPACE_VOYAGES_QA_SUBTRACTIVE_BLEND",poisoned,sizeof poisoned)==1&&poisoned[0]=='1';
#else
  const auto* poisoned=std::getenv("SPACE_VOYAGES_QA_SUBTRACTIVE_BLEND");
  diagnostic_subtractive_blend=trace&&poisoned&&std::strcmp(poisoned,"1")==0;
#endif
  audio_sdk={api,mod};
  garage.bind_loaded();
  audio_enabled=assets.audio_available;
  if(!assets.audio_available)log("Scene-audio resources absent; legacy asset pack has no space soundtrack or effects");
  engine_flames=api->setting_bool(mod,"engine_flames")!=0;
  configure_asteroids();
  const bool damage=api->setting_bool(mod,"asteroid_damage")&&api->setting_bool(mod,"asteroid_contacts")&&voyage.asteroid_layout()&&!assets.health_meter.indices.empty();
  voyage.set_asteroid_damage(damage);
  if(damage)log("Scene-local ship damage enabled; native radar health arc and local-space respawn");
  if(api->setting_bool(mod,"ship_weapons")&&assets.has(assets.projectile_effects.bolt_asset_id))voyage.set_combat_assets(assets.combat);
 }
 void configure_asteroids(){
  if(!assets.full_galaxy||!api->setting_bool(mod,"asteroids_enabled"))return;
  const auto present=[&](const std::string& id){for(const auto& mesh:assets.meshes)if(mesh.id==id||mesh.id.find(id+"_part")==0)return true;return false;};
  if(!present("asteroid_0")||!present("asteroid_1")){log("Original asteroid resources absent; ordinary free flight retained");return;}
  auto layout=build_asteroid_layout(voyage.catalog_data(),voyage.catalog_count());
  if(!layout.complete()){log("Asteroid field geometry unavailable; ordinary free flight retained");return;}
  voyage.set_asteroids(std::move(layout),api->setting_bool(mod,"asteroid_contacts")!=0);
  log("Original Gemlik asteroids enabled: fixed fields and protected planetary transitions");
 }
 template<class T>T read(unsigned at)const{T v{};if(at>=0x10000&&at<=0x2000000-sizeof(T))std::memcpy(&v,api->memory+at,sizeof v);return v;}
 template<class T>void write(unsigned at,T v){if(at>=0x10000&&at<=0x2000000-sizeof(T))std::memcpy(api->memory+at,&v,sizeof v);}
 unsigned address(disc::NativeAddress at)const{return disc::resolve(at,api->relocate_data,api->relocate_level_data);}
 unsigned shared_ship()const{return address(binding.shared_ship);}
 void log(const char* s)const{api->log(mod,OPENRAC_LOG_INFO,s);}
 std::array<bool,19> native_coordinates()const{std::array<bool,19> out{};for(unsigned i=0;i<out.size();++i)out[i]=read<unsigned char>(address(binding.coordinates)+i)!=0;return out;}
 std::array<std::uint8_t,19> native_visits()const{return read<std::array<std::uint8_t,19>>(address(binding.visits));}
 std::array<bool,19> coordinates()const{
  // A custom world's native template flags are local simulation state, not
  // campaign progress. A failed Home query supplies no fallback RAM flags.
  return progress_origin.variant==DestinationVariant::VeldinGarage?
      garage.campaign_observation().coordinates:native_coordinates();
 }
 std::array<std::uint8_t,19> visits()const{
  return progress_origin.variant==DestinationVariant::VeldinGarage?
      garage.campaign_observation().visits:native_visits();
 }
 bool timing_supported(){
  if(disc::native_sixty_hz(read<float>(address(binding.frame_scale)),read<float>(address(binding.frame_scale_squared)),read<float>(address(binding.frame_time))))return true;
  if(!timing_fallback_logged){api->log(mod,OPENRAC_LOG_WARN,"Active game timing is not the supported native60Hz mode; original ship travel retained");timing_fallback_logged=true;}
  return false;
 }
 bool origin_supported(int id)const{return find_destination(voyage.catalog_data(),voyage.catalog_count(),id)!=nullptr;}
 bool refresh_routes(DestinationRoute* observed_origin=nullptr,bool* waiting_provider=nullptr){
  if(waiting_provider)*waiting_provider=false;
  progress_origin={};
  DestinationRoute current{api->loaded_level(),DestinationVariant::Campaign};std::uint32_t generation{};
  if(destination.supported()){
   if(!destination.current(current,&generation)){voyage.set_route_availability({});return false;}
  }else if(api->play_status){openrac_play_status state{};state.size=sizeof state;if(api->play_status(&state)==OPENRAC_HOOK_OK)generation=state.loads;}
  garage.poll(generation);const auto& fleet=garage.observation();openrac_destination home{};
  // A present provider makes native531 ambiguous: it may be the Original's
  // hidden controller rather than Green. Missing/stale ownership must never
  // be treated as if no provider existed, on either SDK13 or SDK14.
  if(garage.bound()&&!fleet.ready){if(waiting_provider)*waiting_provider=true;voyage.set_route_availability({});return false;}
  const auto campaign_status=garage.poll_campaign(generation);
  const bool canonical_ready=campaign_status==CampaignReadStatus::Ready||campaign_status==CampaignReadStatus::Inactive;
  const bool home_ready=!std::strcmp(binding.version,"pal")&&fleet.ready&&canonical_ready&&
      destination.lookup({18,DestinationVariant::VeldinGarage},home);
  voyage.set_route_availability({home_ready});
  // The older bridge can continue campaign travel, but cannot name or mount
  // a logical Home destination safely. Leave that trip to the native provider.
  if(!destination.supported()&&fleet.active)return false;
  if(current.variant==DestinationVariant::VeldinGarage){
   if(!home_ready||!fleet.active||campaign_status!=CampaignReadStatus::Ready){
    if(waiting_provider&&fleet.active&&campaign_status==CampaignReadStatus::NotReady)*waiting_provider=true;
    return false;
   }
  }
  progress_origin=current;
  if(observed_origin)*observed_origin=current;return current.valid();
 }
 void retain_native_departure(int current,const char* reason){
  awaiting_fleet=false;awaiting_fleet_origin=-1;
  destination.cancel();armed=false;entered=-1;entered_route={};arrival_presented=false;
  boarding_exterior_sought=boarding_tail_finished=false;voyage.cancel();
  native_only_departure=true;native_only_origin=current;log(reason);event();
 }
 const ShipProfile* boarding_profile()const{
  const auto& fleet=garage.observation();if(garage.bound()&&!fleet.ready)return nullptr;
  const auto actual=read<unsigned short>(read<unsigned>(shared_ship())+0xa6);
  const auto selected=fleet.ready?ship_identity_from_stable_id(static_cast<std::uint32_t>(fleet.selected_ship)):ship_identity_from_native_class(actual);
  const auto* profile=selected?ship_profile(*selected):nullptr;
  if(!profile||!compatible_boarding_actor(profile->identity,actual))return nullptr;
  if(profile->identity==ShipIdentity::Original&&
     (!assets.original_ship.available||!assets.original_ship.cockpit_available))return nullptr;
  return profile;
 }
 bool selected_route_valid()const{
  return destination.supported()?entered_route.valid()&&voyage.resolved_route(entered)==entered_route: voyage.destination_unlocked(entered);
 }
 using FadeToBlack=void(*)(int);
 FadeToBlack native_departure_fade(){
  const auto fade=reinterpret_cast<FadeToBlack>(api->function(binding.fade_to_black));
  if(!fade&&!fade_fallback_logged){api->log(mod,OPENRAC_LOG_WARN,"Native departure fade is unavailable; original ship travel retained");fade_fallback_logged=true;}
  return fade;
 }
 void skip_boarding_console(int current,int mode,int ship_state){
  if((!armed&&!awaiting_fleet)||current!=(armed?origin:awaiting_fleet_origin)||mode!=6||ship_state!=0||read<int>(address(binding.menu))==14||!timing_supported())return;
  const unsigned sequence=address(binding.boarding_sequence);
  const boarding::Playback playback{read<int>(sequence+0x34),read<int>(sequence+0x38),read<int>(sequence+0x3c),read<int16_t>(sequence+0x40),read<int16_t>(shared_ship()+disc::ship_variant_offset)};
  const auto* profile=armed?ship_profile(identity):boarding_profile();
  const auto entry=profile?profile->boarding_entry:boarding::EntryPresentation::PreserveEntry;
  const auto plan=boarding::plan(playback,true,boarding_exterior_sought,boarding_tail_finished,entry);
  if(plan.action==boarding::Action::SeekExterior){
   const auto parse=reinterpret_cast<void(*)(int)>(api->function(binding.parse_boarding_bank));
   if(!parse)return;
   // Set the once flag before a native resource load, which may pump ticks.
   boarding_exterior_sought=true;parse(plan.bank);
   write<int>(sequence+0x34,plan.time_before_next_tick);
   write<int>(sequence+0x38,plan.frame_before_next_tick);
   write<int>(sequence+0x3c,plan.bank);
   log("Boarding console omitted; authored exterior shot resumes next tick");
  }else if(plan.action==boarding::Action::FinishBoarding){
   boarding_tail_finished=true;write<int>(sequence+0x34,plan.time_before_next_tick);
   log("Boarding dashboard omitted; native cleanup resumes next tick");
  }
 }
 int provisional_destination(const std::array<bool,19>& available)const{
  int fallback=-1;for(std::size_t i=0;i<voyage.catalog_count();++i){const auto& d=voyage.catalog_data()[i];
   if(celestial_body_for_level(d.level)==celestial_body_for_level(origin))continue;
   if(coordinates_for_body(available,d.level))return d.level;
   if(fallback<0)fallback=d.level;
  }return fallback;
 }
 space_voyages_state_v1 state()const{space_voyages_state_v1 s{};s.size=sizeof s;s.phase=unsigned(voyage.phase());s.origin=voyage.origin();s.destination=voyage.target_level();s.candidate=voyage.candidate()?voyage.candidate()->level:-1;s.candidate_unlocked=voyage.candidate_unlocked();s.paused=paused;const auto f=voyage.flight();s.position[0]=f.position.x;s.position[1]=f.position.y;s.position[2]=f.position.z;s.speed=f.speed;return s;}
 void event(){const auto snapshot=state();const auto copy=listeners;for(const auto& l:copy)if(l.fn){bool live=false;for(const auto& current:listeners)if(current.token==l.token)live=true;if(live)try{l.fn(l.user,&snapshot);}catch(...){api->log(mod,OPENRAC_LOG_WARN,"A Space Voyages listener threw an exception");}}}
 void snapshot_visual(){
  previous_visual=voyage.presentation_flight();previous_entry=voyage.space_entry_progress();previous_approach=voyage.approach_progress();
  previous_visual_phase=voyage.phase();previous_scene_seconds=voyage.scene_seconds();previous_damage=voyage.ship_damage();
 }
 void audio_tick(float seconds){
  if(!audio_enabled)return;
  if(!audio_started){
   audio_started=true;
   if(!audio.initialize(assets.audio,audio_sdk.backend()))throw std::runtime_error("Failed to acquire transient audio resources");
   log("Scene audio initialized in OpenRAC mixer: original Gemlik music, engines and combat cues");
  }
  SceneAudioFrame frame;frame.seconds=seconds;frame.scene_seconds=voyage.scene_seconds();frame.ship_class=ship;frame.identity=identity;
  frame.boost_fraction=voyage.flight().boost_fraction;frame.paused=paused;frame.free_flight=voyage.phase()==Phase::FreeFlight;
  frame.damage=voyage.ship_damage();frame.combat=&voyage.combat();audio.tick(frame);
  if(audio.stats().failed)throw std::runtime_error("Transient audio backend failed during space travel");
 }
 void audio_log()const{
  if(!audio_started)return;
  const auto& s=audio.stats();openrac_transient_audio_stats mixed{};mixed.size=sizeof mixed;api->transient_audio_stats(mod,&mixed);
  char message[640];std::snprintf(message,sizeof message,"scene audio: active=%d paused=%d clips=%u voices=%u music=%llu engines=%llu turbo=%llu lasers=%llu impacts=%llu rocks=%llu hits=%llu deaths=%llu respawns=%llu dropped=%llu mixed-frames=%llu nonzero-frames=%llu pre-global-peak=%.6f",int(s.active),int(s.paused),mixed.clips,mixed.voices,static_cast<unsigned long long>(s.music_starts),static_cast<unsigned long long>(s.engine_starts),static_cast<unsigned long long>(s.turbo_starts),static_cast<unsigned long long>(s.laser_events),static_cast<unsigned long long>(s.impact_events),static_cast<unsigned long long>(s.rock_destructions),static_cast<unsigned long long>(s.ship_hits),static_cast<unsigned long long>(s.ship_destructions),static_cast<unsigned long long>(s.respawns),static_cast<unsigned long long>(s.dropped_one_shots),static_cast<unsigned long long>(mixed.rendered_frames),static_cast<unsigned long long>(mixed.nonzero_frames),mixed.peak);log(message);
 }
 void closing(){audio_log();audio.close();renderer.close();log("Transient audio and graphics resources released before native world resumes");}
 void frame(){
  if(in_scene)return;const int current=api->loaded_level();
  if(armed&&current!=origin){
   armed=false;origin=entered=-1;arrival_presented=false;
   boarding_exterior_sought=boarding_tail_finished=false;destination.cancel();entered_route={};voyage.cancel();event();
   log("Departure interrupted by a level change; native control retained");
  }
  if(current<0||current>18)return;++frames;
  const auto shared=shared_ship();const int scene=read<int>(shared+0x20),mode=read<int>(address(binding.cinematic_mode));
  const bool handoff_menu=mode==3&&scene==0&&read<int>(address(binding.menu))==14;
  if(awaiting_fleet&&(current!=awaiting_fleet_origin||scene!=0||(mode!=6&&!handoff_menu))){awaiting_fleet=false;awaiting_fleet_origin=-1;}
  if(native_only_departure&&(current!=native_only_origin||mode!=6)){native_only_departure=false;native_only_origin=-1;}
  if(voyage.phase()==Phase::Arrival&&current==entered&&mode==0){voyage.arrival_complete();event();log("Native landing completed; campaign control restored");}
  // State0 is the existing boarding cinematic; state8 is an arrival and must
  // never arm another departure. Leave first-flight story routes untouched.
  if(!armed&&!native_only_departure&&origin_supported(current)&&scene==0&&(mode==6||(awaiting_fleet&&handoff_menu))&&ordinary_departure_allowed(current,native_coordinates(),native_visits())){
   if(!timing_supported()||!native_departure_fade())return;
   garage.bind_loaded();DestinationRoute from;bool waiting_provider=false;
   if(!refresh_routes(&from,&waiting_provider)||from.native_level!=current){
    if(waiting_provider&&mode==6){
     if(!awaiting_fleet){awaiting_fleet=true;awaiting_fleet_origin=current;boarding_exterior_sought=boarding_tail_finished=false;log("Boarding awaits verified garage identity before the native handoff");}
     skip_boarding_console(current,mode,scene);return;
    }
    retain_native_departure(current,"Logical origin is unavailable; original travel retained");return;
   }
   const auto available=coordinates();
   const auto* profile=boarding_profile();
   if(!profile){
    retain_native_departure(current,"This ship has no compatible free-flight appearance; original travel retained");return;
   }
   if(voyage.begin(from,available,profile->identity)){
    armed=true;origin=current;ship=profile->native_carrier_class;identity=profile->identity;
    if(!awaiting_fleet)boarding_exterior_sought=boarding_tail_finished=false;
    awaiting_fleet=false;awaiting_fleet_origin=-1;
    if(trace){char appearance[192]{};std::snprintf(appearance,sizeof appearance,"Boarding identity captured: stable-id=%u carrier=%d appearance=%s",unsigned(identity),ship,profile->asset_id);log(appearance);}
    log("Boarding detected; preparing free flight after the original ship entry");event();
   }
  }
  skip_boarding_console(current,mode,scene);
  if(armed&&read<int>(address(binding.menu))==14){
   DestinationRoute confirmed_origin;const bool verified_origin=refresh_routes(&confirmed_origin);
   const auto* confirmed_profile=boarding_profile();
   if(!verified_origin||confirmed_origin!=voyage.origin_route()||!confirmed_profile||confirmed_profile->identity!=identity||confirmed_profile->native_carrier_class!=ship){
    retain_native_departure(current,"Boarding identity or logical origin changed before the native handoff; original travel retained");return;
   }
   const auto available=coordinates();const int staging=provisional_destination(available);
   if(staging>=0){
    // Same native departure initializer as the map's confirmation. Its target
    // is provisional until the transition hook, before any destination loads.
    const auto prepare=reinterpret_cast<void(*)(int)>(api->function(binding.prepare_target));
    if(prepare){
     if(destination.supported()&&!destination.begin_departure(voyage.origin_route())){
      if(trace){char context[192]{};std::snprintf(context,sizeof context,"Destination departure native context: mode=%d ship-state=%d menu=%d",read<int>(address(binding.cinematic_mode)),read<int>(shared_ship()+0x20),read<int>(address(binding.menu)));log(context);}
      retain_native_departure(current,"Destination handoff could not retain the origin; native ship menu retained");return;
     }
     write<int>(address(binding.menu)+0xc,1);write<int>(address(binding.menu),0);FlagScope preparing(preparing_departure);prepare(staging);log("Native map bypassed; original departure running");
    }
   }
  }
  if(trace&&frames%120==0){char message[180];std::snprintf(message,sizeof message,"world level=%d mode=%d ship-state=%d menu=%d armed=%d",current,mode,scene,read<int>(address(binding.menu)),int(armed));log(message);}
 }
 int tick(float seconds,unsigned short held,const unsigned char analog[4]){
  if(trace){const auto now=std::chrono::steady_clock::now();if(previous_tick.time_since_epoch().count()){const double ms=std::chrono::duration<double,std::milli>(now-previous_tick).count();frame_ms+=ms;frame_max_ms=std::max(frame_max_ms,ms);++timed_frames;}previous_tick=now;}
  if(!error.empty()){entered=-1;return 0;}
  if(trace&&diagnostic_scene.cancel_tick()){
   entered=-1;entered_route={};arrival_presented=false;
   char message[180]{};std::snprintf(message,sizeof message,"QA scene lease cancellation: trip=%u tick=%u; no destination selected",diagnostic_scene.scene_count(),diagnostic_scene.tick_count());log(message);
   return 0;
  }
  if(entered>=0){
   if(!refresh_routes()){error="Logical destination context was lost";return 0;}
   voyage.set_coordinates(coordinates());
   // Several catch-up ticks can precede one draw. Releasing on the next tick
   // alone could discard the final fade before it was ever presented.
   if(selected_route_valid())return arrival_presented?0:1;
   destination.discard_prepared();entered=-1;entered_route={};arrival_presented=false;voyage.cancel_arrival_to_flight();event();log("Destination revoked after final presentation; free flight resumed");
  }
  ++scene_ticks;const bool start=(held&8)!=0;if(scene_ticks>1&&start&&!last_start)paused=!paused;last_start=start;
  const auto vertical=direct_vertical?VerticalInput::Direct:VerticalInput::NativeAircraft;
  FlightInput input=flight_input_from_pad(analog,held,vertical,read<unsigned char>(address(binding.mirror))!=0);
  // QA supplies already-normalized flight commands after player-pad decoding.
  // Its controller-space input must not pass through the raw stick curve.
  bool triangle=(held&0x1000)!=0;diagnostics.apply(voyage,input,triangle);
  snapshot_visual();
  if(!refresh_routes()){error="Logical origin context changed during free flight";return 0;}
  const auto previous=voyage.phase();voyage.set_coordinates(coordinates());voyage.tick(seconds,input,triangle,paused,weapon_fire_from_pad(held));if(previous!=voyage.phase())event();
  if(trace&&previous==Phase::FreeFlight&&voyage.phase()==Phase::Approach){const auto& shown=renderer.last_stats();char message[200];std::snprintf(message,sizeof message,"entry prompt handoff: destination=%d last-drawn-candidate=%d glyphs=%u font=%u",voyage.target_level(),shown.prompt_candidate,shown.prompt_glyphs,shown.font_texture);log(message);}
  audio_tick(seconds);if(trace&&scene_ticks%120==0)audio_log();
  const auto& hull=voyage.ship_damage();
  if(hull.respawns!=previous_damage.respawns){snapshot_visual();log("Ship restored at safe departure pose; cinematic space entry resumes without loading a planet");}
  if(trace&&hull.damage_events!=previous_damage.damage_events){char message[180];std::snprintf(message,sizeof message,"ship impact: normal-speed=%.3f hull=%d/%d events=%u",hull.last_impact_speed,hull.hull,hull.max_hull,hull.damage_events);log(message);}
  // Arrival listeners may revoke coordinates. Validate their final state before
  // yielding the scene, so no provisional native destination can leak through.
  if(voyage.phase()==Phase::Arrival){
   voyage.set_coordinates(coordinates());
   if(voyage.resolved_route(voyage.target_level())!=voyage.target_route()){voyage.cancel_arrival_to_flight();event();log("Destination revoked at handoff; free flight resumed");}
  }
  if(destination.supported()){
   if(voyage.phase()==Phase::Approach||voyage.phase()==Phase::Arrival){
    if(destination.prepared_route()!=voyage.target_route()){
     destination.discard_prepared();
     if(!destination.prepare(voyage.target_route())){error="Destination could not be prepared; returning to the logical origin";return 0;}
    }
   }else if(destination.prepared_route().valid())destination.discard_prepared();
  }
  if(trace&&scene_ticks%120==0){const auto& f=voyage.flight();char message[384];std::snprintf(message,sizeof message,"flight tick=%u phase=%u position=(%.2f,%.2f,%.2f) speed=%.2f candidate=%d unlocked=%d input=(%.3f,%.3f) yaw=%.3f pitch=%.3f model-roll=%.3f camera-roll=%.3f",scene_ticks,unsigned(voyage.phase()),f.position.x,f.position.y,f.position.z,f.speed,voyage.candidate()?voyage.candidate()->level:-1,int(voyage.candidate_unlocked()),input.horizontal,input.vertical,f.yaw,f.pitch,f.model_roll,f.camera.roll);log(message);}
  if(trace&&scene_ticks%120==0){const auto& stats=renderer.last_stats();char message[280];std::snprintf(message,sizeof message,"scene timing: frames=%u mean-ms=%.3f max-ms=%.3f cpu-draw-ms=%.3f cpu-draw-max-ms=%.3f draws=%u artificial-draws=%u triangles=%u",timed_frames,timed_frames?frame_ms/timed_frames:0.,frame_max_ms,timed_draws?draw_ms/timed_draws:0.,draw_max_ms,stats.draws,stats.artificial_draws,stats.triangles);log(message);frame_ms=frame_max_ms=draw_ms=draw_max_ms=0;timed_frames=timed_draws=0;}
  if(trace&&scene_ticks%120==0&&voyage.asteroid_layout()){const auto& stats=renderer.last_stats();const auto& contact=voyage.asteroid_contacts();char message[260];std::snprintf(message,sizeof message,"asteroids: count=%zu contacts=%llu camera-obstructed=%d camera-lateral=%d boom=%.4f draws=%u instances=%u culled-batches=%u",voyage.asteroid_layout()->asteroids().size(),static_cast<unsigned long long>(contact.total_contacts),int(contact.camera_occluded),int(contact.camera_lateral),contact.camera_fraction,stats.asteroid_draws,stats.asteroid_instances,stats.asteroid_culled_batches);log(message);}
  if(trace&&scene_ticks%120==0){const auto& stats=renderer.last_stats();char message[280];std::snprintf(message,sizeof message,"ship state: hull=%d hud=%.3f damaged=%u destroyed=%d progress=%.4f respawns=%u intro=%d thrusters=%u effect-draws=%u effect-sprites=%u boost=%.4f",hull.hull,hull.hud_hull,hull.damage_events,int(hull.destroyed),hull.destruction_progress,hull.respawns,int(hull.respawn_intro),stats.thrusters,stats.exhaust_draws,stats.effect_sprites,voyage.flight().boost_fraction);log(message);}
  if(trace&&scene_ticks%120==0){const auto& combat=voyage.combat();const auto& stats=renderer.last_stats();char message[280];std::snprintf(message,sizeof message,"space combat: ship=%d armed=%d fire-held=%d shots=%llu destroyed=%llu mask-revision=%llu projectile-revision=%llu laser-draws=%u laser-instances=%u impact-sprites=%u",ship,int(combat.state().armed),int(weapon_fire_from_pad(held)),static_cast<unsigned long long>(combat.state().shots),static_cast<unsigned long long>(combat.state().destroyed),static_cast<unsigned long long>(combat.revision()),static_cast<unsigned long long>(combat.projectile_revision()),stats.projectile_draws,stats.projectiles,stats.impact_sprites);log(message);}
  // Present the completed approach once before release. The host retains this
  // final image while the native loader resumes, instead of the old planet.
  const auto selected_route=voyage.target_route();const int selected=voyage.take_destination();if(selected>=0){entered=selected;entered_route=selected_route;arrival_presented=false;log("Approach complete; presenting the final frame before the original loader");}return 1;
 }
 void draw(int w,int h,float alpha){
  const auto started=std::chrono::steady_clock::now();
  try{
   // Explicit diagnostic fixture: reproduce a blend equation left by the
   // native PS2 effects renderer. Player launchers clear every QA variable.
   if(diagnostic_subtractive_blend){
    const auto equation=reinterpret_cast<void(*)(unsigned)>(api->transient_gl_proc("glBlendEquation"));
    if(!equation)throw std::runtime_error("Inherited-state diagnostic requires glBlendEquation");
    equation(0x800b); // GL_FUNC_REVERSE_SUBTRACT
   }
   // A completed approach must really present its final black frame before
   // releasing the native loader, even if the display is between physics ticks.
   const float t=paused||voyage.phase()==Phase::Arrival||voyage.phase()!=previous_visual_phase?1.f:
       (std::isfinite(alpha)?std::clamp(alpha,0.f,1.f):1.f);
   const auto visual=presentation::interpolate(previous_visual,voyage.presentation_flight(),t);
   const float entry=previous_entry+(voyage.space_entry_progress()-previous_entry)*t;
   const float approach=previous_approach+(voyage.approach_progress()-previous_approach)*t;
   const float scene_seconds=float(previous_scene_seconds+(voyage.scene_seconds()-previous_scene_seconds)*t);
   auto damage=voyage.ship_damage();
   damage.hit_flash=previous_damage.hit_flash+(damage.hit_flash-previous_damage.hit_flash)*t;
   damage.hud_hull=previous_damage.hud_hull+(damage.hud_hull-previous_damage.hud_hull)*t;
   if(damage.destroyed&&previous_damage.destroyed)damage.destruction_progress=previous_damage.destruction_progress+(damage.destruction_progress-previous_damage.destruction_progress)*t;
   renderer.initialize(api->transient_gl_proc,voyage.asteroid_layout(),engine_flames);renderer.draw(voyage,visual,w,h,identity,paused,entry,approach,scene_seconds,&damage);
   if(trace){const auto& shown=renderer.last_stats();if(shown.prompt_candidate!=last_prompt_candidate||voyage.phase()!=last_prompt_phase){
    char message[260];std::snprintf(message,sizeof message,"HUD prompt draw: phase=%u simulation-candidate=%d submitted-candidate=%d glyphs=%u font=%u alpha=%.3f rect=(%.2f,%.2f,%.2f,16) viewport=(%d,%d)",unsigned(voyage.phase()),voyage.candidate()?voyage.candidate()->level:-1,shown.prompt_candidate,shown.prompt_glyphs,shown.font_texture,shown.prompt_alpha,shown.prompt_x,shown.prompt_y,shown.prompt_width,w,h);log(message);
    last_prompt_candidate=shown.prompt_candidate;last_prompt_phase=voyage.phase();
   }}
   // A catch-up burst must not skip the final black picture and interpolate
   // the old wreck across the galaxy. Only a successful draw may acknowledge.
   if(damage.destroyed&&damage.respawn_ready&&damage.destruction_progress>=1.f)voyage.acknowledge_destruction_frame();
   if(voyage.phase()==Phase::Arrival&&entered>=0)arrival_presented=true;
  }catch(const std::exception& e){error=e.what();}
  if(trace){const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();draw_ms+=ms;draw_max_ms=std::max(draw_max_ms,ms);++timed_draws;}
 }
 void fallback_transition(const char* reason){
  destination.cancel();armed=false;entered=-1;entered_route={};arrival_presented=false;
  // A provisional destination was only used to prepare take-off. An adapter
  // failure must not turn that staging value into an unintended journey.
  write<int>(address(binding.pending_destination),origin);voyage.cancel();event();log(reason);
  FlagScope preserve(preserving_ship);original();
 }
 void transition(){
  if(!armed||origin!=api->loaded_level()){original();return;}
  if(destination.supported()&&!destination.has_departure()){
   retain_native_departure(origin,"Departure did not pass through the owned destination handoff; original travel retained");
   original();return;
  }
  const auto fade=native_departure_fade();
  if(!timing_supported()||!fade){fallback_transition("Departure presentation unavailable; native return to origin retained");return;}
  if(!voyage.begin_space_entry()){fallback_transition("Departure state changed; native return to origin retained");return;}
  in_scene=true;paused=false;last_start=false;scene_ticks=0;entered=-1;entered_route={};arrival_presented=false;error.clear();audio_started=false;last_prompt_candidate=-2;last_prompt_phase=Phase::Ground;
  // The hook runs before the original transition's own fade. Finish the last
  // exterior picture through that same native helper, then acquire the scene
  // already at black. It pumps on_frame callbacks, so in_scene is set first.
  fade(12);
  log("Native departure faded to black before the free-flight scene");
  snapshot_visual();
  previous_tick={};frame_ms=frame_max_ms=draw_ms=draw_max_ms=0;timed_frames=timed_draws=0;
  openrac_transient_source callbacks{};callbacks.size=sizeof callbacks;callbacks.user=this;
  callbacks.tick=+[](void* ptr,float seconds,uint16_t held,const uint8_t analog[4])->int{auto& s=*static_cast<Session*>(ptr);try{return s.tick(seconds,held,analog);}catch(const std::exception& e){s.error=e.what();return 0;}};
  callbacks.draw=+[](void* ptr,int w,int h){static_cast<Session*>(ptr)->draw(w,h,1.f);};
  callbacks.closing=+[](void* ptr){static_cast<Session*>(ptr)->closing();};
  callbacks.draw_interpolated=+[](void* ptr,int w,int h,float alpha){static_cast<Session*>(ptr)->draw(w,h,alpha);};
  if(trace)diagnostic_scene.begin_scene();
  log("Entering isolated free-flight scene; campaign world suspended");const int result=api->transient_run(mod,&callbacks);in_scene=false;armed=false;
  if(!error.empty())api->log(mod,OPENRAC_LOG_ERROR,error.c_str());
  const bool logical_origin_valid=refresh_routes();
  voyage.set_coordinates(coordinates());
  const bool chosen=result==OPENRAC_HOOK_OK&&error.empty()&&arrival_presented&&logical_origin_valid&&entered>=0&&selected_route_valid();
  const bool committed=chosen&&(!destination.supported()||
      (destination.prepared_route()==entered_route&&destination.commit()));
  if(committed){write<int>(address(binding.pending_destination),entered);diagnostics.completed();}
  else {destination.cancel();write<int>(address(binding.pending_destination),origin);voyage.cancel();entered_route={};event();log("Transient flight unavailable or cancelled; returning to the logical departure world");}
  {FlagScope preserve(preserving_ship);original();}
  destination.cancel();
 }
};
std::unique_ptr<Session> session;
void transition(){if(session)session->transition();else original();}
void initialize_runtime(){
 // Native travel normally infers a ship from destination/progression flags.
 // The free-flight journey retains the physical ship that was boarded. Set
 // the variant before archives/textures/gameplay are selected, not afterwards.
 if(session&&session->preserving_ship){session->write<int16_t>(session->shared_ship()+disc::ship_variant_offset,int16_t(session->ship-531));session->log("Departure ship retained for native flight and landing");}
 original_initialize();
}
int save_data(int slot,int flags){
 // The native departure helper saves with its provisional target. Preserve its
 // ordinary checkpoint/slot machinery, but serialize the current planet until
 // the player actually chooses a destination in free flight.
 if(session&&session->preparing_departure&&flags>=0){flags=-1;session->log("Native departure checkpoint kept on the origin planet");}
 return original_save(slot,flags);
}
int get_state(space_voyages_state_v1* out){if(!session||!out||out->size<sizeof(*out))return 0;*out=session->state();return 1;}
int subscribe(space_voyages_listener_v1 fn,void* user,unsigned* token){if(!session||!fn||!token)return 0;for(auto& l:session->listeners)if(!l.fn){unsigned id=session->next_token++;if(!id)id=session->next_token++;l={fn,user,id};*token=id;return 1;}return 0;}
int unsubscribe(unsigned token){if(!session||!token)return 0;for(auto& l:session->listeners)if(l.token==token){l={};return 1;}return 0;}
const space_voyages_api_v1 service{sizeof(service),1,get_state,subscribe,unsubscribe};
}
extern "C" {OPENRAC_MOD_DEFINE("space-voyages","0.8.0")}
extern "C" OPENRAC_MOD_EXPORT const space_voyages_api_v1* openrac_space_voyages_query(unsigned version){return session&&version==1?&service:nullptr;}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api,openrac_mod* mod){
 try{
  if(!api||api->api_major!=1||api->api_minor<13||api->size<offsetof(openrac_api,transient_audio_stats)+sizeof(api->transient_audio_stats)||!api->transient_run||!api->transient_gl_proc||!api->transient_audio_create||!api->transient_audio_release||!api->transient_audio_play||!api->transient_audio_set||!api->transient_audio_stop||!api->transient_audio_pause||!api->transient_audio_playing||!api->transient_audio_stats)throw std::runtime_error("Space Voyages requires the OpenRAC 0.10.0 transient-scene audio bridge (SDK interface 1.13)");
  if(!api->memory||!api->relocate_data||!api->relocate_level_data||!api->function)throw std::runtime_error("Native memory relocation services unavailable");
  const auto* binding=disc::select(api->game,api->version);if(!binding)throw std::runtime_error("Space Voyages supports the canonical R&C1 PAL and NTSC-U programs");
  const char* configured=api->setting_string(mod,"assets_directory");const auto root=configured&&configured[0]?std::filesystem::path(configured):library_assets();
  auto data=SceneAssets::read(root);session=std::make_unique<Session>(api,mod,*binding,std::move(data));
  if(api->hook(mod,"rac1.space.transition","void()",reinterpret_cast<void*>(&transition),reinterpret_cast<void**>(&original))!=OPENRAC_HOOK_OK)throw std::runtime_error("Native space transition hook unavailable");
  if(api->hook(mod,"rac1.memcard.save_data","i32(i32,i32)",reinterpret_cast<void*>(&save_data),reinterpret_cast<void**>(&original_save))!=OPENRAC_HOOK_OK)throw std::runtime_error("Native departure checkpoint hook unavailable");
  if(api->hook(mod,"rac1.level.initialize_runtime","void()",reinterpret_cast<void*>(&initialize_runtime),reinterpret_cast<void**>(&original_initialize))!=OPENRAC_HOOK_OK)throw std::runtime_error("Native ship resource selection hook unavailable");
  if(api->on_frame(mod,+[](void*){if(session)session->frame();},nullptr))throw std::runtime_error("Frame registration failed");
  {char message[128];std::snprintf(message,sizeof message,"Native disc binding selected: %s",binding->name);session->log(message);}
  session->log(session->assets.full_galaxy?"Space Voyages: 18 destinations, native Gemlik flight, scene-local combat and original game audio":"Space Voyages prototype: Novalis/Kerwan/Eudora, native Gemlik flight");return 0;
 }catch(const std::exception& e){api->fail(mod,e.what());session.reset();return 1;}
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload(){session.reset();}

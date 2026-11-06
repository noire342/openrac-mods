// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_audio.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <map>
#include <set>
#include <string>
using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"native weapon audio: %s\n",why);std::exit(1);}}
struct Fixture;Fixture* live;
struct Fixture {
 openrac_api api{};openrac_mod* mod=reinterpret_cast<openrac_mod*>(0x1234);
 bool output=true,missing_attack=false;unsigned loads{},plays{},sets{},stops{},next_voice=1;
 std::map<std::string,int> clips;std::set<unsigned> voices;
 Fixture(){
  live=this;api.size=sizeof api;api.api_major=1;api.api_minor=12;
  api.sound_load=+[](openrac_mod* mod,const char* path){
   require(mod==live->mod&&path&&std::strstr(path,"assets/audio/")==path&&std::strstr(path,"..") == nullptr,"load only packaged relative authored audio for this mod");++live->loads;
   if(!live->output||(live->missing_attack&&!std::strcmp(path,"assets/audio/attack.wav")))return -1;
   const auto found=live->clips.find(path);if(found!=live->clips.end())return found->second;
   const int id=int(live->clips.size());live->clips[path]=id;return id;
  };
  api.sound_play=+[](openrac_mod* mod,int sound,float gain,float pitch,int loop){
   require(mod==live->mod&&sound>=0&&std::isfinite(gain)&&gain>=0&&gain<=1&&std::isfinite(pitch)&&pitch>=.5f&&pitch<=1.5f&&(loop==0||loop==1),"public mixer receives a loaded clip and valid finite parameters");
   ++live->plays;const auto voice=live->next_voice++;live->voices.insert(voice);return voice;
  };
  api.sound_set=+[](openrac_mod* mod,unsigned voice,float gain,float pitch){
   require(mod==live->mod&&std::isfinite(gain)&&gain>=0&&gain<=1&&std::isfinite(pitch)&&pitch>=.5f&&pitch<=1.5f,"native volume and pitch updates remain valid");++live->sets;
   return live->voices.count(voice)?OPENRAC_HOOK_OK:OPENRAC_HOOK_UNKNOWN;
  };
  api.sound_stop=+[](openrac_mod* mod,unsigned voice){require(mod==live->mod,"stop is scoped to the weapon's mod handle");++live->stops;live->voices.erase(voice);};
 }
};
WeaponAudioFrame frame(){WeaponAudioFrame f;f.playing=f.alive=f.equipped=true;f.ammunition=20;f.rotor_speed=1;return f;}
}
int main(){
 {Fixture f;NativeWeaponAudio audio(&f.api,f.mod);require(f.loads==0,"constructor waits until the first real audio event");
  require(audio.emit(WeaponAudioCue::Attack),"clip id zero is a valid loaded attack sound");audio.update(frame());
  require(f.loads==weapon_audio_cue_count-4&&f.plays==2&&f.clips.count("assets/audio/charge_ready.wav"),"fourteen custom cues load directly, then attack and idle play");
  for(const auto cue:{WeaponAudioCue::Equip,WeaponAudioCue::LevelUp,WeaponAudioCue::DryFire,WeaponAudioCue::AmmoPickup})require(!audio.emit(cue)&&!audio.available(cue),"excluded native actions are unavailable as custom audio");
  require(!f.clips.count("assets/audio/equip.wav")&&!f.clips.count("assets/audio/xp_level_up.wav")&&!f.clips.count("assets/audio/dry_fire.wav")&&!f.clips.count("assets/audio/ammo_pickup.wav"),"excluded actions never request a custom file");
  const auto loaded=f.loads;audio.reset();require(f.voices.empty(),"reset stops every owned native voice");audio.update(frame());require(f.loads==loaded&&f.plays==3,"level reset reuses loaded clips without replaying equip");
  auto state=frame();state.paused=true;audio.update(state);require(f.voices.empty(),"pause stops native voices");}
 for(unsigned mode=0;mode<4;++mode){Fixture f;if(mode==0)f.api.size=unsigned(offsetof(openrac_api,sound_stop));if(mode==1)f.api.api_minor=11;if(mode==2)f.api.api_major=2;if(mode==3)f.api.sound_set=nullptr;
  NativeWeaponAudio audio(&f.api,f.mod);require(!audio.emit(WeaponAudioCue::Attack),"unavailable sound API leaves the fallback available");audio.update(frame());require(f.loads==0&&f.plays==0,"invalid API capability is never dereferenced");}
 {Fixture f;f.api.api_minor=13;f.missing_attack=true;NativeWeaponAudio audio(&f.api,f.mod);require(!audio.emit(WeaponAudioCue::Attack),"missing custom attack clip reports false");audio.update(frame());require(f.plays==1,"compatible future API still plays available motor audio");}
 {Fixture f;f.output=false;NativeWeaponAudio audio(&f.api,f.mod);require(!audio.emit(WeaponAudioCue::Attack),"a missing audio output is a quiet fallback");
  for(unsigned i=0;i<30;++i)audio.update(frame());require(f.plays==0&&f.loads==weapon_audio_cue_count-4,"failed initial loads are not repeated every frame");
  f.output=true;for(unsigned i=0;i<45;++i)audio.update(frame());require(f.plays==1&&audio.available(WeaponAudioCue::Attack),"late sound-output initialization resumes the current motor without replaying attack");}
 std::printf("native weapon audio: %u API, authored-path, fallback, future-interface and lifecycle checks passed\n",checks);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_audio.hpp"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
namespace revolverator {
namespace {
constexpr std::array<const char*,weapon_audio_cue_count> paths{{
 nullptr,"assets/audio/attack.wav","assets/audio/capture.wav","assets/audio/charge_ready.wav",
 "assets/audio/throw.wav","assets/audio/early_release.wav","assets/audio/ground_contact.wav","assets/audio/heavy_hit.wav",
 "assets/audio/projectile_impact.wav","assets/audio/gold_explosion_low.wav","assets/audio/gold_explosion_full.wav",
 nullptr,nullptr,"assets/audio/gold_acquisition.wav",nullptr,
 "assets/audio/idle_loop.wav","assets/audio/spin_loop.wav","assets/audio/spin_fast_loop.wav"
}};
bool trace(const openrac_api* api,openrac_mod* mod){return api&&api->setting_bool&&api->log&&api->setting_bool(mod,"trace")!=0;}
}
NativeWeaponAudio::NativeWeaponAudio(const openrac_api* api,openrac_mod* mod):api_(api),mod_(mod),policy_(*this){
 clips_.fill(-1);
 constexpr auto required=offsetof(openrac_api,sound_stop)+sizeof(openrac_api{}.sound_stop);
 supported_=api&&api->size>=required&&api->api_major==1&&api->api_minor>=12&&api->sound_load&&api->sound_play&&api->sound_set&&api->sound_stop;
}
NativeWeaponAudio::~NativeWeaponAudio(){reset();}
void NativeWeaponAudio::load(){
 if(!supported_)return;
 for(unsigned i=0;i<clips_.size();++i)if(paths[i]&&clips_[i]<0){
  clips_[i]=api_->sound_load(mod_,paths[i]);
  if(trace(api_,mod_)){char text[256];std::snprintf(text,sizeof text,"WEAPON_AUDIO {\"event\":\"load\",\"path\":\"%s\",\"sound\":%d}",paths[i],clips_[i]);api_->log(mod_,OPENRAC_LOG_INFO,text);}
 }
 retry_seconds_=1.f;
}
bool NativeWeaponAudio::available(WeaponAudioCue cue)const{return supported_&&unsigned(cue)<clips_.size()&&paths[unsigned(cue)]&&clips_[unsigned(cue)]>=0;}
bool NativeWeaponAudio::emit(WeaponAudioCue cue,float intensity){if(retry_seconds_<=0)load();return policy_.emit(cue,intensity);}
void NativeWeaponAudio::update(const WeaponAudioFrame& frame){
 if(std::isfinite(frame.seconds)&&frame.seconds>0&&frame.seconds<=.1f)retry_seconds_=std::max(0.f,retry_seconds_-frame.seconds);
 if(retry_seconds_<=0&&frame.playing&&!frame.paused&&frame.alive)load();
 policy_.update(frame);
}
void NativeWeaponAudio::reset(){policy_.reset();}
WeaponAudioPlayback::Voice NativeWeaponAudio::play(WeaponAudioCue cue,float gain,float pitch,bool loop){
 if(!available(cue))return 0;
 const auto voice=api_->sound_play(mod_,clips_[unsigned(cue)],std::clamp(gain,0.f,1.f),std::clamp(pitch,.5f,1.5f),loop?1:0);
 if(voice&&trace(api_,mod_)){char text[320];std::snprintf(text,sizeof text,"WEAPON_AUDIO {\"event\":\"play\",\"cue\":\"%s\",\"sound\":%d,\"voice\":%u,\"loop\":%s,\"gain\":%.5f,\"pitch\":%.5f}",paths[unsigned(cue)],clips_[unsigned(cue)],voice,loop?"true":"false",gain,pitch);api_->log(mod_,OPENRAC_LOG_INFO,text);}
 return voice;
}
bool NativeWeaponAudio::set(Voice voice,float gain,float pitch){return supported_&&api_->sound_set(mod_,voice,std::clamp(gain,0.f,1.f),std::clamp(pitch,.5f,1.5f))==OPENRAC_HOOK_OK;}
void NativeWeaponAudio::stop(Voice voice){if(supported_&&voice){api_->sound_stop(mod_,voice);
 if(trace(api_,mod_)){char text[120];std::snprintf(text,sizeof text,"WEAPON_AUDIO {\"event\":\"stop\",\"voice\":%u}",voice);api_->log(mod_,OPENRAC_LOG_INFO,text);}
}}
}

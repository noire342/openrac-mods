// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "weapon_audio.hpp"
#include <algorithm>
#include <cmath>
namespace revolverator {
namespace {
bool is_loop(WeaponAudioCue cue){return cue==WeaponAudioCue::IdleLoop||cue==WeaponAudioCue::SpinLoop||cue==WeaponAudioCue::SpinFastLoop;}
bool world_sound(WeaponAudioCue cue){return cue==WeaponAudioCue::Impact||cue==WeaponAudioCue::GoldenImpact||cue==WeaponAudioCue::GoldenImpactFull;}
float volume(WeaponAudioCue cue){
 switch(cue){
 case WeaponAudioCue::Ready:return .58f;
 case WeaponAudioCue::DryFire:case WeaponAudioCue::Drop:return .7f;
 case WeaponAudioCue::GroundContact:case WeaponAudioCue::AmmoPickup:return .6f;
 case WeaponAudioCue::GoldenPurchase:case WeaponAudioCue::LevelUp:return .7f;
 default:return .8f;
 }
}
}
bool WeaponAudio::emit(WeaponAudioCue cue,float intensity){
 // These actions retain their existing game audio exclusively.
 if(cue==WeaponAudioCue::Equip||cue==WeaponAudioCue::LevelUp||cue==WeaponAudioCue::DryFire||cue==WeaponAudioCue::AmmoPickup)return false;
 if(unsigned(cue)>=weapon_audio_cue_count||is_loop(cue)||!std::isfinite(intensity)||intensity<=0||pending_size_==pending_.size())return false;
 intensity=std::clamp(intensity,0.f,1.f);
 if(cue==WeaponAudioCue::GoldenImpact&&intensity>=.65f)cue=WeaponAudioCue::GoldenImpactFull;
 if(!playback_.available(cue))return false;
 pending_[pending_size_++]={cue,intensity};return true;
}
void WeaponAudio::stop(Voice& voice){if(voice.id)playback_.stop(voice.id);voice={};}
void WeaponAudio::stop_loops(){for(auto& voice:loops_)stop(voice);}
void WeaponAudio::reset(){for(auto& voice:shots_)stop(voice);stop_loops();pending_size_=0;live_=spinning_=ready_=false;}
void WeaponAudio::one_shot(Event event){
 auto slot=std::find_if(shots_.begin(),shots_.end(),[](const Voice& voice){return !voice.id;});
 if(slot==shots_.end()){slot=std::min_element(shots_.begin(),shots_.end(),[](const Voice& a,const Voice& b){return a.age<b.age;});stop(*slot);}
 const float gain=volume(event.cue)*event.intensity,pitch=1.f;
 const auto id=playback_.play(event.cue,gain,pitch,false);
 if(id)*slot={id,event.cue,gain,pitch,++age_};
}
void WeaponAudio::loop(WeaponAudioCue cue,float gain,float pitch,float seconds){
 auto wanted=std::find_if(loops_.begin(),loops_.end(),[&](const Voice& voice){return voice.id&&voice.cue==cue;});
 if(wanted==loops_.end()&&playback_.available(cue)){
  wanted=std::find_if(loops_.begin(),loops_.end(),[](const Voice& voice){return !voice.id;});
  if(wanted==loops_.end()){wanted=std::min_element(loops_.begin(),loops_.end(),[](const Voice& a,const Voice& b){return a.gain<b.gain;});stop(*wanted);}
  const auto id=playback_.play(cue,0.f,pitch,true);if(id)*wanted={id,cue,0.f,pitch,++age_};
 }
 for(auto& voice:loops_){
  if(!voice.id)continue;
  const bool active=voice.cue==cue;
  const float target=active?gain:0.f;
  voice.gain+=std::clamp(target-voice.gain,-seconds*6.f,seconds*6.f);
  if(active)voice.pitch=pitch;
  if(voice.gain<=.001f&&!active){stop(voice);continue;}
  if(!playback_.set(voice.id,voice.gain,voice.pitch))voice={};
 }
}
void WeaponAudio::update(const WeaponAudioFrame& frame){
 const bool valid=std::isfinite(frame.seconds)&&frame.seconds>0&&frame.seconds<=.1f&&std::isfinite(frame.charge)&&std::isfinite(frame.rotor_speed);
 if(!valid||!frame.playing||frame.paused||!frame.alive){reset();return;}
 for(auto& voice:shots_)if(voice.id&&!playback_.set(voice.id,voice.gain,voice.pitch))voice={};
 const bool spin=frame.equipped&&frame.phase==Phase::Spinning;
 const float charge=std::clamp(frame.charge,0.f,1.f);
 // Resuming a menu/pause selects the current motor sound without replaying
 // capture, equip, or a ready transition that happened before the pause.
 if(spin&&charge>=1.f&&!ready_&&live_&&spinning_)emit(WeaponAudioCue::Ready);
 ready_=spin&&(ready_||charge>=1.f);spinning_=spin;live_=frame.equipped;
 for(unsigned i=0;i<pending_size_;++i)if(frame.equipped||world_sound(pending_[i].cue))one_shot(pending_[i]);
 pending_size_=0;
 if(!frame.equipped){stop_loops();for(auto& voice:shots_)if(!world_sound(voice.cue))stop(voice);return;}
 if(spin){
  const float rpm=std::clamp((std::abs(frame.rotor_speed)-24.f)/18.f,0.f,1.f);
  loop(charge>=.65f?WeaponAudioCue::SpinFastLoop:WeaponAudioCue::SpinLoop,charge>=.65f?.44f:.46f,.88f+.22f*charge+.12f*rpm,frame.seconds);
 }else if(frame.ammunition>0&&std::abs(frame.rotor_speed)>.001f&&frame.phase!=Phase::Windup)
  loop(WeaponAudioCue::IdleLoop,.16f,1.f,frame.seconds);
 else stop_loops();
}
}

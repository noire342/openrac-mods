// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openrac/openrac_mod.h>
#include "scene_audio.hpp"
namespace spacevoyages {
// The only translation layer between audio presentation and the host ABI.
// This object and its lease owner outlive every backend callback.
struct SceneAudioSdk {
 const openrac_api* api{};openrac_mod* mod{};
 static openrac_transient_audio_params parameters(const SceneAudioParams& p){
  openrac_transient_audio_params out{};out.size=sizeof out;
  out.flags=(p.loop?OPENRAC_TRANSIENT_AUDIO_LOOP:0u)|(p.paused?OPENRAC_TRANSIENT_AUDIO_PAUSED:0u);
  out.group=p.music?OPENRAC_TRANSIENT_AUDIO_MUSIC:OPENRAC_TRANSIENT_AUDIO_EFFECTS;
  out.gain=p.gain;out.pitch=p.pitch;out.pan=p.pan;return out;
 }
 SceneAudioBackend backend(){
  return {this,
   +[](void* user,const SceneAudioPcmView& p){auto& s=*static_cast<SceneAudioSdk*>(user);openrac_transient_audio_pcm out{};out.size=sizeof out;out.sample_rate=p.sample_rate;out.channels=p.channels;out.samples=p.samples;out.frames=p.frames;out.loop_begin_frame=p.loop_begin_frame;out.loop_end_frame=p.loop_end_frame;return s.api->transient_audio_create(s.mod,&out);},
   +[](void* user,std::uint32_t clip){auto& s=*static_cast<SceneAudioSdk*>(user);return int(s.api->transient_audio_release(s.mod,clip)==OPENRAC_HOOK_OK);},
   +[](void* user,std::uint32_t clip,const SceneAudioParams& p){auto& s=*static_cast<SceneAudioSdk*>(user);const auto out=parameters(p);return s.api->transient_audio_play(s.mod,clip,&out);},
   +[](void* user,std::uint32_t voice,const SceneAudioParams& p){auto& s=*static_cast<SceneAudioSdk*>(user);const auto out=parameters(p);return int(s.api->transient_audio_set(s.mod,voice,&out)==OPENRAC_HOOK_OK);},
   +[](void* user,std::uint32_t voice){auto& s=*static_cast<SceneAudioSdk*>(user);return int(s.api->transient_audio_stop(s.mod,voice)==OPENRAC_HOOK_OK);},
   +[](void* user,bool paused){auto& s=*static_cast<SceneAudioSdk*>(user);return int(s.api->transient_audio_pause(s.mod,paused?1:0)==OPENRAC_HOOK_OK);},
   +[](void* user,std::uint32_t voice){auto& s=*static_cast<SceneAudioSdk*>(user);return s.api->transient_audio_playing(s.mod,voice);}
  };
 }
};
}

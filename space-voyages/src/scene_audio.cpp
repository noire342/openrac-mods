// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_audio.hpp"
#include <algorithm>
#include <cmath>

namespace spacevoyages {
namespace {
std::size_t index(SceneAudioClipId id){return static_cast<std::size_t>(id);}
bool range(float value,float low,float high){return std::isfinite(value)&&value>=low&&value<=high;}
bool valid(const SceneAudioAssets& assets){
    std::uint64_t bytes=0;
    for(const auto& clip:assets.clips){
        if(clip.samples.empty())continue;
        if(clip.sample_rate<8000||clip.sample_rate>96000||(clip.channels!=1&&clip.channels!=2)||
           clip.samples.size()%clip.channels||!range(clip.gain,0,4)||!range(clip.pitch,.25f,4))return false;
        const auto frames=clip.samples.size()/clip.channels;
        const auto end=clip.loop_end_frame?clip.loop_end_frame:frames;
        if(frames>std::uint64_t(clip.sample_rate)*600||clip.loop_begin_frame>=end||end>frames)return false;
        bytes+=clip.samples.size()*sizeof(std::int16_t);if(bytes>256u*1024u*1024u)return false;
    }
    for(const auto& s:assets.ships){
        if(index(s.engine)>scene_audio_clip_count||index(s.turbo)>scene_audio_clip_count||index(s.laser)>scene_audio_clip_count||
           !range(s.idle_gain,0,4)||!range(s.boost_gain,0,4)||!range(s.laser_gain,0,4)||
           !range(s.idle_pitch,.25f,4)||!range(s.boost_pitch,.25f,4)||
           !range(s.turbo_idle_pitch,.25f,4)||!range(s.turbo_boost_pitch,.25f,4))return false;
    }
    return true;
}
}
bool SceneAudio::initialize(const SceneAudioAssets& assets,SceneAudioBackend backend)noexcept {
    close();stats_={};last_shot_=last_event_=0;last_damage_=last_respawns_=0;last_destroyed_=false;ship_profile_=nullptr;boost_=0;last_scene_seconds_=-1.;
    if(!backend.create||!backend.release||!backend.play||!backend.set||!backend.stop||!backend.pause||!backend.playing||!valid(assets)){
        stats_.failed=true;return false;
    }
    assets_=&assets;backend_=backend;
    try{
        for(std::size_t i=0;i<assets.clips.size();++i){const auto& clip=assets.clips[i];if(clip.samples.empty())continue;
            const SceneAudioPcmView view{clip.sample_rate,clip.channels,clip.samples.data(),clip.samples.size()/clip.channels,clip.loop_begin_frame,clip.loop_end_frame};
            clips_[i]=backend_.create(backend_.user,view);
            if(!clips_[i]){stats_.failed=true;close();return false;}++stats_.clips;
        }
        stats_.active=stats_.clips!=0;return stats_.active;
    }catch(...){stats_.failed=true;close();return false;}
}
bool SceneAudio::has(SceneAudioClipId id)const noexcept{return index(id)<clips_.size()&&clips_[index(id)]!=0;}
SceneAudioParams SceneAudio::params(SceneAudioClipId id,bool looping,bool music,float gain,float pitch)const noexcept {
    const auto& clip=assets_->clips[index(id)];
    return {looping,stats_.paused,music,std::clamp(clip.gain*gain,0.f,4.f),std::clamp(clip.pitch*pitch,.25f,4.f),0};
}
void SceneAudio::stop(std::uint32_t& voice){if(voice){backend_.stop(backend_.user,voice);voice=0;}}
bool SceneAudio::loop(std::uint32_t& voice,SceneAudioClipId clip,bool music,float gain,float pitch){
    if(stats_.failed)return false;
    if(!has(clip)){stop(voice);return false;}
    if(voice&&!backend_.playing(backend_.user,voice))voice=0;
    if(voice){if(!backend_.set(backend_.user,voice,params(clip,true,music,gain,pitch)))stats_.failed=true;return false;}
    voice=backend_.play(backend_.user,clips_[index(clip)],params(clip,true,music,gain,pitch));
    if(!voice)stats_.failed=true;return voice!=0;
}
void SceneAudio::cue(SceneAudioClipId clip,float gain){
    if(stats_.failed||!has(clip))return;
    for(auto& voice:effects_)if(!voice){
        voice=backend_.play(backend_.user,clips_[index(clip)],params(clip,false,false,gain));
        if(voice)++stats_.one_shots_played;else stats_.failed=true;return;
    }
    ++stats_.dropped_one_shots;
}
void SceneAudio::voices(){
    for(auto& voice:effects_)if(voice&&!backend_.playing(backend_.user,voice))voice=0;
    stats_.voices=unsigned((music_!=0)+(engine_!=0)+(turbo_!=0));
    for(auto voice:effects_)if(voice)++stats_.voices;
    stats_.peak_voices=std::max(stats_.peak_voices,stats_.voices);
}
void SceneAudio::consume_combat(const SceneAudioFrame& frame,bool playable){
    if(!frame.combat)return;
    std::array<const SpaceProjectile*,SpaceCombat::projectile_capacity> shots{};std::size_t count=0;
    for(const auto& p:frame.combat->projectiles())if(p.id>last_shot_)shots[count++]=&p;
    std::sort(shots.begin(),shots.begin()+count,[](auto a,auto b){return a->id<b->id;});
    for(std::size_t i=0;i<count;++i){const auto& p=*shots[i];if(p.birth_seconds>frame.scene_seconds+1e-6)break;
        last_shot_=p.id;
        const auto identity=frame.identity?frame.identity:ship_identity_from_native_class(frame.ship_class);
        const auto* selected=identity?ship_profile(*identity):nullptr;
        if(playable&&p.lifetime>0&&selected&&selected->weapons_allowed){
            const auto& profile=assets_->ships[selected->slot];++stats_.laser_events;cue(profile.laser,profile.laser_gain);
        }
    }
    std::array<const SpaceCombatEvent*,SpaceCombat::event_capacity> events{};count=0;
    const auto available=std::min(frame.combat->event_count(),events.size());
    for(std::size_t i=0;i<available;++i){const auto& e=frame.combat->events()[i];if(e.id>last_event_)events[count++]=&e;}
    std::sort(events.begin(),events.begin()+count,[](auto a,auto b){return a->id<b->id;});
    for(std::size_t i=0;i<count;++i){const auto& e=*events[i];if(e.scene_seconds>frame.scene_seconds+1e-6)break;
        last_event_=e.id;if(!playable)continue;
        if(e.kind==SpaceCombatEventKind::Destroyed){++stats_.rock_destructions;cue(SceneAudioClipId::RockDestroyed);}
        else {++stats_.impact_events;cue(SceneAudioClipId::RockHit);}
    }
}
void SceneAudio::tick(const SceneAudioFrame& frame)noexcept {
    if(!stats_.active)return;
    if(!frame.scene_active){close();return;}
    try{
        if(frame.paused!=stats_.paused){
            if(!backend_.pause(backend_.user,frame.paused)){stats_.failed=true;close();return;}
            stats_.paused=frame.paused;
        }
        if(frame.paused)return;
        if(!std::isfinite(frame.seconds)||frame.seconds<=0||!std::isfinite(frame.scene_seconds)||!std::isfinite(frame.boost_fraction))return;
        if(frame.scene_seconds<0||frame.scene_seconds<=last_scene_seconds_)return;
        last_scene_seconds_=frame.scene_seconds;
        voices();
        if(loop(music_,SceneAudioClipId::Music,true,1,1))++stats_.music_starts;
        const bool destroyed=frame.damage.destroyed||frame.damage.hull<=0;
        const auto new_damage=frame.damage.damage_events>=last_damage_?frame.damage.damage_events-last_damage_:0;
        const bool death=destroyed&&!last_destroyed_;
        const auto hit_cues=new_damage-std::min<std::uint32_t>(new_damage,death?1u:0u);
        // At most29 effects can play simultaneously. Large externally injected
        // counters cannot turn one native tick into an unbounded cue loop.
        for(std::uint32_t i=0;i<std::min<std::uint32_t>(hit_cues,unsigned(effect_voice_capacity));++i)cue(SceneAudioClipId::ShipHit);
        stats_.ship_hits+=hit_cues;
        if(hit_cues>effect_voice_capacity)stats_.dropped_one_shots+=hit_cues-effect_voice_capacity;
        if(death){++stats_.ship_destructions;cue(SceneAudioClipId::ShipDestroyed);}
        if(frame.damage.respawns>last_respawns_){
            stats_.respawns+=frame.damage.respawns-last_respawns_;cue(SceneAudioClipId::Respawn);
            boost_=std::clamp(frame.boost_fraction,0.f,1.f);
        }
        last_damage_=frame.damage.damage_events;last_respawns_=frame.damage.respawns;last_destroyed_=destroyed;
        const auto identity=frame.identity?frame.identity:ship_identity_from_native_class(frame.ship_class);
        const auto* selected=identity?ship_profile(*identity):nullptr;
        if(ship_profile_!=selected){stop(engine_);stop(turbo_);ship_profile_=selected;}
        const float fraction=std::clamp(frame.boost_fraction,0.f,1.f);
        const float blend=1.f-std::exp(-std::min(frame.seconds,.25f)/.08f);
        if(destroyed)boost_=0;else boost_+=(fraction-boost_)*blend;
        if(destroyed||!selected){stop(engine_);stop(turbo_);}
        else {
            const auto& profile=assets_->ships[selected->slot];const bool turbo=has(profile.turbo);
            const float pitch=profile.idle_pitch+(profile.boost_pitch-profile.idle_pitch)*boost_;
            const float gain=turbo?profile.idle_gain*(1.f-boost_):profile.idle_gain+(profile.boost_gain-profile.idle_gain)*boost_;
            if(loop(engine_,profile.engine,false,gain,pitch))++stats_.engine_starts;
            if(turbo&&(fraction>0||boost_>.0001f)){
                const float turbo_pitch=profile.turbo_idle_pitch+(profile.turbo_boost_pitch-profile.turbo_idle_pitch)*boost_;
                if(loop(turbo_,profile.turbo,false,profile.boost_gain*boost_,turbo_pitch))++stats_.turbo_starts;
            }else stop(turbo_);
        }
        consume_combat(frame,frame.free_flight&&!destroyed);
        if(stats_.failed){close();return;}voices();
    }catch(...){stats_.failed=true;close();}
}
void SceneAudio::close()noexcept {
    if(backend_.stop&&(stats_.active||stats_.clips)){try{backend_.stop(backend_.user,0);}catch(...){} }
    for(auto& clip:clips_)if(clip){if(backend_.release){try{backend_.release(backend_.user,clip);}catch(...){}}clip=0;}
    effects_.fill(0);music_=engine_=turbo_=0;assets_=nullptr;backend_={};
    stats_.active=stats_.paused=false;stats_.voices=stats_.clips=0;
}
}

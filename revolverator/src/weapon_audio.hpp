// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "controller.hpp"
#include <array>
#include <cstdint>

namespace revolverator {
enum class WeaponAudioCue : unsigned {
 Equip,Attack,Capture,Ready,Throw,Drop,GroundContact,HeavyHit,Impact,GoldenImpact,GoldenImpactFull,
 DryFire,AmmoPickup,GoldenPurchase,LevelUp,IdleLoop,SpinLoop,SpinFastLoop,Count
};
inline constexpr unsigned weapon_audio_cue_count=unsigned(WeaponAudioCue::Count);
class WeaponAudioSink {
public:
 virtual ~WeaponAudioSink()=default;
 virtual bool emit(WeaponAudioCue,float intensity=1.f)=0;
};
struct WeaponAudioFrame {
 float seconds=1.f/60.f;
 bool playing{},paused{},alive{},equipped{};
 Phase phase=Phase::Idle;
 float charge{};unsigned ammunition{};float rotor_speed{};
};
class WeaponAudioPlayback {
public:
 using Voice=std::uint32_t;
 virtual ~WeaponAudioPlayback()=default;
 virtual bool available(WeaponAudioCue)const=0;
 virtual Voice play(WeaponAudioCue,float gain,float pitch,bool loop)=0;
 virtual bool set(Voice,float gain,float pitch)=0;
 virtual void stop(Voice)=0;
};
// Event/state policy only: no game memory, files, or SDK calls. One-shots are
// queued before the single post-controller update. At most six owned voices.
class WeaponAudio final : public WeaponAudioSink {
public:
 explicit WeaponAudio(WeaponAudioPlayback& playback):playback_(playback){}
 ~WeaponAudio(){reset();}
 bool emit(WeaponAudioCue,float intensity=1.f)override;
 void update(const WeaponAudioFrame&);
 void reset();
private:
 struct Event {WeaponAudioCue cue=WeaponAudioCue::Equip;float intensity=1;};
 struct Voice {WeaponAudioPlayback::Voice id{};WeaponAudioCue cue=WeaponAudioCue::Equip;float gain{},pitch=1;std::uint64_t age{};};
 WeaponAudioPlayback& playback_;
 std::array<Event,8> pending_{};unsigned pending_size_{};
 std::array<Voice,4> shots_{};std::array<Voice,2> loops_{};
 std::uint64_t age_{};bool live_{},spinning_{},ready_{};
 void stop(Voice&);
 void stop_loops();
 void one_shot(Event);
 void loop(WeaponAudioCue,float gain,float pitch,float seconds);
};
}

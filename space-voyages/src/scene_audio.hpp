// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ship_damage.hpp"
#include "space_combat.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spacevoyages {
enum class SceneAudioClipId : std::uint8_t {
    Music,Engine531,Engine532,Engine533,Laser532,Laser533,
    RockHit,RockDestroyed,ShipHit,ShipDestroyed,Respawn,
    Turbo531,Turbo532,Turbo533,Count
};
inline constexpr std::size_t scene_audio_clip_count=static_cast<std::size_t>(SceneAudioClipId::Count);
struct SceneAudioClip {
    std::vector<std::int16_t> samples; // owned decoded interleaved S16 PCM
    std::uint32_t sample_rate{},channels{};
    std::uint64_t loop_begin_frame{},loop_end_frame{}; // end exclusive;0=frames
    float gain=1.f,pitch=1.f;
};
struct SceneAudioShipDefinition {
    SceneAudioClipId engine=SceneAudioClipId::Engine531;
    SceneAudioClipId laser=SceneAudioClipId::Count;
    SceneAudioClipId turbo=SceneAudioClipId::Turbo531;
    // Audio presentation policy, not flight coefficients. Source-derived
    // profiles may override these neutral gains and gentle pitch modulation.
    float idle_gain=1.f,boost_gain=1.f,idle_pitch=1.f,boost_pitch=1.12f;
    float laser_gain=1.f,turbo_idle_pitch=1.f,turbo_boost_pitch=1.f;
};
struct SceneAudioAssets {
    std::array<SceneAudioClip,scene_audio_clip_count> clips;
    std::array<SceneAudioShipDefinition,ship_profile_count> ships{{
        {SceneAudioClipId::Engine531,SceneAudioClipId::Count,SceneAudioClipId::Turbo531},
        {SceneAudioClipId::Engine532,SceneAudioClipId::Laser532,SceneAudioClipId::Turbo532},
        {SceneAudioClipId::Engine533,SceneAudioClipId::Laser533,SceneAudioClipId::Turbo533},
        // Explicit fighter69 sound fallback, sharing existing PCM handles.
        {SceneAudioClipId::Engine531,SceneAudioClipId::Count,SceneAudioClipId::Turbo531}
    }};
};
// The Session's small SDK1.13 adapter supplies these callbacks. This module
// neither depends on an SDK binary nor opens an OS audio device/player.
struct SceneAudioPcmView {
    std::uint32_t sample_rate{},channels{};
    const std::int16_t* samples{};
    std::uint64_t frames{},loop_begin_frame{},loop_end_frame{};
};
struct SceneAudioParams {
    bool loop{},paused{},music{};
    float gain=1.f,pitch=1.f,pan{};
};
struct SceneAudioBackend {
    void* user{};
    std::uint32_t(*create)(void*,const SceneAudioPcmView&){};
    int(*release)(void*,std::uint32_t){};
    std::uint32_t(*play)(void*,std::uint32_t,const SceneAudioParams&){};
    int(*set)(void*,std::uint32_t,const SceneAudioParams&){};
    int(*stop)(void*,std::uint32_t){}; //0 stops all voices of this lease owner
    int(*pause)(void*,bool){}; // freeze music AND effects cursors
    int(*playing)(void*,std::uint32_t){};
};
struct SceneAudioFrame {
    float seconds=FlightController::native_step;
    double scene_seconds{};
    int ship_class=531;
    std::optional<ShipIdentity> identity; // explicit identity overrides legacy stock class
    float boost_fraction{};
    bool paused{},free_flight{},scene_active=true;
    ShipDamageState damage{};
    const SpaceCombat* combat{};
};
struct SceneAudioStats {
    bool active{},paused{},failed{};
    unsigned voices{},peak_voices{},clips{};
    std::uint64_t music_starts{},engine_starts{},turbo_starts{};
    std::uint64_t laser_events{},impact_events{},rock_destructions{};
    std::uint64_t ship_hits{},ship_destructions{},respawns{};
    std::uint64_t one_shots_played{},dropped_one_shots{};
};
class SceneAudio final {
public:
    static constexpr std::size_t voice_capacity=32,effect_voice_capacity=29;
    SceneAudio()=default;
    ~SceneAudio(){close();}
    SceneAudio(const SceneAudio&)=delete;
    SceneAudio& operator=(const SceneAudio&)=delete;
    // Call only while the exclusive transient-scene lease is active. Assets
    // stay alive until close(); host create copies PCM. Missing clips skip
    // optional cues, whereas malformed data/backend failure cleans up fully.
    bool initialize(const SceneAudioAssets&,SceneAudioBackend)noexcept;
    // One call per native simulation tick, never per render. A paused call
    // changes only host pause state: no cue consumption or boost advancement.
    void tick(const SceneAudioFrame&)noexcept;
    void close()noexcept; // before lease release, also on every error path
    const SceneAudioStats& stats()const noexcept{return stats_;}
    float smoothed_boost()const noexcept{return boost_;}
private:
    const SceneAudioAssets* assets_{};
    SceneAudioBackend backend_{};
    std::array<std::uint32_t,scene_audio_clip_count> clips_{};
    std::array<std::uint32_t,effect_voice_capacity> effects_{};
    std::uint32_t music_{},engine_{},turbo_{};
    const ShipProfile* ship_profile_{};
    float boost_{};
    double last_scene_seconds_=-1.;
    std::uint64_t last_shot_{},last_event_{};
    std::uint32_t last_damage_{},last_respawns_{};
    bool last_destroyed_{};
    SceneAudioStats stats_{};
    SceneAudioParams params(SceneAudioClipId,bool loop,bool music,float gain=1.f,float pitch=1.f)const noexcept;
    bool has(SceneAudioClipId)const noexcept;
    bool loop(std::uint32_t&,SceneAudioClipId,bool music,float gain,float pitch);
    void stop(std::uint32_t&);
    void cue(SceneAudioClipId,float gain=1.f);
    void voices();
    void consume_combat(const SceneAudioFrame&,bool playable);
};
}

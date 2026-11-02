// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_audio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace spacevoyages;
namespace {std::size_t allocations;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks;
void require(bool pass,const char* why){++checks;if(!pass){std::fprintf(stderr,"scene_audio: %s\n",why);std::exit(1);}}
std::size_t ix(SceneAudioClipId id){return static_cast<std::size_t>(id);}
SceneAudioAssets assets(){
    SceneAudioAssets a;
    for(std::size_t i=0;i<a.clips.size();++i){auto& clip=a.clips[i];clip.samples.assign(128,std::int16_t(i+1));clip.sample_rate=8000;clip.channels=1;}
    a.clips[ix(SceneAudioClipId::Music)].loop_begin_frame=16;a.clips[ix(SceneAudioClipId::Music)].loop_end_frame=96;
    return a;
}
struct Fake {
    struct Clip {SceneAudioPcmView pcm{};bool live{};};
    struct Voice {std::uint32_t id{},clip{};SceneAudioParams params{};bool live{};double cursor{};};
    std::array<Clip,256> clips{};std::array<Voice,64> voices{};
    std::array<unsigned,scene_audio_clip_count> plays{};
    std::uint32_t created{},released{},next_voice{},stops{},pause_calls{},set_calls{},play_calls{};
    unsigned create_failure{},play_failure{},set_failure{};bool paused{},parameters_valid=true;
    Voice* voice(std::uint32_t id){for(auto& v:voices)if(v.id==id)return &v;return nullptr;}
    bool params_valid(const SceneAudioParams& p){return std::isfinite(p.gain)&&p.gain>=0&&p.gain<=4&&std::isfinite(p.pitch)&&p.pitch>=.25f&&p.pitch<=4&&p.pan>=-1&&p.pan<=1;}
    SceneAudioBackend backend(){return {this,
        +[](void* u,const SceneAudioPcmView& pcm){auto& f=*static_cast<Fake*>(u);++f.created;if(f.create_failure==f.created)return std::uint32_t(0);f.clips[f.created]={pcm,true};return f.created;},
        +[](void* u,std::uint32_t clip){auto& f=*static_cast<Fake*>(u);if(f.clips[clip].live){f.clips[clip].live=false;++f.released;}return 1;},
        +[](void* u,std::uint32_t clip,const SceneAudioParams& p){auto& f=*static_cast<Fake*>(u);++f.play_calls;f.parameters_valid=f.parameters_valid&&f.params_valid(p);if(f.play_failure==f.play_calls)return std::uint32_t(0);
            for(auto& v:f.voices)if(!v.live){v={++f.next_voice,clip,p,true,0};++f.plays[std::size_t(f.clips[clip].pcm.samples[0]-1)];return v.id;}return std::uint32_t(0);},
        +[](void* u,std::uint32_t id,const SceneAudioParams& p){auto& f=*static_cast<Fake*>(u);++f.set_calls;f.parameters_valid=f.parameters_valid&&f.params_valid(p);if(f.set_failure==f.set_calls)return 0;auto* v=f.voice(id);if(!v||!v->live)return 0;v->params=p;return 1;},
        +[](void* u,std::uint32_t id){auto& f=*static_cast<Fake*>(u);++f.stops;for(auto& v:f.voices)if(!id||v.id==id)v.live=false;return 1;},
        +[](void* u,bool paused){auto& f=*static_cast<Fake*>(u);++f.pause_calls;f.paused=paused;return 1;},
        +[](void* u,std::uint32_t id){auto& f=*static_cast<Fake*>(u);auto* v=f.voice(id);return v&&v->live?1:0;}
    };}
    void advance(double seconds){if(paused)return;
        for(auto& v:voices)if(v.live&&!v.params.paused){const auto& pcm=clips[v.clip].pcm;v.cursor+=seconds*pcm.sample_rate*v.params.pitch;
            if(v.params.loop){const double end=double(pcm.loop_end_frame?pcm.loop_end_frame:pcm.frames);if(v.cursor>=end)v.cursor=double(pcm.loop_begin_frame)+std::fmod(v.cursor-pcm.loop_begin_frame,end-pcm.loop_begin_frame);}
            else if(v.cursor>=pcm.frames)v.live=false;
        }
    }
    unsigned live_voices()const {return unsigned(std::count_if(voices.begin(),voices.end(),[](const auto& v){return v.live;}));}
    unsigned live_clips()const {return unsigned(std::count_if(clips.begin(),clips.end(),[](const auto& c){return c.live;}));}
    Voice* semantic(SceneAudioClipId clip){for(auto& v:voices)if(v.live&&clips[v.clip].pcm.samples[0]==std::int16_t(ix(clip)+1))return &v;return nullptr;}
};
void tick(SceneAudio& audio,Fake& fake,SceneAudioFrame& frame){fake.advance(frame.seconds);frame.scene_seconds+=1./60.;audio.tick(frame);}
SpaceCombatAssets weapons(){SpaceCombatAssets a;for(auto& s:a.ships){s.armed=true;s.count=1;}return a;}
}
int main(){
    require(scene_audio_clip_count==14&&SceneAudio::voice_capacity==32,"semantic audio catalog and scene voice budget fit SDK16clip/64voice caps");
    {
        auto a=assets();Fake fake;SceneAudio audio;require(audio.initialize(a,fake.backend()),"Original borrows the existing audio catalog");
        const auto created=fake.created;SceneAudioFrame frame;frame.identity=ShipIdentity::Original;frame.ship_class=533;frame.free_flight=true;frame.boost_fraction=1;
        SpaceCombat foreign;foreign.set_assets(weapons());foreign.set_world({},nullptr,0);foreign.reset(ShipIdentity::Red);frame.combat=&foreign;FlightState ship;ship.speed=0;
        const auto before=allocations;for(int t=0;t<120;++t){foreign.step(ship,true,frame.scene_seconds+1./60);tick(audio,fake,frame);}
        require(fake.created==created&&fake.semantic(SceneAudioClipId::Engine531)&&fake.semantic(SceneAudioClipId::Turbo531)&&!fake.semantic(SceneAudioClipId::Engine533),"explicit Original selects its declared fighter69 fallback without duplicating PCM or taking the legacy class profile");
        require(!audio.stats().laser_events&&!fake.plays[ix(SceneAudioClipId::Laser532)]&&!fake.plays[ix(SceneAudioClipId::Laser533)]&&allocations==before,"Original emits no laser cues even with foreign armed combat and allocates no tick storage");
        frame.identity=static_cast<ShipIdentity>(255);tick(audio,fake,frame);
        require(!fake.semantic(SceneAudioClipId::Engine531)&&!fake.semantic(SceneAudioClipId::Engine533),"invalid explicit identity never falls back to a native-class engine");
    }
    {
        auto a=assets();Fake fake;SceneAudio audio;const auto before=allocations;
        require(audio.initialize(a,fake.backend())&&audio.stats().clips==14&&allocations==before,"scene initialization borrows decoded PCM and creates fixed host resources without copying or allocating in the module");
        const auto& music=fake.clips[1].pcm;
        require(music.samples==a.clips[0].samples.data()&&music.frames==128&&music.loop_begin_frame==16&&music.loop_end_frame==96,"complete native PCM intro and exclusive loop points reach the host unchanged");
        SceneAudioFrame frame;tick(audio,fake,frame);
        require(fake.semantic(SceneAudioClipId::Music)&&fake.semantic(SceneAudioClipId::Engine531)&&audio.stats().music_starts==1&&audio.stats().engine_starts==1,"first native tick starts one music loop and the selected ship engine");
        require(fake.semantic(SceneAudioClipId::Music)->params.music&&!fake.semantic(SceneAudioClipId::Engine531)->params.music,"music and effects use independent native mixer groups");
        frame.boost_fraction=1;tick(audio,fake,frame);const float first=audio.smoothed_boost();
        require(first>0&&first<1&&fake.semantic(SceneAudioClipId::Turbo531)&&audio.stats().turbo_starts==1,"boost starts its original distinct turbo loop and crossfades gradually");
        const auto engine=fake.semantic(SceneAudioClipId::Engine531)->params,turbo=fake.semantic(SceneAudioClipId::Turbo531)->params;
        require(engine.gain<1&&engine.gain>0&&turbo.gain>0&&turbo.gain<1&&std::abs(engine.gain+turbo.gain-1)<1e-6f&&engine.pitch>1,"base and turbo gains form a bounded smooth crossfade with native-base pitch modulation");
        const auto sets=fake.set_calls;audio.tick(frame);require(audio.smoothed_boost()==first&&fake.set_calls==sets,"duplicate same-timestamp calls cannot advance engine settling or restart sound");
        frame.damage.damage_events=1;frame.damage.hull=90;tick(audio,fake,frame);
        frame.paused=true;audio.tick(frame);const auto position=fake.semantic(SceneAudioClipId::Music)->cursor;const auto effect_position=fake.semantic(SceneAudioClipId::ShipHit)->cursor;const float boost=audio.smoothed_boost();
        fake.advance(4);frame.seconds=.25f;frame.boost_fraction=0;audio.tick(frame);
        require(fake.paused&&fake.semantic(SceneAudioClipId::Music)->cursor==position&&audio.smoothed_boost()==boost&&fake.pause_calls==1,"pause freezes music, effects and engine envelopes without consuming elapsed time");
        require(fake.semantic(SceneAudioClipId::ShipHit)->cursor==effect_position,"pause freezes an active effects-group one-shot cursor as well as the music group");
        frame.paused=false;frame.seconds=1.f/60;tick(audio,fake,frame);
        require(!fake.paused&&fake.pause_calls==2&&audio.stats().music_starts==1,"resume continues the same music voice rather than restarting its native intro");
        for(int t=0;t<100;++t)tick(audio,fake,frame);
        require(!fake.semantic(SceneAudioClipId::Turbo531)&&audio.stats().engine_starts==1,"turbo fades out before its voice stops, retaining the base engine loop");
        require(fake.semantic(SceneAudioClipId::Music)->cursor>=16&&fake.semantic(SceneAudioClipId::Music)->cursor<96,"fake host demonstrates preserved intro then bounded loop cursor semantics");
        audio.close();audio.close();require(!fake.live_clips()&&!fake.live_voices()&&fake.released==14&&!audio.stats().active,"idempotent scene close stops all owner voices and releases every clip exactly once");
    }
    {
        auto a=assets();a.clips[ix(SceneAudioClipId::Turbo532)].samples.clear();Fake fake;SceneAudio audio;audio.initialize(a,fake.backend());SceneAudioFrame frame;frame.ship_class=532;frame.boost_fraction=1;
        for(int t=0;t<100;++t)tick(audio,fake,frame);
        require(fake.semantic(SceneAudioClipId::Engine532)&&fake.semantic(SceneAudioClipId::Engine532)->params.pitch>1.11f&&!fake.semantic(SceneAudioClipId::Turbo532),"missing optional turbo falls back to base-engine pitch instead of inventing a sound");
        frame.ship_class=533;tick(audio,fake,frame);
        require(!fake.semantic(SceneAudioClipId::Engine532)&&fake.semantic(SceneAudioClipId::Engine533)&&fake.semantic(SceneAudioClipId::Turbo533)&&audio.stats().music_starts==1,"ship variants select their own engine/turbo bindings while music remains continuous");
    }
    {
        auto a=assets();Fake fake;SceneAudio audio;audio.initialize(a,fake.backend());SpaceCombat combat;combat.set_world({},nullptr,0);combat.set_assets(weapons());combat.reset(532);
        SceneAudioFrame frame;frame.ship_class=532;frame.free_flight=true;frame.combat=&combat;FlightState ship;ship.speed=0;
        const auto before=allocations;
        for(int t=0;t<60;++t){combat.step(ship,true,frame.scene_seconds+1./60.);tick(audio,fake,frame);for(int draw=0;draw<4;++draw)audio.tick(frame);}
        require(combat.state().shots==15&&audio.stats().laser_events==15&&fake.plays[ix(SceneAudioClipId::Laser532)]==15,"15Hz native shot ids produce one cue each, with no per-draw duplication");
        require(allocations==before,"native audio/combat tick and deduplication hot paths allocate no memory");
        frame.ship_class=531;for(int t=0;t<20;++t){combat.step(ship,true,frame.scene_seconds+1./60.);tick(audio,fake,frame);}
        require(audio.stats().laser_events==15&&!fake.plays[ix(SceneAudioClipId::Laser533)],"green531 never emits a laser cue even if supplied combat metadata belongs to an armed ship");
        frame.ship_class=533;frame.free_flight=false;for(int t=0;t<20;++t){combat.step(ship,true,frame.scene_seconds+1./60.);tick(audio,fake,frame);}
        require(audio.stats().laser_events==15,"intro and approach calls consume stale shot ids without creating cinematic laser audio");
    }
    {
        auto a=assets();Fake fake;SceneAudio audio;audio.initialize(a,fake.backend());SpaceCombat combat;combat.set_world({{{20,0,0},3}},nullptr,0);combat.set_assets(weapons());combat.reset(533);
        SceneAudioFrame frame;frame.ship_class=533;frame.free_flight=true;frame.combat=&combat;FlightState ship;ship.speed=0;
        for(int t=0;t<50;++t){combat.step(ship,t<12,frame.scene_seconds+1./60.);tick(audio,fake,frame);}
        require(audio.stats().impact_events==2&&audio.stats().rock_destructions==1&&fake.plays[ix(SceneAudioClipId::RockHit)]==2&&fake.plays[ix(SceneAudioClipId::RockDestroyed)]==1,"persistent impact/destruction event ids produce the original corresponding cues exactly once");
        for(int t=0;t<30;++t)tick(audio,fake,frame);
        require(audio.stats().impact_events==2&&audio.stats().rock_destructions==1,"persistent combat ring entries do not replay after quiet ticks");
        frame.damage.damage_events=1;frame.damage.hull=80;tick(audio,fake,frame);frame.damage.damage_events=2;frame.damage.hull=60;tick(audio,fake,frame);
        require(audio.stats().ship_hits==2&&fake.plays[ix(SceneAudioClipId::ShipHit)]==2,"new ship damage counters cause one hit cue each");
        frame.boost_fraction=1;for(int t=0;t<20;++t)tick(audio,fake,frame);
        frame.damage.damage_events=3;frame.damage.hull=0;frame.damage.destroyed=true;tick(audio,fake,frame);
        const auto music=fake.semantic(SceneAudioClipId::Music)->id;
        require(audio.stats().ship_destructions==1&&audio.stats().ship_hits==2&&!fake.semantic(SceneAudioClipId::Engine533)&&!fake.semantic(SceneAudioClipId::Turbo533),"fatal hit selects one destruction cue and stops engines without double-layering the final hit");
        for(int t=0;t<60;++t)tick(audio,fake,frame);
        require(audio.stats().ship_destructions==1&&fake.semantic(SceneAudioClipId::Music)->id==music,"destruction progress does not replay explosion or restart music");
        frame.boost_fraction=0;frame.damage.hull=100;frame.damage.destroyed=false;frame.damage.respawns=1;frame.damage.respawn_intro=true;frame.free_flight=false;tick(audio,fake,frame);
        require(audio.stats().respawns==1&&audio.stats().engine_starts==2&&fake.semantic(SceneAudioClipId::Engine533)&&fake.semantic(SceneAudioClipId::Music)->id==music,"local respawn restores engine and cue while preserving the same continuous music voice");
        require(!fake.semantic(SceneAudioClipId::Turbo533)&&audio.smoothed_boost()==0&&fake.semantic(SceneAudioClipId::Engine533)->params.gain==1,"dying under boost cannot carry a stale turbo voice or engine envelope into the respawn intro");
        frame.damage.respawn_intro=false;frame.free_flight=true;for(int t=0;t<60;++t)tick(audio,fake,frame);
        require(audio.stats().ship_hits==2&&audio.stats().ship_destructions==1&&audio.stats().respawns==1,"respawn grace and old damage counters create no phantom hit or repeated respawn audio");
    }
    {
        auto a=assets();Fake fake;SceneAudio audio;audio.initialize(a,fake.backend());SceneAudioFrame frame;frame.damage.damage_events=100000;tick(audio,fake,frame);
        require(audio.stats().voices<=32&&audio.stats().peak_voices<=32&&audio.stats().dropped_one_shots>0&&fake.live_voices()<=32,"large externally injected cue counters remain bounded by the fixed voice pool");
        frame.damage.damage_events=100000;const auto before=allocations;
        for(int t=0;t<10000;++t){frame.boost_fraction=(t%400<200)?1.f:0.f;tick(audio,fake,frame);}
        require(allocations==before&&fake.parameters_valid&&audio.stats().music_starts==1,"ten thousand engine/boost/cleanup ticks use bounded parameters, no allocations and one music loop");
        frame.scene_active=false;audio.tick(frame);require(!fake.live_clips()&&!fake.live_voices(),"closing/error scene state releases all owned audio before the lease ends");
    }
    {
        auto a=assets();Fake fake;fake.create_failure=4;SceneAudio audio;
        require(!audio.initialize(a,fake.backend())&&audio.stats().failed&&!fake.live_clips()&&!fake.live_voices()&&fake.released==3,"partial SDK clip-creation failure releases all earlier resources");
        Fake play_failure;SceneAudio failed;play_failure.play_failure=2;failed.initialize(a,play_failure.backend());SceneAudioFrame frame;tick(failed,play_failure,frame);
        require(failed.stats().failed&&!failed.stats().active&&!play_failure.live_voices()&&!play_failure.live_clips(),"play failure stops prior music and releases the entire scene audio pack");
        Fake set_failure;SceneAudio setters;set_failure.set_failure=1;setters.initialize(a,set_failure.backend());SceneAudioFrame f;tick(setters,set_failure,f);tick(setters,set_failure,f);
        require(setters.stats().failed&&!set_failure.live_clips()&&!set_failure.live_voices(),"runtime voice update failure follows the same complete cleanup path");
        a.clips[0].loop_begin_frame=96;a.clips[0].loop_end_frame=16;Fake invalid;SceneAudio refuse;
        require(!refuse.initialize(a,invalid.backend())&&invalid.created==0,"invalid loop intervals fail before any host resource is created");
        a=assets();a.clips[0].channels=2;a.clips[0].samples.push_back(1);
        require(!refuse.initialize(a,invalid.backend()),"partial stereo frames are rejected instead of indexing beyond borrowed PCM");
        SceneAudioAssets empty;require(!refuse.initialize(empty,invalid.backend())&&!refuse.stats().failed,"an absent optional audio pack cleanly remains silent");
    }
    std::printf("scene_audio: %u PCM, loop, native-event, pause, respawn, bounded-voice and cleanup checks passed\n",checks);
}

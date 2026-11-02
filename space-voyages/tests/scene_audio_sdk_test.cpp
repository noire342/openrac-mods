// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises the actual adapter, including the SDK's zero-success convention.
#include "scene_audio_sdk.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace spacevoyages;
namespace {
unsigned checks{},calls{},sets{},pauses{},stops{},releases{};int sdk_result=OPENRAC_HOOK_OK;
openrac_mod* expected_owner=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x12340));
openrac_transient_audio_pcm captured_pcm{};openrac_transient_audio_params captured_params{};
std::uint32_t captured_handle{};int captured_pause{};
std::array<bool,256> live{};
std::uint32_t next_clip=1,next_voice=100;
void check(bool value,const char* what,int line){++checks;if(!value){std::fprintf(stderr,"adapter:%d %s\n",line,what);std::exit(2);}}
#define CHECK(value) check(bool(value),#value,__LINE__)
void owner(openrac_mod* value){++calls;CHECK(value==expected_owner);}
std::uint32_t create(openrac_mod* m,const openrac_transient_audio_pcm* p){owner(m);CHECK(p&&p->size==sizeof(*p));captured_pcm=*p;return next_clip++;}
int release(openrac_mod* m,std::uint32_t h){owner(m);++releases;captured_handle=h;return sdk_result;}
std::uint32_t play(openrac_mod* m,std::uint32_t h,const openrac_transient_audio_params* p){owner(m);CHECK(p&&p->size==sizeof(*p));captured_handle=h;captured_params=*p;const auto v=next_voice++;CHECK(v<live.size());live[v]=true;return v;}
int set(openrac_mod* m,std::uint32_t h,const openrac_transient_audio_params* p){owner(m);++sets;captured_handle=h;CHECK(p&&p->size==sizeof(*p));captured_params=*p;return sdk_result;}
int stop(openrac_mod* m,std::uint32_t h){owner(m);++stops;captured_handle=h;if(!h)live.fill(false);else{CHECK(h<live.size());live[h]=false;}return sdk_result;}
int pause(openrac_mod* m,int value){owner(m);++pauses;captured_pause=value;return sdk_result;}
int playing(openrac_mod* m,std::uint32_t h){owner(m);captured_handle=h;return h<live.size()&&live[h];}
void protocol(){
    openrac_api api{};api.transient_audio_create=create;api.transient_audio_release=release;api.transient_audio_play=play;api.transient_audio_set=set;
    api.transient_audio_stop=stop;api.transient_audio_pause=pause;api.transient_audio_playing=playing;
    SceneAudioSdk sdk{&api,expected_owner};auto backend=sdk.backend();CHECK(backend.user==&sdk);
    const std::array<std::int16_t,12> samples{0,1,2,3,4,5,6,7,8,9,10,11};
    SceneAudioPcmView pcm{44100,2,samples.data(),6,2,5};const auto clip=backend.create(backend.user,pcm);
    CHECK(clip&&captured_pcm.samples==samples.data()&&captured_pcm.sample_rate==44100&&captured_pcm.channels==2&&captured_pcm.frames==6);
    CHECK(captured_pcm.loop_begin_frame==2&&captured_pcm.loop_end_frame==5);
    SceneAudioParams p{true,true,true,.6f,1.2f,-.3f};const auto voice=backend.play(backend.user,clip,p);CHECK(voice&&captured_handle==clip);
    CHECK(captured_params.flags==(OPENRAC_TRANSIENT_AUDIO_LOOP|OPENRAC_TRANSIENT_AUDIO_PAUSED)&&captured_params.group==OPENRAC_TRANSIENT_AUDIO_MUSIC);
    CHECK(captured_params.gain==.6f&&captured_params.pitch==1.2f&&captured_params.pan==-.3f);
    p={false,false,false,.8f,.9f,.2f};CHECK(backend.set(backend.user,voice,p)==1&&captured_handle==voice);
    CHECK(captured_params.flags==0&&captured_params.group==OPENRAC_TRANSIENT_AUDIO_EFFECTS&&captured_params.gain==.8f);
    CHECK(backend.pause(backend.user,true)==1&&captured_pause==1);CHECK(backend.pause(backend.user,false)==1&&captured_pause==0);
    CHECK(backend.playing(backend.user,voice)==1);CHECK(backend.stop(backend.user,0)==1&&captured_handle==0);CHECK(backend.playing(backend.user,voice)==0);
    CHECK(backend.release(backend.user,clip)==1&&captured_handle==clip);
    sdk_result=OPENRAC_HOOK_UNKNOWN;
    CHECK(backend.release(backend.user,clip)==0&&backend.set(backend.user,voice,p)==0&&backend.stop(backend.user,0)==0&&backend.pause(backend.user,true)==0);
    sdk_result=OPENRAC_HOOK_OK;
    SceneAudioAssets assets;for(auto id:{SceneAudioClipId::Music,SceneAudioClipId::Engine531}){auto& c=assets.clips[std::size_t(id)];c.sample_rate=48000;c.channels=1;c.samples={1000,2000,3000,4000};}
    SceneAudio audio;CHECK(audio.initialize(assets,backend));SceneAudioFrame frame;frame.ship_class=531;frame.scene_seconds=1;audio.tick(frame);
    CHECK(audio.stats().active&&!audio.stats().failed&&audio.stats().music_starts==1&&audio.stats().engine_starts==1);
    const auto previous_sets=sets;frame.scene_seconds+=1./60.;audio.tick(frame);CHECK(sets>previous_sets&&!audio.stats().failed); // The old raw0-success adapter failed here.
    frame.paused=true;audio.tick(frame);CHECK(audio.stats().paused&&captured_pause==1&&!audio.stats().failed);
    frame.paused=false;frame.scene_seconds+=1./60.;audio.tick(frame);CHECK(!audio.stats().paused&&!audio.stats().failed&&captured_pause==0);
    const auto prior_releases=releases;audio.close();CHECK(!audio.stats().active&&releases==prior_releases+2&&captured_handle!=0&&stops>0&&pauses>0&&calls>0);
}
}
int main(){protocol();std::printf("PASS %u checks: actual SDK1.13 adapter sizes/owner/PCM intro-loop/flags/groups/gain/pitch/pan, OK0-to-true and failure-to-false, stop0, music+engine nexttick update/pause/close. No device proof.\n",checks);}

// SPDX-License-Identifier: GPL-3.0-or-later
// Production Session and SDK adapter, with fake audio handles and no GL/device.
#include "../src/mod.cpp"
#include <array>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks;
void require(bool pass,const char* why){++checks;if(!pass){std::fprintf(stderr,"session_audio: %s\n",why);std::exit(1);}}
std::vector<unsigned char> memory(0x2000000);
openrac_api api{};
auto* const owner=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x1234));
struct Fake {
 struct Clip {openrac_transient_audio_pcm pcm{};bool live{};};
 struct Voice {std::uint32_t id{},clip{};openrac_transient_audio_params params{};bool live{};};
 std::array<Clip,32> clips{};std::array<Voice,64> voices{};
 unsigned created{},released{},plays{},sets{},stop_calls{},pause_calls{},settings_reads{};
 unsigned fail_create{},fail_play{},fail_set{};
 bool lease=true,paused{},abi_valid=true;
 Voice* voice(std::uint32_t id){for(auto& v:voices)if(v.id==id)return &v;return nullptr;}
 unsigned live_clips()const{unsigned count=0;for(const auto& c:clips)count+=c.live;return count;}
 unsigned live_voices()const{unsigned count=0;for(const auto& v:voices)count+=v.live;return count;}
 unsigned semantic(SceneAudioClipId clip)const{unsigned count=0;for(const auto& v:voices)if(v.live&&clips[v.clip].pcm.samples[0]==std::int16_t(std::size_t(clip)+1))++count;return count;}
} fake;
void reset(){
 fake={};std::fill(memory.begin(),memory.end(),0);api={};api.size=sizeof api;api.api_major=1;api.api_minor=13;api.memory=memory.data();
 memory[disc::ntsc_u.coordinates.value+1]=1;memory[disc::ntsc_u.coordinates.value+3]=1;
 api.relocate_data=+[](unsigned at){return at;};api.relocate_level_data=+[](int,unsigned at){return at;};api.loaded_level=+[](){return 3;};
 api.setting_bool=+[](openrac_mod*,const char* key)->int{return !std::strcmp(key,"ship_weapons");};
 api.setting_int=+[](openrac_mod*,const char* key)->std::int64_t{++fake.settings_reads;return !std::strcmp(key,"music_volume")?20:30;};
 api.log=+[](openrac_mod*,int,const char*){};api.transient_gl_proc=+[](const char*)->void*{return nullptr;};
 api.transient_audio_create=+[](openrac_mod* mod,const openrac_transient_audio_pcm* p)->std::uint32_t{
  fake.abi_valid=fake.abi_valid&&mod==owner&&p&&p->size==sizeof(*p);if(!fake.lease)return 0;
  ++fake.created;if(fake.fail_create==fake.created)return 0;fake.clips[fake.created]={*p,true};return fake.created;
 };
 api.transient_audio_release=+[](openrac_mod* mod,std::uint32_t clip){fake.abi_valid=fake.abi_valid&&mod==owner;if(fake.clips[clip].live){fake.clips[clip].live=false;++fake.released;}return int(OPENRAC_HOOK_OK);};
 api.transient_audio_play=+[](openrac_mod* mod,std::uint32_t clip,const openrac_transient_audio_params* p)->std::uint32_t{
  fake.abi_valid=fake.abi_valid&&mod==owner&&p&&p->size==sizeof(*p)&&p->group<=1&&p->flags<=3;
  ++fake.plays;if(!fake.lease||fake.fail_play==fake.plays)return 0;
  for(std::size_t i=0;i<fake.voices.size();++i)if(!fake.voices[i].live){const auto id=std::uint32_t(i+1);fake.voices[i]={id,clip,*p,true};return id;}return 0;
 };
 api.transient_audio_set=+[](openrac_mod* mod,std::uint32_t id,const openrac_transient_audio_params* p){fake.abi_valid=fake.abi_valid&&mod==owner&&p->size==sizeof(*p);++fake.sets;auto* v=fake.voice(id);if(fake.fail_set==fake.sets||!v||!v->live)return int(OPENRAC_HOOK_UNKNOWN);v->params=*p;return int(OPENRAC_HOOK_OK);};
 api.transient_audio_stop=+[](openrac_mod* mod,std::uint32_t id){fake.abi_valid=fake.abi_valid&&mod==owner;++fake.stop_calls;for(auto& v:fake.voices)if(!id||v.id==id)v.live=false;return int(OPENRAC_HOOK_OK);};
 api.transient_audio_pause=+[](openrac_mod* mod,int paused){fake.abi_valid=fake.abi_valid&&mod==owner;++fake.pause_calls;fake.paused=paused!=0;return int(OPENRAC_HOOK_OK);};
 api.transient_audio_playing=+[](openrac_mod*,std::uint32_t id){const auto* v=fake.voice(id);return v&&v->live?1:0;};
 api.transient_audio_stats=+[](openrac_mod* mod,openrac_transient_audio_stats* out){fake.abi_valid=fake.abi_valid&&mod==owner&&out->size==sizeof(*out);out->clips=fake.live_clips();out->voices=fake.live_voices();out->paused=fake.paused;return int(OPENRAC_HOOK_OK);};
}
SceneAssets data(bool available=true){
 SceneAssets out;out.audio_available=available;
 if(available)for(std::size_t i=0;i<out.audio.clips.size();++i){if(i==std::size_t(SceneAudioClipId::Respawn))continue;auto& clip=out.audio.clips[i];clip.samples.assign(128,std::int16_t(i+1));clip.sample_rate=8000;clip.channels=1;}
 out.audio.clips[0].loop_begin_frame=16;out.audio.clips[0].loop_end_frame=96;
 out.projectile_effects.bolt_asset_id="projectile_laser";MeshAsset bolt;bolt.id="projectile_laser";out.meshes.push_back(std::move(bolt));
 for(unsigned i=1;i<3;++i){out.combat.ships[i].armed=true;out.combat.ships[i].count=1;}
 return out;
}
void prepare(Session& s,ShipIdentity identity){std::array<bool,19> c{};c[1]=c[3]=true;require(s.voyage.begin(3,c,identity)&&s.voyage.departure_complete(),"synthetic Session fixture enters free flight without a game or GL context");s.identity=identity;s.ship=ship_profile(identity)->native_carrier_class;s.origin=3;s.in_scene=true;s.snapshot_visual();}
void prepare(Session& s,int ship){const auto identity=ship_identity_from_native_class(ship);require(identity.has_value(),"legacy Session audio fixture names a stock carrier");prepare(s,*identity);}
}
int main(){
 for(const auto identity:{ShipIdentity::Green,ShipIdentity::Red,ShipIdentity::Purple,ShipIdentity::Original}){
  reset();Session s(&api,owner,disc::ntsc_u,data());prepare(s,identity);
  require(fake.created==0&&fake.plays==0&&fake.settings_reads==0,"constructor neither adds special volume controls nor acquires audio outside the scene tick");
  const auto native_before=memory;
  for(int t=0;t<60;++t)require(s.tick(1.f/60,0x8000,nullptr)==1,"audio-enabled Session native tick continues normally");
  require(s.audio_started&&s.audio.stats().active&&fake.created==13&&fake.abi_valid,"first native tick acquires the13 original PCM resources through the real SDK1.13 adapter");
  require(fake.clips[1].pcm.loop_begin_frame==16&&fake.clips[1].pcm.loop_end_frame==96,"Session adapter preserves original intro/loop positions");
  require(s.audio.stats().laser_events==(ship_profile(identity)->weapons_allowed?15:0),"explicit ship identity routes15Hz shot ids while Original and Green remain silent");
  bool groups=false;for(const auto& v:fake.voices)if(v.live&&fake.clips[v.clip].pcm.samples[0]==1)groups=v.params.group==OPENRAC_TRANSIENT_AUDIO_MUSIC&&v.params.gain==1;
  require(groups,"music uses the normal game mixer group with unity scene gain and no extra volume control");
  require(memory==native_before,"scene audio, combat and callbacks do not alter any byte of native32MiB");
  s.tick(1.f/60,8,nullptr);const auto plays=fake.plays,sets=fake.sets;const auto clock=s.voyage.scene_seconds();
  for(int t=0;t<20;++t)s.tick(1.f/60,0x8000,nullptr);
  require(s.paused&&fake.paused&&fake.plays==plays&&fake.sets==sets&&s.voyage.scene_seconds()==clock,"paused Session freezes both groups, native audio cues and engine updates without a draw");
  s.tick(1.f/60,8,nullptr);require(!s.paused&&!fake.paused&&s.audio.stats().music_starts==1,"resume keeps the original music voice rather than replaying the intro");
  const auto calls=fake.plays;const auto settings=fake.sets;s.draw(1280,800,.5f);
  require(!s.error.empty()&&fake.plays==calls&&fake.sets==settings,"a controlled missing-GL render failure never acquires, plays or updates audio");
  require(s.tick(1.f/60,0,nullptr)==0,"render error yields to the host closing path before another audio tick");
  SceneAudioFrame lifecycle;lifecycle.scene_seconds=s.voyage.scene_seconds()+1;lifecycle.ship_class=s.ship;lifecycle.identity=identity;lifecycle.damage.respawns=1;lifecycle.damage.respawn_intro=true;
  const auto before_respawn=fake.plays;s.audio.tick(lifecycle);
  require(s.audio.stats().respawns==1&&fake.plays==before_respawn,"the lifecycle can register a respawn without inventing a voice for the absent non-Gemlik cue");
  s.closing();s.closing();require(!fake.live_clips()&&!fake.live_voices()&&fake.released==13,"host closing callback releases all original audio after normal operation or a render failure");
 }
 for(int failure:{0,1,2,3}){
  reset();if(failure==0)fake.fail_create=4;if(failure==1)fake.fail_play=2;if(failure==2)fake.fail_set=1;if(failure==3)fake.lease=false;
  Session s(&api,owner,disc::ntsc_u,data());prepare(s,532);bool threw=false;
  try{s.tick(1.f/60,0,nullptr);if(failure==2)s.tick(1.f/60,0,nullptr);}catch(const std::exception&){threw=true;}
  require(threw,"SDK create/play/set or missing-lease failure is surfaced to the production transient callback boundary");
  s.closing();require(!fake.live_clips()&&!fake.live_voices(),"all Session audio error paths clean up every successfully acquired host resource");
 }
 reset();api.setting_int=nullptr;Session legacy(&api,owner,disc::ntsc_u,data(false));prepare(legacy,531);legacy.tick(1.f/60,0,nullptr);
 require(!legacy.audio_started&&!fake.created&&!fake.plays,"legacy audio-absent packs safely leave the optional setting_int pointer unused");
 legacy.closing();
 std::printf("session_audio: %u SDK adapter, native-tick, pause, render-quiescence and cleanup checks passed\n",checks);
}

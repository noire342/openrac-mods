// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "weapon_audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include <limits>
using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"weapon audio: %s\n",why);std::exit(1);}}
struct Backend:WeaponAudioPlayback{
 struct Sound{WeaponAudioCue cue;float gain,pitch;bool loop;};
 std::map<Voice,Sound> active;std::vector<Sound> played;std::set<WeaponAudioCue> missing;
 unsigned next=1,peak{};
 bool available(WeaponAudioCue cue)const override{return !missing.count(cue);}
 Voice play(WeaponAudioCue cue,float gain,float pitch,bool loop)override{
  require(std::isfinite(gain)&&std::isfinite(pitch)&&gain>=0&&gain<=1&&pitch>=.5f&&pitch<=1.5f,"play parameters stay finite and within the native mixer range");
  const auto id=next++;active.emplace(id,Sound{cue,gain,pitch,loop});played.push_back({cue,gain,pitch,loop});peak=std::max(peak,unsigned(active.size()));return id;
 }
 bool set(Voice id,float gain,float pitch)override{
  require(std::isfinite(gain)&&std::isfinite(pitch)&&gain>=0&&gain<=1&&pitch>=.5f&&pitch<=1.5f,"voice changes remain valid");
  const auto found=active.find(id);if(found==active.end())return false;found->second.gain=gain;found->second.pitch=pitch;return true;
 }
 void stop(Voice id)override{active.erase(id);}
 unsigned count(WeaponAudioCue cue)const{return unsigned(std::count_if(played.begin(),played.end(),[&](const Sound& s){return s.cue==cue;}));}
 unsigned loops()const{return unsigned(std::count_if(active.begin(),active.end(),[](const auto& s){return s.second.loop;}));}
};
WeaponAudioFrame frame(){WeaponAudioFrame f;f.playing=f.alive=f.equipped=true;f.ammunition=20;f.rotor_speed=1;return f;}
void steps(WeaponAudio& audio,WeaponAudioFrame f,unsigned n){while(n--)audio.update(f);}
}
int main(){
 {Backend b;WeaponAudio audio(b);for(const auto cue:{WeaponAudioCue::Equip,WeaponAudioCue::LevelUp,WeaponAudioCue::DryFire,WeaponAudioCue::AmmoPickup})require(!audio.emit(cue),"excluded actions retain their existing native audio");audio.update(frame());for(const auto cue:{WeaponAudioCue::Equip,WeaponAudioCue::LevelUp,WeaponAudioCue::DryFire,WeaponAudioCue::AmmoPickup})require(b.count(cue)==0,"no excluded custom clips are played");}
 {Backend b;WeaponAudio audio(b);auto f=frame();steps(audio,f,12);require(b.loops()==1&&b.count(WeaponAudioCue::IdleLoop)==1,"the equipped motor has one quiet idle loop");
  require(b.active.begin()->second.gain<=.16f,"idle motor remains discreet");f.ammunition=0;audio.update(f);require(b.active.empty(),"empty ammunition stops the idle motor immediately");
  f.ammunition=20;audio.update(f);require(b.loops()==1,"refilling resumes only the current motor loop");f.equipped=false;audio.update(f);require(b.active.empty(),"switching weapon stops its motor");}
 {Backend b;WeaponAudio audio(b);auto f=frame();f.phase=Phase::Spinning;f.ammunition=0;f.rotor_speed=24;steps(audio,f,12);
  require(b.count(WeaponAudioCue::SpinLoop)==1,"the last round still powers the actually carried enemy loop");const auto pitch=b.active.begin()->second.pitch;
  f.charge=.9f;f.rotor_speed=40.2f;audio.update(f);require(b.count(WeaponAudioCue::SpinFastLoop)==1&&b.loops()==2,"normal rotation crossfades into the fast motor");
  steps(audio,f,12);require(b.loops()==1&&b.active.begin()->second.pitch>pitch,"charge raises pitch without leaving obsolete loop voices");
  f.paused=true;audio.update(f);require(b.active.empty(),"pause stops every owned loop immediately");f.paused=false;audio.update(f);require(b.loops()==1&&b.count(WeaponAudioCue::Ready)==0,"resume restores the current loop without a stale ready cue");}
 {Backend b;WeaponAudio audio(b);auto f=frame();f.phase=Phase::Spinning;audio.update(f);require(audio.emit(WeaponAudioCue::Capture),"capture cue queues");audio.update(f);
  f.charge=1;audio.update(f);steps(audio,f,15);require(b.count(WeaponAudioCue::Capture)==1&&b.count(WeaponAudioCue::Ready)==1,"capture and full-charge readiness each sound once");
  f.paused=true;audio.emit(WeaponAudioCue::Equip);audio.update(f);f.paused=false;audio.update(f);require(b.count(WeaponAudioCue::Ready)==1&&b.count(WeaponAudioCue::Equip)==0,"pause discards pending one-shots and does not replay readiness");
  f.phase=Phase::Idle;f.charge=0;audio.update(f);f.phase=Phase::Spinning;audio.update(f);f.charge=1;audio.update(f);require(b.count(WeaponAudioCue::Ready)==2,"a later distinct capture may become ready again");}
 {Backend b;WeaponAudio audio(b);auto f=frame();audio.update(f);f.phase=Phase::Spinning;f.charge=.8f;audio.update(f);
  for(unsigned i=0;i<8;++i)require(audio.emit(WeaponAudioCue::Attack),"bounded event queue accepts its documented capacity");require(!audio.emit(WeaponAudioCue::Attack),"queue overflow is refused");audio.update(f);
  require(b.peak<=6&&b.active.size()<=6,"rapid actions own at most four one-shots plus two crossfade loops");
  f.alive=false;audio.update(f);require(b.active.empty(),"death removes all owned voices");}
 {Backend b;WeaponAudio audio(b);auto f=frame();require(audio.emit(WeaponAudioCue::GoldenImpact,.3f)&&audio.emit(WeaponAudioCue::GoldenImpact,.9f),"gold impact endpoints queue independently");audio.update(f);
  require(b.count(WeaponAudioCue::GoldenImpact)==1&&b.count(WeaponAudioCue::GoldenImpactFull)==1,"charge selects one gold impact endpoint per event");
  b.missing.insert(WeaponAudioCue::Equip);require(!audio.emit(WeaponAudioCue::Equip),"a missing equip clip returns false for the native fallback");
  require(!audio.emit(WeaponAudioCue::Attack,std::numeric_limits<float>::quiet_NaN())&&!audio.emit(WeaponAudioCue::IdleLoop),"invalid intensity and externally requested loops are refused");
  f.seconds=std::numeric_limits<float>::quiet_NaN();audio.update(f);require(b.active.empty(),"malformed timing fails quiet");}
 {Backend b;WeaponAudio audio(b);auto f=frame();f.equipped=false;audio.emit(WeaponAudioCue::Attack);audio.emit(WeaponAudioCue::Impact);audio.update(f);
  require(b.count(WeaponAudioCue::Attack)==0&&b.count(WeaponAudioCue::Impact)==1&&b.loops()==0,"a previously thrown enemy can impact after switching away without playing held-weapon actions");
  b.active.emplace(500,Backend::Sound{WeaponAudioCue::Attack,1,1,false});audio.reset();require(b.active.size()==1&&b.active.count(500),"reset stops only voices the weapon owns");}
 std::printf("weapon audio: %u event, lifecycle, charge, crossfade and voice-bound checks passed\n",checks);
}

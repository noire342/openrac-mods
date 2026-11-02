// SPDX-License-Identifier: GPL-3.0-or-later
// Offline original OpenRAC audio-library renderer. No SDL, game or audio device.
#include "audio/sound_player.h"
#include "assets/sound/vag.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {
using namespace openrac;
std::vector<std::uint8_t> read(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary|std::ios::ate);
 if(!in)throw std::runtime_error("Missing source audio file");
 const auto size=in.tellg();if(size<=0||size>128*1024*1024)throw std::runtime_error("Invalid audio file size");
 std::vector<std::uint8_t> out(static_cast<std::size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(out.data()),size);
 if(!in)throw std::runtime_error("Incomplete source audio read");return out;
}
void output(const std::filesystem::path& path,const std::vector<std::int16_t>& pcm){
 if(std::filesystem::exists(path))throw std::runtime_error("Never overwrite an audio extraction");
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(pcm.data()),std::streamsize(pcm.size()*2));
 if(!out)throw std::runtime_error("Cannot write decoded PCM");
}
void new_paths(const std::filesystem::path& pcm,const std::filesystem::path& info){
 if(std::filesystem::absolute(pcm).lexically_normal()==std::filesystem::absolute(info).lexically_normal()
    ||std::filesystem::exists(pcm)||std::filesystem::exists(info))
  throw std::runtime_error("Never overwrite an audio extraction or its metadata");
}
void metrics(std::ofstream& out,const std::vector<std::int16_t>& pcm){
 double squares=0;unsigned peak=0;std::size_t nonzero=0;
 for(const auto sample:pcm){const auto a=unsigned(std::abs(int(sample)));peak=std::max(peak,a);squares+=double(sample)*sample;if(sample)++nonzero;}
 out<<",\n\"peak\":"<<peak<<",\n\"rms\":"<<std::sqrt(squares/double(pcm.size()))<<",\n\"nonzero_samples\":"<<nonzero;
}
}
int main(int argc,char** argv){
 try{
  if(argc<2)throw std::runtime_error("Use vag INPUT OUTPUT INFO, or bank INPUT SID VOLUME BEND LOOP OUTPUT INFO");
  const std::string mode=argv[1];
  if(mode=="vag"){
   if(argc!=5)throw std::runtime_error("vag INPUT OUTPUT INFO");
   new_paths(argv[3],argv[4]);
   const auto raw=read(argv[2]);const auto vag=assets::parse_vag(raw);
   if(vag.header.data_size%16||vag.body.size()!=vag.header.data_size||vag.header.sample_rate<8000||vag.header.sample_rate>96000)
    throw std::runtime_error("Invalid/truncated original VAG body or sample rate");
   const auto pcm=assets::decode_adpcm(vag.body);
   if(pcm.empty())throw std::runtime_error("Empty VAG");output(argv[3],pcm);
   std::ofstream info(argv[4]);info<<"{\n\"sample_rate\":"<<vag.header.sample_rate<<",\n\"channels\":1,\n\"frames\":"<<pcm.size()<<",\n\"adpcm_bytes\":"<<vag.body.size();metrics(info,pcm);info<<"\n}\n";return 0;
  }
  if(mode!="bank"||argc!=9)throw std::runtime_error("bank INPUT SID VOLUME BEND LOOP OUTPUT INFO");
  new_paths(argv[7],argv[8]);
  const auto raw=read(argv[2]);const auto source=assets::parse_sound_bank(raw);const auto sid=std::stoul(argv[3]);
  const auto volume=std::stoi(argv[4]),bend=std::stoi(argv[5]);const bool loop=std::stoi(argv[6])!=0;
  if(sid>=source.sounds.size()||volume<0||volume>4096||bend<-32768||bend>32767)throw std::runtime_error("Invalid sound selection");
  std::optional<assets::SampleExtent> extent;unsigned tones=0;
  for(const auto& grain:source.sound_grains(sid))if(grain.is_tone()){
   ++tones;extent=assets::sample_extent(source.samples,grain.tone().sample_offset);
  }
  if(loop&&(tones!=1||!extent||!extent->looped))throw std::runtime_error("Loop render requires one original repeating tone");
  audio::SoundPlayer player;const auto bank=player.load_bank(raw);audio::PlayRequest request;request.volume=volume;request.pitch_bend=bend;
  const auto handle=player.play(bank,std::uint32_t(sid),request);if(!handle)throw std::runtime_error("Original sound refused to play");
  std::vector<std::int16_t> pcm;pcm.reserve(48000*2*5);
  std::uint64_t loop_begin=0,loop_end=0,phase=0,next_loop=0,period=0;unsigned stable=0,last_pitch=0,first_pitch=0;
  if(loop){next_loop=std::uint64_t(extent->samples())*4096;period=std::uint64_t(extent->samples()-extent->loop_start.value_or(0)*28)*4096;}
  bool complete=false;
  for(std::uint64_t frame=0;frame<48000ull*30;++frame){
   std::array<std::int16_t,2> sample{};player.render(sample);pcm.insert(pcm.end(),sample.begin(),sample.end());
   if(loop){
    unsigned active=0,pitch=0;
    for(const auto& voice:player.mixer().voices())if(voice.active()){++active;pitch=voice.pitch();}
    if(active!=1)throw std::runtime_error("Loop sound has no single stable original voice");
    if(!first_pitch)first_pitch=pitch;
    stable=pitch==last_pitch?stable+1:0;last_pitch=pitch;phase+=pitch;
    while(phase>=next_loop){
     if(stable>=2400){if(!loop_begin)loop_begin=frame+1;else{loop_end=frame+1;complete=true;}}
     else loop_begin=0;
     next_loop+=period;
    }
    if(complete)break;
   }else if(!player.playing(handle)&&player.mixer().active_voices()==0){complete=true;break;}
  }
  if(!complete||pcm.empty()||!player.unsupported().empty())throw std::runtime_error("Sound did not complete cleanly, or has unsupported original grains");
  output(argv[7],pcm);std::ofstream info(argv[8]);
  info<<"{\n\"sample_rate\":48000,\n\"channels\":2,\n\"frames\":"<<pcm.size()/2
      <<",\n\"sound_id\":"<<sid<<",\n\"request_volume\":"<<volume<<",\n\"request_bend\":"<<bend
      <<",\n\"loop_begin_frame\":"<<loop_begin<<",\n\"loop_end_frame\":"<<loop_end
      <<",\n\"first_spu_pitch\":"<<first_pitch<<",\n\"final_spu_pitch\":"<<last_pitch<<",\n\"unsupported_grains\":0";
  if(loop){
   const auto begin=std::size_t(loop_begin)*2,end=std::size_t(loop_end)*2;
   const auto delta=std::max(std::abs(int(pcm[end-2])-pcm[begin]),std::abs(int(pcm[end-1])-pcm[begin+1]));
   info<<",\n\"native_loop_begin_sample\":"<<extent->loop_start.value_or(0)*28
       <<",\n\"native_loop_end_sample\":"<<extent->samples()<<",\n\"loop_boundary_delta\":"<<delta;
  }
  metrics(info,pcm);info<<"\n}\n";return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

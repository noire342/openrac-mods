// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises the production reader with tiny synthetic PCM and scene bytes.
// No original artwork, native game, SDK voice or audio device is opened.
#include "assets.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace spacevoyages;
namespace {
namespace fs=std::filesystem;
unsigned checks{},rejections{};
void require(bool pass,const std::string& why){++checks;if(!pass)throw std::runtime_error("audio_assets: "+why);}
std::string replace(std::string text,const std::string& from,const std::string& to){const auto p=text.find(from);if(p==std::string::npos)throw std::runtime_error("Bad synthetic mutation token");text.replace(p,from.size(),to);return text;}
constexpr std::array<const char*,14> roles{{"music","engine_531","engine_532","engine_533","laser_532","laser_533","rock_hit","rock_destroyed","ship_hit","ship_destroyed","respawn","turbo_531","turbo_532","turbo_533"}};
class Fixture {
    fs::path parent_,root_;
public:
    Fixture():parent_(fs::canonical(fs::current_path())){
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned n=0;n<64;++n){const auto candidate=parent_/("_space-audio-assets-"+std::to_string(stamp)+"-"+std::to_string(n));if(fs::create_directory(candidate)){root_=fs::canonical(candidate);break;}}
        if(root_.empty())throw std::runtime_error("Cannot create private audio-assets fixture");
        const float vertices[3][8]={{0,0,0,0,0,1,0,0},{1,0,0,0,0,1,1,0},{0,1,0,0,0,1,0,1}};const std::uint32_t indices[3]={0,1,2};
        binary("vertices.bin",vertices,sizeof vertices);binary("indices.bin",indices,sizeof indices);
        binary("white.rgba",std::vector<unsigned char>{255,255,255,255});binary("font.rgba.bin",std::vector<unsigned char>{0,0,0,0});
        binary("space_sky.rgba.bin",std::vector<unsigned char>(2048u*1024u*4u));binary("radar.rgba.bin",std::vector<unsigned char>(128u*128u*4u));
        text("font.tsv","1 1\n65 0 0 0 1\n97 0 0 0 1\n");
        std::string meshes;for(const char* id:{"planet_novalis","planet_kerwan","planet_eudora","ship_531","ship_532","ship_533"})meshes+=std::string(id)+" vertices.bin indices.bin white.rgba 1 1\n";
        text("assets.tsv",meshes);pcm();
    }
    ~Fixture(){if(root_.empty())return;try{
        const auto resolved=fs::canonical(root_);const auto status=fs::symlink_status(root_);
        if(!fs::is_directory(status)||fs::is_symlink(status)||resolved.parent_path()!=parent_||resolved.filename().string().rfind("_space-audio-assets-",0)!=0)
            throw std::runtime_error("Refusing unsafe fixture cleanup target");
        fs::remove_all(resolved);
    }catch(const std::exception& e){std::fprintf(stderr,"Retained owned fixture %s: %s\n",root_.string().c_str(),e.what());}}
    const fs::path& root()const{return root_;}
    void binary(const std::string& name,const void* data,std::size_t size){
        const fs::path relative(name);if(relative.is_absolute()||relative.has_root_name()||relative.has_root_directory()||name.find("..")!=std::string::npos||name.find(':')!=std::string::npos)throw std::runtime_error("Unsafe fixture writer name");
        std::ofstream out(root_/relative,std::ios::binary|std::ios::trunc);if(!out||!out.write(static_cast<const char*>(data),std::streamsize(size)))throw std::runtime_error("Cannot write owned synthetic bytes");
    }
    void binary(const std::string& name,const std::vector<unsigned char>& bytes){binary(name,bytes.data(),bytes.size());}
    void text(const std::string& name,const std::string& contents){binary(name,contents.data(),contents.size());}
    void pcm(){binary("mono.pcm",std::vector<unsigned char>{0,0,255,127,0,128,52,18});binary("stereo.pcm",std::vector<unsigned char>{0,0,255,127,0,128,52,18,255,255,1,0,2,0,3,0});}
};
std::string manifest(){std::string out="# synthetic S16 schema only\nformat\t1\n";for(auto role:roles)if(std::string(role)!="respawn")out+=std::string("clip ")+role+" mono.pcm 8000 1 4 0 0\n";return out;}
void reject(Fixture& fixture,const std::string& source,const std::string& why){
    fixture.pcm();fixture.text("audio.tsv",source);bool rejected=false;
    try{const auto result=SceneAssets::read(fixture.root());(void)result;}catch(const std::exception&){rejected=true;}
    require(rejected,why+" must fail closed");++rejections;
}
}
int main(){try{
    Fixture fixture;
    {
        const auto legacy=SceneAssets::read(fixture.root());require(!legacy.audio_available,"legacy packs with no audio manifest remain compatible and silent");
        for(const auto& clip:legacy.audio.clips)require(clip.samples.empty(),"legacy resource packs cannot acquire invented audio");
    }
    const auto good=manifest();fixture.text("audio.tsv",good);
    {
        const auto loaded=SceneAssets::read(fixture.root());require(loaded.audio_available,"the13 original required roles activate audio without inventing a respawn cue");
        for(std::size_t i=0;i<loaded.audio.clips.size();++i){const auto& clip=loaded.audio.clips[i];
            if(i==std::size_t(SceneAudioClipId::Respawn)){require(clip.samples.empty(),"optional absent respawn audio remains absent");continue;}
            require(clip.sample_rate==8000&&clip.channels==1&&clip.samples.size()==4,"declared PCM metadata and sample count are preserved");
            require(clip.samples[0]==0&&clip.samples[1]==32767&&clip.samples[2]==-32768&&clip.samples[3]==0x1234,"little-endian signed PCM values retain exact native16-bit sample bytes");
            require(clip.loop_begin_frame==0&&clip.loop_end_frame==0&&clip.gain==1&&clip.pitch==1,"unset loop/gain/pitch metadata uses documented neutral defaults");
        }
    }
    fixture.text("audio.tsv",good+"clip respawn mono.pcm 8000 1 4 0 0\n");
    require(!SceneAssets::read(fixture.root()).audio.clips[std::size_t(SceneAudioClipId::Respawn)].samples.empty(),"an explicitly supplied optional role remains forward-compatible without requiring it in original packs");
    {
        auto stereo=replace(good,"clip music mono.pcm 8000 1 4 0 0","clip music stereo.pcm 96000 2 4 1 3");
        stereo+="gain music .375 # native per-clip gain\npitch music 1.25\ngain engine_531 0\npitch engine_531 .25\ngain turbo_533 4\npitch turbo_533 4\n";
        fixture.text("audio.tsv",stereo);const auto loaded=SceneAssets::read(fixture.root());const auto& music=loaded.audio.clips[0];
        require(music.channels==2&&music.sample_rate==96000&&music.samples.size()==8&&music.loop_begin_frame==1&&music.loop_end_frame==3,"stereo framing and the attack plus exclusive loop interval survive reading unchanged");
        require(music.samples[0]==0&&music.samples[1]==32767&&music.samples[2]==-32768&&music.samples[3]==0x1234&&music.samples[4]==-1,"interleaved stereo channel order is not flattened or swapped");
        require(std::abs(music.gain-.375f)<1e-6f&&music.pitch==1.25f,"source-defined gain and base pitch reach the adapter without resampling");
        require(loaded.audio.clips[1].gain==0&&loaded.audio.clips[1].pitch==.25f&&loaded.audio.clips[13].gain==4&&loaded.audio.clips[13].pitch==4,"allowed SDK gain and pitch interval endpoints are accepted exactly");
    }
    const std::vector<std::pair<std::string,std::string>> invalid={
        {"unsupported schema",replace(good,"format\t1","format 2")},
        {"missing schema",replace(good,"format\t1\n","")},
        {"duplicate schema",good+"format 1\n"},
        {"trailing schema fields",replace(good,"format\t1","format 1 extra")},
        {"incomplete13-role original catalog",replace(good,"clip turbo_533 mono.pcm 8000 1 4 0 0\n","")},
        {"empty manifest","# no complete catalog\n"},
        {"unknown record",good+"invented_voice 23\n"},
        {"unknown role",replace(good,"clip music ","clip unknown ")},
        {"case-sensitive role",replace(good,"clip music ","clip Music ")},
        {"duplicate clip",good+"clip music mono.pcm 8000 1 4 0 0\n"},
        {"truncated clip",replace(good,"clip music mono.pcm 8000 1 4 0 0","clip music mono.pcm")},
        {"trailing clip fields",replace(good,"clip music mono.pcm 8000 1 4 0 0","clip music mono.pcm 8000 1 4 0 0 extra")},
        {"low sample rate",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 7999 1 4 0 0")},
        {"high sample rate",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 96001 1 4 0 0")},
        {"zero channels",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 0 4 0 0")},
        {"unsupported surround",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 3 4 0 0")},
        {"zero frames",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 0 0 0")},
        {"negative frames",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 -1 0 0")},
        {"duration over600seconds",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 4800001 0 0")},
        {"unbounded PCM size request",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 96000 2 18446744073709551615 0 0")},
        {"overflowing frame integer",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 96000 2 18446744073709551616 0 0")},
        {"short mono payload",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 5 0 0")},
        {"long mono payload",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 3 0 0")},
        {"stereo byte-length mismatch",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 2 4 0 0")},
        {"reversed loop",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 4 3 1")},
        {"empty loop",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 4 2 2")},
        {"loop after sample",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 4 4 0")},
        {"loop end beyond sample",replace(good,"mono.pcm 8000 1 4 0 0","mono.pcm 8000 1 4 0 5")},
        {"negative gain",good+"gain music -.001\n"},
        {"excessive gain",good+"gain music 4.001\n"},
        {"NaN gain",good+"gain music nan\n"},
        {"infinite gain",good+"gain music inf\n"},
        {"duplicate gain",good+"gain music .5\ngain music .7\n"},
        {"unknown gain role",good+"gain unknown .5\n"},
        {"truncated gain",good+"gain music\n"},
        {"trailing gain fields",good+"gain music .5 extra\n"},
        {"zero pitch",good+"pitch music 0\n"},
        {"low pitch",good+"pitch music .249\n"},
        {"high pitch",good+"pitch music 4.001\n"},
        {"NaN pitch",good+"pitch music nan\n"},
        {"infinite pitch",good+"pitch music inf\n"},
        {"duplicate pitch",good+"pitch music 1\npitch music 2\n"},
        {"unknown pitch role",good+"pitch unknown 1\n"},
        {"truncated pitch",good+"pitch music\n"},
        {"trailing pitch fields",good+"pitch music 1 extra\n"},
        {"missing PCM payload",replace(good,"clip music mono.pcm","clip music absent.pcm")}
    };
    for(const auto& test:invalid)reject(fixture,test.second,test.first);
    for(const std::string& path:{"../outside.pcm","..\\outside.pcm","/outside.pcm","C:outside.pcm","C:/outside.pcm","C:\\outside.pcm","stream:ads","//server/share/file.pcm","\\outside.pcm","\\\\server\\share\\file.pcm"})
        reject(fixture,replace(good,"clip music mono.pcm","clip music "+path),"unsafe audio path "+path);
    fixture.pcm();fixture.text("audio.tsv",good);fixture.binary("mono.pcm",std::vector<unsigned char>{0,1,2,3,4,5,6});bool odd=false;
    try{const auto ignored=SceneAssets::read(fixture.root());(void)ignored;}catch(const std::exception&){odd=true;}
    require(odd,"odd-byte S16 payload is refused rather than truncating its last sample");
    std::printf("audio_assets: %u reader checks and%u malformed catalogs rejected\n",checks,rejections);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}

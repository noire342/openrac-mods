// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_asset_bundle.hpp"
#include "common/json.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using revolverator::NativeAssetBundle;
using openrac::json::Value;
namespace {
int checks;
void require(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"asset bundle: %s\n",why);std::exit(1);}}
std::string utf8(const fs::path& path){const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
std::string read(const fs::path& path){std::ifstream f(path,std::ios::binary);require(bool(f),"fixture/staged file is readable");return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
Value parse(const std::string& text){Value result;std::string error;require(openrac::json::parse(text,result,error),"JSON parses independently through the upstream reader");return result;}
Value read_json(const fs::path& path){return parse(read(path));}
bool same(const Value& a,const Value& b){
    if(a.type()!=b.type())return false;
    switch(a.type()){
    case Value::Type::Null:return true;
    case Value::Type::Bool:return a.boolean()==b.boolean();
    case Value::Type::Number:return std::bit_cast<std::uint64_t>(a.number())==std::bit_cast<std::uint64_t>(b.number());
    case Value::Type::String:return a.string()==b.string();
    case Value::Type::Array:
        if(a.items().size()!=b.items().size())return false;
        for(std::size_t i=0;i<a.items().size();++i)if(!same(a[i],b[i]))return false;
        return true;
    case Value::Type::Object:
        if(a.members().size()!=b.members().size())return false;
        for(const auto& [key,value]:a.members()){
            const auto found=b.members().find(key);
            if(found==b.members().end()||!same(value,found->second))return false;
        }
        return true;
    }
    return false;
}
std::string name(unsigned level){return "level_"+(level<10?std::string("0"):std::string{})+std::to_string(level);}
std::string manifest(unsigned level,const char* version="ntsc"){
    return "{\"format\":3,\"game\":\"rac1\",\"version\":\""+std::string(version)+"\",\"level\":"+std::to_string(level)+R"JSON(,
      "terrain":"terrain.glb","collision":"collision.glb","sky":{"mesh":"sky.glb","background":[0,0.25,-0.0]},
      "classes":{"tie":{"998":{"mesh":null}},"shrub":{},"moby":{
        "0":{"mesh":"ratchet.glb","extra":true},
        "304":{"mesh":"mobys/moby_304.glb","scale":0.12,"name":"gold_weapon_case"},
        "180":{"mesh":"mobys/moby_180.glb","scale":0.1458333283662796,"name":"walloper",
               "matrix":[1,0,0,1],"future":{"enabled":false,"label":"\u00e8\ud83d\ude80","offset":1e-100}},
        "225":{"mesh":"mobys/moby_225.glb","scale":0.027870021760463715,"name":"mine_glove_ammo",
               "materials":["silver",null],"future":{"custom":9007199254740991}}}},
      "future":{"null":null,"bool":[true,false],"empty":{},"text":"quote\" slash\\ tab\t newline\n"}}
    )JSON";
}
struct Fixture {
    fs::path root,levels,mod,cache;
    std::vector<fs::path> files,dirs;
    std::map<fs::path,std::string> originals;
    explicit Fixture(const char* version="ntsc"){
        const auto parent=fs::canonical(fs::temp_directory_path());
        const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
        bool created=false;
        for(unsigned i=0;i<64;++i){root=parent/("openrac-asset-bundle-test-"+std::to_string(tick)+"-"+std::to_string(i));if(fs::create_directory(root)){created=true;break;}}
        require(created,"exclusive fixture directory exists");dirs.push_back(root);
        levels=root/"levels with spaces";mod=root/fs::path(u8"plugin \u00e8");cache=root/"cache";
        directory(levels);directory(mod);directory(cache);
        write(mod/"assets/ammunition/model.glb","authored ammunition glb");
        write(mod/"assets/ammunition/texture.png","authored ammunition texture");
        for(unsigned i=0;i<19;++i){
            const auto native=levels/name(i)/"manifest.json";source(native,manifest(i,version));
            write(mod/"assets/graphics"/name(i)/"mobys/moby_2017.glb","drill model");
            write(mod/"assets/graphics"/name(i)/"textures/moby_revolverator_original.png","drill texture");
            write(mod/"assets/graphics"/name(i)/"mobys/moby_2018.glb","golden drill model");
            write(mod/"assets/graphics"/name(i)/"textures/moby_revolverator_gold.png","golden drill texture");
            if(i==1)write(mod/"assets/graphics"/name(i)/"mobys/moby_2019.glb","static golden drill model");
        }
    }
    void directory(const fs::path& path){
        if(fs::is_directory(path))return;
        directory(path.parent_path());require(fs::create_directory(path),"fixture subdirectory created");dirs.push_back(path);
    }
    void write(const fs::path& path,const std::string& text){
        directory(path.parent_path());
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;require(bool(out),"fixture writes successfully");
    }
    void source(const fs::path& path,const std::string& text){write(path,text);originals[path]=text;}
    void replace(unsigned level,const std::string& from,const std::string& to){
        const auto path=levels/name(level)/"manifest.json";auto text=read(path);const auto at=text.find(from);
        require(at!=std::string::npos,"mutation addresses its intended source field");text.replace(at,from.size(),to);source(path,text);
    }
    void unchanged()const{for(const auto& [path,text]:originals)require(read(path)==text,"every original manifest remains byte-identical");}
    NativeAssetBundle::Paths paths()const{return {levels,mod,cache};}
    ~Fixture(){
        // Every entry was created by this fixture; no recursive deletion and
        // no source/player directory is ever enumerated for cleanup.
        std::error_code error;
        for(auto i=files.rbegin();i!=files.rend();++i)fs::remove(*i,error);
        for(auto i=dirs.rbegin();i!=dirs.rend();++i)fs::remove(*i,error);
    }
};
template<class Function>void rejects(Function action,const char* why){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,why);}
void rejects_stage(Fixture& fixture,const char* version="ntsc"){
    rejects([&]{NativeAssetBundle bundle(fixture.paths(),0,version);},"unsafe/missing input is rejected before publishing any overlay");
    require(fs::is_empty(fixture.cache),"validation failure leaves no staged manifests or session");fixture.unchanged();
}
void serializer_checks(){
    const auto original=parse(R"JSON({"null":null,"bools":[true,false],"numbers":[0,-0.0,0.1458333283662796,1e-100,5e-324,1.7976931348623157e308],"empty":[{},[],""],"key\"\\\n":"\b\f\r\n\t\u0000\u001f\u00e8\ud83d\ude80"})JSON");
    const auto text=revolverator::asset_bundle::serialize_json(original);
    require(same(original,parse(text)),"all six JSON kinds, exact doubles, UTF-8 and escaped controls round-trip");
    require(text.find("\\u0000")!=std::string::npos&&text.find("\\u001f")!=std::string::npos,"embedded NUL/control bytes become valid JSON escapes");
    require(revolverator::asset_bundle::serialize_json(parse(text))==text,"serialization is deterministic after round-trip");
    const auto overflow=parse("1e999");
    rejects([&]{(void)revolverator::asset_bundle::serialize_json(overflow);},"non-finite parsed numbers cannot produce invalid JSON");
}
void command_line_checks(){
    using revolverator::asset_bundle::levels_from_arguments;
    const auto data=fs::absolute("data with spaces"),first=fs::absolute("first levels"),last=fs::absolute("last levels");
    require(levels_from_arguments({"game","--data",data})==data/"port/ntsc","data fallback matches native NTSC layout");
    require(levels_from_arguments({"game","--data",data,"--levels",first,"--levels",last})==last,"last explicit levels wins");
    require(levels_from_arguments({"game","--levels",first,"--data",data})==first,"data ordering cannot override explicit levels");
    require(levels_from_arguments({"game","--mods","--levels","--data",data})==data/"port/ntsc","another option's value is not mistaken for an option");
    require(levels_from_arguments({"game"},data)==data/"port/ntsc","native environment data fallback is supported");
    require(levels_from_arguments({"game","--data",data},{},"pal")==data/"port/pal","PAL data fallback mounts port/pal");
    require(levels_from_arguments({"game"},data,"pal")==data/"port/pal","PAL environment fallback mounts port/pal");
    require(levels_from_arguments({"game","--data",data,"--levels",last},{},"pal")==last,"explicit PAL levels retain native precedence");
    for(const char* version:std::array<const char*,5>{{nullptr,"","PAL","pal_v1","jp"}})
        rejects([&]{(void)levels_from_arguments({"game","--levels",last},data,version);},"unknown disc versions fail even with explicit level roots");
    rejects([&]{(void)levels_from_arguments({"game","--levels"});},"missing native option value fails closed");
    rejects([&]{(void)levels_from_arguments({"game"});},"absent native data and levels never guess a source path");
}
void staging_checks(const char* version="ntsc"){
    Fixture fixture(version);fs::path config,session;
    {
        NativeAssetBundle bundle(fixture.paths(),0,version);config=bundle.configuration();session=config.parent_path();
        require(config.is_absolute()&&session.parent_path()==fs::canonical(fixture.cache),"configuration lives only under owned cache");
        const auto aliases=read_json(config);
        require(aliases["format"].number()==1&&aliases["aliases"].items().size()==19,"overlay has exactly19 format1 aliases");
        require(std::distance(fs::directory_iterator(session),fs::directory_iterator{})==20,"only19 manifests and the configuration are created");
        for(unsigned i=0;i<19;++i){
            const auto& alias=aliases["aliases"][i];
            require(alias["original"].string()==utf8(fixture.levels/name(i)/"manifest.json"),"alias retains the requested absolute native manifest name");
            const auto replacement=fs::path(std::u8string(reinterpret_cast<const char8_t*>(alias["replacement"].string().data()),alias["replacement"].string().size()));
            require(replacement==session/(name(i)+".json"),"each alias targets only its owned staged file");
            const auto before=read_json(fixture.levels/name(i)/"manifest.json"),after=read_json(replacement);
            for(const auto& [key,value]:before.members())if(key!="classes")require(same(value,after[key]),"unknown/native top-level fields are preserved");
            for(const auto& [key,value]:before["classes"].members())if(key!="moby")require(same(value,after["classes"][key]),"tie/shrub/unknown class groups are preserved");
            const auto& original=before["classes"]["moby"];const auto& extended=after["classes"]["moby"];
            require(extended.members().size()==original.members().size()+4+(i==1?1:0),"four visual classes plus one static Novalis display exist");
            for(const auto& [key,value]:original.members())require(same(value,extended[key]),"all original classes including Walloper/ammo remain unchanged");
            for(const auto* id:{"2017","2018","2016","32000"}){
                const auto& native=original[std::string(id)=="2017"||std::string(id)=="2018"?"180":"225"];
                for(const auto& [key,value]:native.members())if(key!="mesh"&&key!="name")require(same(value,extended[id][key]),"clones preserve scale, material, matrix and future properties");
                require(extended[id].members().size()==native.members().size(),"clones override only mesh/name");
            }
            require(extended["2017"]["mesh"].string()==utf8(fixture.mod/"assets/graphics"/name(i)/"mobys/moby_2017.glb"),"per-level drill mesh path is absolute and Unicode-safe");
            require(extended["2018"]["mesh"].string()==utf8(fixture.mod/"assets/graphics"/name(i)/"mobys/moby_2018.glb"),"Golden uses a separate mesh path with the same native animation source");
            if(i==1){
                require(extended["2019"]["scale"].number()==original["304"]["scale"].number(),"static display preserves its own case actor metadata");
                require(extended["2019"]["mesh"].string()==utf8(fixture.mod/"assets/graphics"/name(i)/"mobys/moby_2019.glb"),"static display uses the independent no-skin model");
            }
            require(extended["2016"]["mesh"].string()==utf8(fixture.mod/"assets/ammunition/model.glb")&&extended["32000"]["mesh"].string()==extended["2016"]["mesh"].string(),"both ammo clones use the packaged cartridge mesh");
        }
        fixture.unchanged();
        auto second=std::make_unique<NativeAssetBundle>(fixture.paths(),0,version);const auto second_config=second->configuration();
        require(second_config.parent_path()!=session,"same-process instances reserve separate sessions");
        second.reset();require(!fs::exists(second_config.parent_path())&&fs::exists(config),"one destructor cannot remove another live session");
    }
    require(!fs::exists(config)&&!fs::exists(session),"owned files and now-empty unique directory are cleaned up");fixture.unchanged();
    fs::path foreign,with_foreign;
    {
        NativeAssetBundle bundle(fixture.paths(),0,version);with_foreign=bundle.configuration().parent_path();
        fixture.dirs.push_back(with_foreign);foreign=with_foreign/"foreign-do-not-delete.txt";fixture.write(foreign,"external owner");
    }
    require(fs::exists(foreign)&&read(foreign)=="external owner","destructor preserves unowned files and their directory");
    require(std::distance(fs::directory_iterator(with_foreign),fs::directory_iterator{})==1,"owned cleanup still removes all20 owned files around a foreign entry");
}
void rejection_checks(){
    for(const auto* id:{"2016","2017","2018","2019","32000"}){
        Fixture f;f.replace(18,"\"moby\":{","\"moby\":{\""+std::string(id)+"\":null,");rejects_stage(f);
    }
    for(const auto* id:{"180","225"}){
        Fixture f;f.replace(18,"\""+std::string(id)+"\":{","\"9999\":{");rejects_stage(f);
    }
    {Fixture f;f.replace(18,"\"version\":\"ntsc\"","\"version\":\"pal\"");rejects_stage(f);}
    {Fixture f;f.replace(18,"\"game\":\"rac1\"","\"game\":\"rac2\"");rejects_stage(f);}
    {Fixture f;f.replace(18,"\"level\":18","\"level\":17");rejects_stage(f);}
    {Fixture f;f.replace(18,"\"format\":3","\"format\":2");rejects_stage(f);}
    {Fixture f;rejects([&]{NativeAssetBundle bundle(f.paths(),4);},"a new client must not silently stage old unreadable levels");
     require(fs::is_empty(f.cache),"runtime format mismatch is rejected before file publication");f.unchanged();}
    {Fixture f;for(unsigned i=0;i<19;++i)f.replace(i,"\"format\":3","\"format\":4");
     NativeAssetBundle bundle(f.paths(),4);const auto aliases=read_json(bundle.configuration());
     for(unsigned i=0;i<19;++i){const auto native=read_json(f.levels/name(i)/"manifest.json");
      const auto staged=read_json(bundle.configuration().parent_path()/(name(i)+".json"));
      require(staged["format"].number()==4,"all0.10 staged levels retain format4");
      for(const auto& [key,value]:native.members())if(key!="classes")require(same(value,staged[key]),"format4 native metadata remains intact");
      require(same(native["classes"]["moby"]["180"],staged["classes"]["moby"]["180"]),"native Walloper metadata is preserved on0.10");}
     f.unchanged();}
    {Fixture f;f.source(f.levels/name(18)/"manifest.json","{invalid json");rejects_stage(f);}
    {Fixture f;fs::remove(f.mod/"assets/graphics/level_18/mobys/moby_2017.glb");rejects_stage(f);}
    {Fixture f;fs::remove(f.mod/"assets/graphics/level_18/mobys/moby_2018.glb");rejects_stage(f);}
    {Fixture f;fs::remove(f.mod/"assets/graphics/level_18/textures/moby_revolverator_gold.png");rejects_stage(f);}
    {Fixture f;fs::remove(f.mod/"assets/graphics/level_01/mobys/moby_2019.glb");rejects_stage(f);}
    {Fixture f;fs::remove(f.mod/"assets/ammunition/texture.png");rejects_stage(f);}
    {Fixture f;auto paths=f.paths();paths.cache_root=f.levels/"must-not-create";
     rejects([&]{NativeAssetBundle bundle(paths);},"cache cannot write into original levels");require(!fs::exists(paths.cache_root),"source cache rejection happens before directory creation");f.unchanged();}
}
void pal_checks(){
    {Fixture f("pal");rejects_stage(f);}
    {Fixture f;rejects_stage(f,"pal");}
    {Fixture f("pal");f.replace(18,"\"version\":\"pal\"","\"version\":\"ntsc\"");rejects_stage(f,"pal");}
    {Fixture f("pal");for(const char* version:std::array<const char*,5>{{nullptr,"","PAL","pal_v1","jp"}})rejects_stage(f,version);}
    {Fixture f("pal");for(unsigned i=0;i<19;++i)f.replace(i,"\"format\":3","\"format\":4");
     NativeAssetBundle bundle(f.paths(),4,"pal");const auto aliases=read_json(bundle.configuration());
     require(aliases["aliases"].items().size()==19,"PAL stages exactly its19 gameplay manifests");
     for(unsigned i=0;i<19;++i){const auto native=read_json(f.levels/name(i)/"manifest.json");
      const auto staged=read_json(bundle.configuration().parent_path()/(name(i)+".json"));
      require(staged["format"].number()==4&&staged["version"].string()=="pal","PAL format4 and disc identity survive staging");
      for(const auto& [key,value]:native.members())if(key!="classes")require(same(value,staged[key]),"PAL non-class metadata is unchanged");
      for(const auto& [key,value]:native["classes"]["moby"].members())require(same(value,staged["classes"]["moby"][key]),"PAL original classes remain unchanged");}
     f.unchanged();}
}
}
int main(){serializer_checks();command_line_checks();staging_checks();staging_checks("pal");rejection_checks();pal_checks();std::printf("native asset bundle: %d checks passed\n",checks);}

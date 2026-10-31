// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_asset_bundle.hpp"
#include "common/json.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace revolverator {
namespace {
namespace fs=std::filesystem;
using Value=openrac::json::Value;
using Path=fs::path;

std::string utf8(const Path& path){
    const auto text=path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()),text.size()};
}
Path absolute_path(const Path& path){
    if(path.empty())throw std::runtime_error("asset bundle: an input path is empty");
    return fs::absolute(path).lexically_normal();
}
class JsonWriter {
public:
    void string(std::string_view text){
        out_+='"';
        constexpr char hex[]="0123456789abcdef";
        for(const unsigned char c:text){
            switch(c){
            case '"':out_+="\\\"";break;
            case '\\':out_+="\\\\";break;
            case '\b':out_+="\\b";break;
            case '\f':out_+="\\f";break;
            case '\n':out_+="\\n";break;
            case '\r':out_+="\\r";break;
            case '\t':out_+="\\t";break;
            default:
                if(c<0x20){out_+="\\u00";out_+=hex[c>>4];out_+=hex[c&15];}
                else out_+=static_cast<char>(c);
            }
        }
        out_+='"';
    }
    void value(const Value& input){
        switch(input.type()){
        case Value::Type::Null:out_+="null";break;
        case Value::Type::Bool:out_+=input.boolean()?"true":"false";break;
        case Value::Type::Number:{
            const double number=input.number();
            if(!std::isfinite(number))throw std::runtime_error("asset bundle: non-finite JSON number");
            std::array<char,64> buffer{};
            const auto result=std::to_chars(buffer.data(),buffer.data()+buffer.size(),number,
                std::chars_format::general,std::numeric_limits<double>::max_digits10);
            if(result.ec!=std::errc{})throw std::runtime_error("asset bundle: cannot serialize JSON number");
            out_.append(buffer.data(),result.ptr);break;
        }
        case Value::Type::String:string(input.string());break;
        case Value::Type::Array:{
            out_+='[';bool first=true;
            for(const auto& item:input.items()){separator(first);value(item);}
            out_+=']';break;
        }
        case Value::Type::Object:object(input,{});break;
        }
    }
    // The upstream tree is immutable. Overrides contain already serialized
    // values; existing fields are retained unless explicitly named here.
    void object(const Value& input,const std::map<std::string,std::string>& overrides){
        if(!input.is_object())throw std::runtime_error("asset bundle: expected a JSON object");
        out_+='{';bool first=true;
        for(const auto& [key,child]:input.members()){
            separator(first);string(key);out_+=':';
            const auto replacement=overrides.find(key);
            if(replacement==overrides.end())value(child);else out_+=replacement->second;
        }
        for(const auto& [key,serialized]:overrides){
            if(input.members().find(key)!=input.members().end())continue;
            separator(first);string(key);out_+=':';out_+=serialized;
        }
        out_+='}';
    }
    std::string take(){return std::move(out_);}
private:
    void separator(bool& first){if(!first)out_+=',';first=false;}
    std::string out_;
};
std::string json_quote(std::string_view text){JsonWriter writer;writer.string(text);return writer.take();}
std::string patched(const Value& value,const std::map<std::string,std::string>& overrides){
    JsonWriter writer;writer.object(value,overrides);return writer.take();
}
std::string read_file(const Path& path){
    // Read through std::filesystem::path so Windows keeps UTF-16 filenames.
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("asset bundle: cannot read "+utf8(path));
    std::string text{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    if(file.bad())throw std::runtime_error("asset bundle: failed reading "+utf8(path));
    return text;
}
Value document(const Path& path){
    Value result;std::string error;
    if(!openrac::json::parse(read_file(path),result,error))
        throw std::runtime_error("asset bundle: invalid JSON "+utf8(path)+": "+error);
    return result;
}
void require_file(const Path& path){
    if(!fs::is_regular_file(path))throw std::runtime_error("asset bundle: missing file "+utf8(path));
}
std::string level_name(unsigned level){return "level_"+(level<10?std::string("0"):std::string{})+std::to_string(level);}
std::string disc_version(const char* version){
    if(!version||(std::string_view(version)!="ntsc"&&std::string_view(version)!="pal"))
        throw std::runtime_error("asset bundle: only PAL and NTSC-U disc versions are supported");
    return version;
}
std::string manifest_json(const Value& root,unsigned level,const Path& drill,const Path& golden,const Path& ammunition,const std::string& version){
    if(!root.is_object()||!root["format"].is_number()||
       (root["format"].number()!=3&&root["format"].number()!=4)||
       !root["game"].is_string()||root["game"].string()!="rac1"||
       !root["version"].is_string()||root["version"].string()!=version||
       !root["level"].is_number()||root["level"].number()!=double(level))
        throw std::runtime_error("asset bundle: expected R&C1 "+version+" format3/4 "+level_name(level));
    const auto& classes=root["classes"];const auto& mobys=classes["moby"];
    if(!classes.is_object()||!mobys.is_object()||!mobys["180"].is_object()||!mobys["225"].is_object())
        throw std::runtime_error("asset bundle: missing native class180/225 in "+level_name(level));
    for(const auto* key:{"2016","2017","2018","2019","32000"})
        if(mobys.members().find(key)!=mobys.members().end())
            throw std::runtime_error("asset bundle: custom class collision "+std::string(key)+" in "+level_name(level));
    const auto clone=[&](const Value& native,const Path& mesh,const char* name){
        return patched(native,{{"mesh",json_quote(utf8(mesh))},{"name",json_quote(name)}});
    };
    std::map<std::string,std::string> additions{
        {"2017",clone(mobys["180"],drill,"revolverator_independent_weapon")},
        {"2018",clone(mobys["180"],golden,"revolverator_golden_weapon")},
        {"2016",clone(mobys["225"],ammunition,"revolverator_ammunition_vendor_preview")},
        {"32000",clone(mobys["225"],ammunition,"revolverator_ammunition")}};
    if(level==1){
        if(!mobys["304"].is_object())throw std::runtime_error("asset bundle: Novalis Gold case304 is missing");
        additions.emplace("2019",clone(mobys["304"],golden.parent_path()/"moby_2019.glb","revolverator_golden_shop_display"));
    }
    const auto added=patched(mobys,additions);
    return patched(root,{{"classes",patched(classes,{{"moby",added}})}})+'\n';
}
bool component_equal(const Path& a,const Path& b){
#ifdef _WIN32
    return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;
#else
    return a==b;
#endif
}
bool within(const Path& child,const Path& parent){
    auto c=child.begin();
    for(auto p=parent.begin();p!=parent.end();++p,++c)
        if(c==child.end()||!component_equal(*c,*p))return false;
    return true;
}
bool regular_owned_directory(const Path& path){
    std::error_code error;const auto status=fs::symlink_status(path,error);
    if(error||!fs::is_directory(status)||fs::is_symlink(status))return false;
#ifdef _WIN32
    const DWORD attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0)return false;
#endif
    return true;
}
unsigned long process_id(){
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}
#ifdef _WIN32
Path environment_path(const wchar_t* name){
    const DWORD needed=GetEnvironmentVariableW(name,nullptr,0);if(!needed)return {};
    std::wstring value(needed,L'\0');
    const DWORD written=GetEnvironmentVariableW(name,value.data(),needed);
    if(!written||written>=needed)throw std::runtime_error("asset bundle: environment changed during discovery");
    value.resize(written);return Path(value);
}
Path module_root(){
    static const unsigned char anchor=0;
    HMODULE module{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&anchor),&module))throw std::runtime_error("asset bundle: cannot identify own module");
    std::wstring filename(32768,L'\0');
    const DWORD length=GetModuleFileNameW(module,filename.data(),static_cast<DWORD>(filename.size()));
    if(!length||length>=filename.size())throw std::runtime_error("asset bundle: own module path is unavailable");
    filename.resize(length);Path parent=Path(filename).parent_path();
    for(unsigned depth=0;depth<5&&!parent.empty();++depth){
        const auto manifest=parent/"openrac-mod.json";
        if(fs::is_regular_file(manifest)){
            const auto info=document(manifest);
            if(info["id"].string()=="revolverator"&&fs::is_directory(parent/"assets"))return parent;
        }
        const auto next=parent.parent_path();if(next==parent)break;parent=next;
    }
    throw std::runtime_error("asset bundle: own plugin assets directory was not found");
}
#endif
NativeAssetBundle::Paths process_paths(const char* version){
    (void)disc_version(version);
#ifdef _WIN32
    int count=0;auto** values=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!values)throw std::runtime_error("asset bundle: cannot read process arguments");
    struct Arguments {wchar_t** values;~Arguments(){LocalFree(values);}} guard{values};
    std::vector<Path> args;args.reserve(static_cast<std::size_t>(count));
    for(int i=0;i<count;++i)args.emplace_back(values[i]);
    auto local=environment_path(L"LOCALAPPDATA");
    if(local.empty())throw std::runtime_error("asset bundle: LOCALAPPDATA is unavailable");
    return {asset_bundle::levels_from_arguments(args,environment_path(L"OPENRAC_DATA"),version),module_root(),
        local/"OpenRAC/revolverator/assets"};
#else
    throw std::runtime_error("asset bundle: native module discovery is supported on Windows only");
#endif
}
}

namespace asset_bundle {
std::string serialize_json(const Value& input){JsonWriter writer;writer.value(input);return writer.take();}
fs::path levels_from_arguments(const std::vector<Path>& args,const Path& environment_data,const char* version){
    const auto disc=disc_version(version);
    Path levels,data;
    for(std::size_t i=1;i<args.size();++i){
        const auto& option=args[i];
        const bool target=option==Path("--levels")||option==Path("--data");
        const bool takes_value=target||option==Path("--cards")||option==Path("--mods")||option==Path("--settings")||
            option==Path("--plugins")||option==Path("--probes")||option==Path("--frames")||option==Path("--level")||option==Path("--debug-dump");
        if(!takes_value)continue;
        if(i+1>=args.size())throw std::runtime_error("asset bundle: missing native command-line value");
        const auto& value=args[++i];
        if(target&&value.empty())throw std::runtime_error("asset bundle: empty native data/levels path");
        if(option==Path("--levels"))levels=value;
        else if(option==Path("--data"))data=value;
    }
    if(!levels.empty())return absolute_path(levels);
    if(data.empty())data=environment_data;
    if(data.empty())throw std::runtime_error("asset bundle: neither --levels nor --data is available");
    return absolute_path(data/"port"/disc);
}
}

NativeAssetBundle::NativeAssetBundle(unsigned required_format,const char* version):NativeAssetBundle(process_paths(version),required_format,version){}
NativeAssetBundle::NativeAssetBundle(Paths paths,unsigned required_format,const char* version){
    const auto disc=disc_version(version);
    const auto levels=absolute_path(paths.levels_root),mod=absolute_path(paths.mod_root);
    if(!fs::is_directory(levels)||!fs::is_directory(mod))throw std::runtime_error("asset bundle: source roots are unavailable");
    const auto cache=fs::weakly_canonical(absolute_path(paths.cache_root));
    if(within(cache,fs::canonical(levels))||within(cache,fs::canonical(mod)))
        throw std::runtime_error("asset bundle: cache must be outside original levels and plugin assets");
    const auto ammo=mod/"assets/ammunition/model.glb";
    require_file(ammo);require_file(mod/"assets/ammunition/texture.png");
    struct Staged {Path original;std::string text;};
    std::vector<Staged> manifests;manifests.reserve(19);
    for(unsigned level=0;level<19;++level){
        const auto name=level_name(level);
        const auto art=mod/"assets/graphics"/name;
        const auto drill=art/"mobys/moby_2017.glb";
        const auto golden=art/"mobys/moby_2018.glb";
        require_file(drill);require_file(art/"textures/moby_revolverator_original.png");
        require_file(golden);require_file(art/"textures/moby_revolverator_gold.png");
        if(level==1)require_file(art/"mobys/moby_2019.glb");
        const auto original=levels/name/"manifest.json";
        require_file(original);
        const auto native=document(original);
        if(required_format&&(!native["format"].is_number()||native["format"].number()!=double(required_format)))
            throw std::runtime_error("Revolverator: this OpenRAC client requires level format "+
                std::to_string(required_format)+". Use Check and play in the official launcher to read the levels again.");
        manifests.push_back({original,manifest_json(native,level,drill,golden,ammo,disc)});
    }
    files_.reserve(20);
    try{
        fs::create_directories(cache);cache_root_=fs::canonical(cache);
        static std::atomic<std::uint64_t> sequence{0};
        const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;attempt<64&&!owns_session_;++attempt){
            const auto id=sequence.fetch_add(1,std::memory_order_relaxed);
            session_=cache_root_/("session"+std::to_string(process_id())+"-"+std::to_string(tick)+"-"+std::to_string(id));
            std::error_code error;owns_session_=fs::create_directory(session_,error);
            if(error)throw std::system_error(error,"asset bundle: cannot create owned session");
        }
        if(!owns_session_)throw std::runtime_error("asset bundle: cannot reserve a unique session");
        std::string aliases="{\"format\":1,\"aliases\":[";
        for(unsigned level=0;level<19;++level){
            const auto replacement=session_/(level_name(level)+".json");
            write_owned(replacement,manifests[level].text);
            if(level)aliases+=',';
            aliases+="{\"original\":"+json_quote(utf8(manifests[level].original))+",\"replacement\":"+json_quote(utf8(replacement))+"}";
        }
        aliases+="]}\n";
        const auto config=session_/"file-overlay.json";write_owned(config,aliases);configuration_=config;
    }catch(...){cleanup();throw;}
}
void NativeAssetBundle::write_owned(Path path,const std::string& text){
#ifdef _WIN32
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"asset bundle: cannot create staged file");
    struct Close {HANDLE file;~Close(){CloseHandle(file);}} close{file};
    files_.push_back(std::move(path));
    for(std::size_t at=0;at<text.size();){
        const auto count=static_cast<DWORD>(std::min<std::size_t>(text.size()-at,std::numeric_limits<DWORD>::max()));
        DWORD written=0;
        if(!WriteFile(file,text.data()+at,count,&written,nullptr)||!written)
            throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"asset bundle: failed writing staged file");
        at+=written;
    }
#else
    const int file=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(file<0)throw std::system_error(errno,std::generic_category(),"asset bundle: cannot create staged file");
    struct Close {int file;~Close(){::close(file);}} close{file};
    files_.push_back(std::move(path));
    for(std::size_t at=0;at<text.size();){
        const auto written=write(file,text.data()+at,text.size()-at);
        if(written<0&&errno==EINTR)continue;
        if(written<=0)throw std::system_error(errno,std::generic_category(),"asset bundle: failed writing staged file");
        at+=static_cast<std::size_t>(written);
    }
#endif
}
NativeAssetBundle::~NativeAssetBundle()noexcept{cleanup();}
void NativeAssetBundle::cleanup()noexcept{
    if(!owns_session_)return;
    try{
        std::error_code error;
        if(session_.parent_path()!=cache_root_||!regular_owned_directory(session_)||fs::canonical(session_,error)!=session_||error)return;
        for(const auto& path:files_){
            if(path.parent_path()!=session_)continue;
            const auto status=fs::symlink_status(path,error);
            if(!error&&fs::is_regular_file(status)&&!fs::is_symlink(status))fs::remove(path,error);
            error.clear();
        }
        fs::remove(session_,error); // Fails safely if any foreign entry remains.
    }catch(...){}
    owns_session_=false;
}
}

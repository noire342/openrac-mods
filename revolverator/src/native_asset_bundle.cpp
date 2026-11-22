// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_asset_bundle.hpp"
#include "common/json.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
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
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#ifdef __APPLE__
#include "darwin_process_paths.hpp"
#else
extern char** environ;
#endif
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
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << std::setprecision(std::numeric_limits<double>::max_digits10) << number;
            if(!text)throw std::runtime_error("asset bundle: cannot serialize JSON number");
            out_+=text.str();break;
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
std::map<std::string,std::string> class_additions(const Value& root,unsigned level,const Path& drill,
        const Path& golden,const Path& ammunition,const std::string& version,bool use_openrac_assets){
    if(!root.is_object()||!root["format"].is_number()||
       (use_openrac_assets?root["format"].number()<4:
        (root["format"].number()!=3&&root["format"].number()!=4))||
       !root["game"].is_string()||root["game"].string()!="rac1"||
       !root["version"].is_string()||root["version"].string()!=version||
       !root["level"].is_number()||root["level"].number()!=double(level))
        throw std::runtime_error("asset bundle: expected R&C1 "+version+" extracted manifest for "+level_name(level));
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
    return additions;
}
std::string manifest_json(const Value& root,const std::map<std::string,std::string>& additions){
    const auto& classes=root["classes"];
    const auto added=patched(classes["moby"],additions);
    return patched(root,{{"classes",patched(classes,{{"moby",added}})}})+'\n';
}
std::string classes_json(const std::map<std::string,std::string>& additions){
    std::string text="{\"moby\":{";
    bool first=true;
    for(const auto& [id,entry]:additions){
        if(!first)text+=',';
        first=false;
        text+=json_quote(id)+':'+entry;
    }
    return text+"}}\n";
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
#ifndef _WIN32
struct FileLock {
    int fd=-1;
    FileLock(const FileLock&)=delete;
    FileLock& operator=(const FileLock&)=delete;
    FileLock()=default;
    ~FileLock(){if(fd>=0)close(fd);}
    void acquire(const Path& path,int mode,const char* reason){
        fd=open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
        if(fd<0)throw std::runtime_error(reason);
        int result;
        do{result=flock(fd,mode);}while(result<0&&errno==EINTR);
        if(result<0)throw std::runtime_error(reason);
    }
};
#endif
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
Path module_path(const void* anchor){
    HMODULE module{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(anchor),&module))throw std::runtime_error("asset bundle: cannot identify own module");
    std::wstring filename(32768,L'\0');
    const DWORD length=GetModuleFileNameW(module,filename.data(),static_cast<DWORD>(filename.size()));
    if(!length||length>=filename.size())throw std::runtime_error("asset bundle: own module path is unavailable");
    filename.resize(length);return Path(filename);
}
#else
Path environment_path(const char* name){
    const char* value=std::getenv(name);
    return value&&*value?Path(value):Path{};
}
Path module_path(const void* anchor){
    Dl_info image{};
    if(!dladdr(anchor,&image)||!image.dli_fname)throw std::runtime_error("asset bundle: cannot identify own module");
    return fs::canonical(image.dli_fname);
}
#endif
Path module_root(){
    static const unsigned char anchor=0;
    auto parent=module_path(&anchor).parent_path();
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
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
Path find_python(Path executable){
    for(auto parent=executable.parent_path();!parent.empty();parent=parent.parent_path()){
        const auto candidate=parent/"python/bin/python3";
        if(access(candidate.c_str(),X_OK)==0&&fs::is_regular_file(candidate)&&fs::is_regular_file(parent/"tools/openrac.py"))
            return candidate;
        if(parent==parent.parent_path())break;
    }
#ifndef __APPLE__
    const Path system_python="/usr/bin/python3";
    if(access(system_python.c_str(),X_OK)==0&&fs::is_regular_file(system_python))return system_python;
#endif
    throw std::runtime_error("Revolverator: the OpenRAC launcher's Python is required to derive art from your disc.");
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
#elif defined(__linux__) && !defined(__ANDROID__)
    std::ifstream file("/proc/self/cmdline",std::ios::binary);
    std::vector<Path> args;
    for(std::string value;std::getline(file,value,'\0');)args.emplace_back(value);
    auto cache=environment_path("XDG_CACHE_HOME");
    if(!cache.is_absolute()){
        const auto home=environment_path("HOME");
        if(!home.is_absolute())throw std::runtime_error("asset bundle: no usable home or XDG cache path");
        cache=home/".cache";
    }
    return {asset_bundle::levels_from_arguments(args,environment_path("OPENRAC_DATA"),version),module_root(),
        cache/"openrac/revolverator/assets",find_python(fs::read_symlink("/proc/self/exe")),true};
#elif defined(__APPLE__)
    const auto home=environment_path("HOME");
    if(!home.is_absolute())throw std::runtime_error("asset bundle: no usable macOS home path");
    return {asset_bundle::levels_from_arguments(darwin_process_arguments(),environment_path("OPENRAC_DATA"),version),
        module_root(),home/"Library/Caches/OpenRAC/revolverator/assets",find_python(darwin_executable_path()),true};
#else
    throw std::runtime_error("asset bundle: native module discovery is unavailable on this platform");
#endif
}

// Quote one argument using the inverse of CommandLineToArgvW. No shell or PATH
// lookup is involved: only Python from the running official client's own tree
// may execute our shipped, standard-library-only preparation script.
#ifdef _WIN32
std::wstring quoted_argument(const Path& argument){
    std::wstring result=L"\"";unsigned slashes=0;
    for(const wchar_t c:argument.native()){
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;slashes=0;continue;}
        result.append(slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'\"';return result;
}
Path official_python(){
    std::wstring text(32768,L'\0');const auto count=GetModuleFileNameW(nullptr,text.data(),DWORD(text.size()));
    if(!count||count>=text.size())throw std::runtime_error("asset bundle: running client path is unavailable");
    text.resize(count);auto parent=Path(text).parent_path();
    for(unsigned i=0;i<7&&!parent.empty();++i){
        const auto python=parent/"python/python.exe";
        if(fs::is_regular_file(python))return python;
        const auto next=parent.parent_path();if(next==parent)break;parent=next;
    }
    throw std::runtime_error("Revolverator: the official client's bundled Python was not found. Update OpenRAC through its launcher.");
}
#endif
Path prepared_graphics(const Path& levels,const Path& mod,const Path& cache,const char* version,
        const Path& interpreter,bool use_openrac_assets){
    // Explicit private companion packs remain supported for old QA fixtures.
    // Public packages omit this directory and always derive from local data.
    if(fs::is_directory(mod/"assets/graphics"))return mod/"assets/graphics";
    const auto script=mod/"tools/prepare_runtime_assets.py";require_file(script);
    require_file(mod/"tools/build_model_pack.py");require_file(mod/"tools/build_golden_assets.py");
    require_file(mod/"tools/build_tip_glow.py");require_file(mod/"tools/build_vent_glow.py");
    fs::create_directories(cache);
    const auto stamp=std::to_string(process_id())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto result=cache/("prepared-"+stamp+".txt"),log=cache/("prepare-"+stamp+".log");
#ifdef _WIN32
    (void)use_openrac_assets;
    const auto python=interpreter.empty()?official_python():absolute_path(interpreter);require_file(python);
    const std::vector<Path> arguments{python,"-I",script,"--levels",levels,"--cache",cache/"derived","--version",version,"--result",result};
    std::wstring command;
    for(const auto& argument:arguments){if(!command.empty())command+=L' ';command+=quoted_argument(argument);}
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    const HANDLE output=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    const HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE||input==INVALID_HANDLE_VALUE){
        if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
        throw std::runtime_error("asset bundle: cannot create local preparation diagnostics");
    }
    STARTUPINFOW startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input;startup.hStdOutput=output;startup.hStdError=output;
    PROCESS_INFORMATION process{};
    const BOOL started=CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
    CloseHandle(output);CloseHandle(input);
    if(!started)throw std::runtime_error("Revolverator: could not start automatic local asset preparation. See "+utf8(log));
    CloseHandle(process.hThread);
    const auto waited=WaitForSingleObject(process.hProcess,120000);
    DWORD code=1;
    if(waited==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
    else{TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);}
    CloseHandle(process.hProcess);
    if(waited!=WAIT_OBJECT_0||code!=0||!fs::is_regular_file(result))
        throw std::runtime_error("Revolverator: local asset preparation failed. Use Check and play in the official launcher; diagnostics: "+utf8(log));
#else
    const auto python=absolute_path(interpreter);require_file(python);
    FileLock cache_lock;
    cache_lock.acquire(cache/".prepare.lock",LOCK_EX,"asset bundle: cannot lock local preparation cache");
    std::vector<std::string> arguments{python.string(),"-B",
#ifdef __ANDROID__
        "-P","-s",  // Keep the launcher's PYTHONHOME for Android's stdlib.
#else
        "-I",
#endif
        script.string(),"--levels",levels.string(),
        "--cache",(cache/"derived").string(),"--version",version,"--result",result.string()};
    if(use_openrac_assets)arguments.emplace_back("--portable-assets");
    std::vector<char*> argv;
    for(auto& arg:arguments)argv.push_back(arg.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions{};
    if(posix_spawn_file_actions_init(&actions)!=0)throw std::runtime_error("asset bundle: cannot initialize preparation process");
    struct SpawnActions {posix_spawn_file_actions_t& actions;~SpawnActions(){posix_spawn_file_actions_destroy(&actions);}} scoped{actions};
    if(posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0)!=0||
       posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,log.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600)!=0||
       posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO)!=0)
        throw std::runtime_error("asset bundle: cannot set up local preparation diagnostics");
    pid_t process{};
#ifdef __APPLE__
    auto** environment=*_NSGetEnviron();
#else
    auto** environment=environ;
#endif
    if(posix_spawn(&process,python.c_str(),&actions,nullptr,argv.data(),environment)!=0)
        throw std::runtime_error("Revolverator: could not start local asset preparation. Diagnostics: "+utf8(log));
    int status{};
    pid_t waited{};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(2);
    do{
        waited=waitpid(process,&status,WNOHANG);
        if(waited!=0&&!(waited<0&&errno==EINTR))break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }while(std::chrono::steady_clock::now()<deadline);
    if(waited==0||(waited<0&&errno==EINTR)){
        kill(process,SIGKILL);
        while(waitpid(process,&status,0)<0&&errno==EINTR){}
    }
    if(waited!=process||!WIFEXITED(status)||WEXITSTATUS(status)!=0||!fs::is_regular_file(result))
        throw std::runtime_error("Revolverator: local asset preparation failed. Use Check and play in the official launcher; diagnostics: "+utf8(log));
#endif
    const auto derived=fs::u8path(read_file(result));
    std::error_code ignored;fs::remove(result,ignored);
    if(!fs::is_directory(derived)||!within(fs::weakly_canonical(derived),fs::weakly_canonical(cache/"derived")))
        throw std::runtime_error("asset bundle: derived graphics are outside the owned local cache");
    require_file(derived/"READY.json");return derived;
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
    const bool use_openrac_assets=paths.use_openrac_assets;
#ifndef _WIN32
    FileLock mount_lock;
    if(use_openrac_assets)mount_lock.acquire(mod/".revolverator-assets.lock",LOCK_EX|LOCK_NB,
        "Revolverator: another OpenRAC session owns this installation's generated assets, or the mod directory is read-only.");
#else
    if(use_openrac_assets)throw std::runtime_error("asset bundle: expected the Windows asset adapter");
#endif
    const auto graphics=prepared_graphics(levels,mod,cache,version,paths.preparation_python,use_openrac_assets);
    const bool derived=graphics!=mod/"assets/graphics";
    if(derived)glow_root_="generated-glow/"+utf8(graphics.filename());
    struct Staged {Path original;std::string text;};
    std::vector<Staged> manifests;manifests.reserve(19);
    for(unsigned level=0;level<19;++level){
        const auto name=level_name(level);
        const auto art=graphics/name;
        const auto drill=art/"mobys/moby_2017.glb";
        const auto golden=art/"mobys/moby_2018.glb";
        require_file(drill);require_file(art/(derived?"textures/base.png":"textures/moby_revolverator_original.png"));
        require_file(golden);require_file(art/(derived?"textures/gold.png":"textures/moby_revolverator_gold.png"));
        if(level==1)require_file(art/"mobys/moby_2019.glb");
        const auto original=levels/name/"manifest.json";
        require_file(original);
        const auto native=document(original);
        if(required_format&&!use_openrac_assets&&(!native["format"].is_number()||native["format"].number()!=double(required_format)))
            throw std::runtime_error("Revolverator: this OpenRAC client requires level format "+
                std::to_string(required_format)+". Use Check and play in the official launcher to read the levels again.");
        const auto additions=class_additions(native,level,drill,golden,ammo,disc,use_openrac_assets);
        manifests.push_back({original,use_openrac_assets?classes_json(additions):manifest_json(native,additions)});
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
#ifndef _WIN32
        if(use_openrac_assets){
            level_directories_.reserve(manifests.size());
            for(unsigned level=0;level<19;++level){
                const auto directory=session_/level_name(level);
                level_directories_.push_back(directory);
                fs::create_directory(directory);
                write_owned(directory/"classes.json",manifests[level].text);
            }
            const auto link=mod/"generated-levels";
            const auto status=fs::symlink_status(link);
            if(fs::is_symlink(status)){
                const auto target=fs::read_symlink(link);
                if(target.parent_path()!=cache_root_||!target.filename().string().starts_with("session"))
                    throw std::runtime_error("asset bundle: generated-levels belongs to another installation");
                fs::remove(link);
            }else if(fs::is_directory(status)&&fs::is_empty(link))fs::remove(link);
            asset_link_=link;
            fs::create_directory_symlink(session_,link);
            asset_lock_=std::exchange(mount_lock.fd,-1);
            return;
        }
#endif
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
NativeAssetBundle::~NativeAssetBundle()noexcept{
    cleanup();
#ifndef _WIN32
    if(asset_lock_>=0)close(asset_lock_);
#endif
}
void NativeAssetBundle::cleanup()noexcept{
    if(!owns_session_)return;
    try{
        std::error_code error;
#ifndef _WIN32
        if(!asset_link_.empty()&&fs::is_symlink(fs::symlink_status(asset_link_,error))&&
           !error&&fs::read_symlink(asset_link_,error)==session_&&!error)fs::remove(asset_link_,error);
        error.clear();
#endif
        if(session_.parent_path()!=cache_root_||!regular_owned_directory(session_)||fs::canonical(session_,error)!=session_||error)return;
        for(const auto& path:files_){
            const auto parent=path.parent_path();
            const bool class_file=parent.parent_path()==session_&&regular_owned_directory(parent)&&path.filename()=="classes.json";
            if(parent!=session_&&!class_file)continue;
            const auto status=fs::symlink_status(path,error);
            if(!error&&fs::is_regular_file(status)&&!fs::is_symlink(status))fs::remove(path,error);
            error.clear();
        }
        for(const auto& directory:level_directories_){
            if(regular_owned_directory(directory))fs::remove(directory,error);
            error.clear();
        }
        fs::remove(session_,error); // Fails safely if any foreign entry remains.
    }catch(...){}
    owns_session_=false;
}
}

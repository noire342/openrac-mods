// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal_save.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace revolverator {
namespace {
constexpr unsigned campaign_generation = 0x15ee20;
using Path=std::filesystem::path;
std::vector<Path> arguments(){
    std::vector<Path> result;
#ifdef _WIN32
    int count=0;auto** values=CommandLineToArgvW(GetCommandLineW(),&count);
    if(values){for(int i=0;i<count;++i)result.emplace_back(values[i]);LocalFree(values);}
#elif defined(__linux__)
    std::ifstream file("/proc/self/cmdline",std::ios::binary);std::string value;
    while(std::getline(file,value,'\0'))result.emplace_back(value);
#endif
    return result;
}
Path environment_path(const char* name){
#ifdef _WIN32
    std::wstring wide;for(const char* p=name;*p;++p)wide+=wchar_t(*p);
    const auto size=GetEnvironmentVariableW(wide.c_str(),nullptr,0);if(!size)return {};
    std::wstring value(size,L'\0');GetEnvironmentVariableW(wide.c_str(),value.data(),size);value.resize(size-1);return Path(value);
#else
    const auto* value=std::getenv(name);return value?Path(value):Path{};
#endif
}
Path default_card_root(const char* serial){
#ifdef _WIN32
    auto root=environment_path("LOCALAPPDATA");
    if(root.empty())root=environment_path("USERPROFILE")/"AppData/Local";
    return root/"openrac/memcard"/serial;
#elif defined(__APPLE__)
    return environment_path("HOME")/"Library/Application Support/OpenRAC/memcard"/serial;
#else
    auto root=environment_path("XDG_DATA_HOME");
    if(root.empty())root=environment_path("HOME")/".local/share";
    return root/"openrac/memcard"/serial;
#endif
}
}
NativeSaveFields::NativeSaveFields(const openrac_api* api, const arsenal::Registry& registry)
    : Memory(api), level_(api->loaded_level()), campaign_generation_(read<unsigned>(global(campaign_generation))) {
    const auto favourites = global(0x141ea0);
    const auto& logical = registry.quick();
    for (unsigned slot = 0; slot < logical.size(); ++slot) {
        const auto* descriptor = registry.descriptor(logical[slot]);
        if (descriptor && registry.owned(logical[slot])) {
            mask(favourites + slot * sizeof(int), int(descriptor->carrier), 0);
        }
    }
    const auto equipped = registry.equipped();
    if (const auto* descriptor = registry.descriptor(equipped)) {
        mask(global(0x141660), int(descriptor->carrier), int(arsenal::NativeWrenchKey));
    }
}
void NativeSaveFields::mask(unsigned address, int original, int replacement) {
    if (!valid(address, sizeof(int)) || read<int>(address) != original) return;
    words_[count_++] = {address, original, replacement};
    write(address, replacement);
}
NativeSaveFields::~NativeSaveFields() {
    if (campaign_transition_ || api_->loaded_level() != level_) return;
    for (unsigned i = 0; i < count_; ++i) {
        const auto& word = words_[i];
        if (read<int>(word.address) == word.masked) write(word.address, word.original);
    }
}
bool NativeSaveFields::accept_campaign_transition() {
    // The native challenge-mode reset advances this counter after rebuilding
    // stock equipment and favourites, before queuing any memory-card write.
    campaign_transition_ = read<unsigned>(global(campaign_generation)) == campaign_generation_ + 1u;
    return campaign_transition_;
}

NativeArsenalSave::NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal)
    :Memory(api),arsenal_(arsenal),mod_(mod),card_root_(default_card_root(bindings().card_serial())){
    const auto args=arguments();
    for(std::size_t i=1;i<args.size();++i){
        if(args[i]==Path("--no-card"))no_card_=true;
        if(args[i]==Path("--cards")&&i+1<args.size())card_root_=args[++i];
    }
    if(api->data_dir){
        if(const char* root=api->data_dir(mod);root&&*root)
            mod_data_=std::filesystem::u8path(root);
    }
    migrate_legacy();
}
NativeArsenalSave::NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal,Paths paths)
    :Memory(api),arsenal_(arsenal),mod_(mod),card_root_(std::move(paths.card_root)),mod_data_(std::move(paths.mod_data)),no_card_(paths.no_card){
    if(card_root_.empty()&&!no_card_)throw std::invalid_argument("arsenal save: an explicit card root must not be empty");
    migrate_legacy();
}
void NativeArsenalSave::migrate_legacy(){
    if(no_card_)return;
    // Run during mod load, before the title menu enumerates the card folder.
    for(int port=0;port<2;++port){
        for(int slot=0;slot<5;++slot){
            const auto native=save_path(port,slot);
            const auto status=arsenal::migrate_legacy_sidecar(native,arsenal::sidecar_path(native,mod_data_));
            if(status!=arsenal::StoreStatus::Missing)report("migration",status);
        }
    }
}
std::filesystem::path NativeArsenalSave::save_path(int port,int slot)const{
    auto root=card_root_;if(port)root+=Path(".slot2");
    return root/bindings().card_directory()/("save"+std::to_string(slot)+".bin");
}
std::filesystem::path NativeArsenalSave::active_save_path()const{
    if(no_card_)return {};
    const auto& layout=bindings().memcard();const auto state=native_state();
    // Both native drivers expose one card descriptor. active_card chooses that
    // descriptor (0 or -1); its port chooses the physical first/second card.
    // Physical port1 does not mean indexing past card[0] by card_stride.
    if(read<int>(state+layout.active_card)!=0)return {};
    const int port=read<int>(state+layout.port),slot=read<int>(state+layout.save_index);
    if(port<0||port>1||slot<0||slot>4)return {};
    return save_path(port,slot);
}
void NativeArsenalSave::report(const char* operation,arsenal::StoreStatus status)const{
    char text[192];std::snprintf(text,sizeof text,"arsenal %s: %s",operation,arsenal::store_status_message(status));
    api_->log(mod_,status==arsenal::StoreStatus::Ok?OPENRAC_LOG_INFO:OPENRAC_LOG_WARN,text);
}
arsenal::Snapshot NativeArsenalSave::vanilla_snapshot()const{
    arsenal::Snapshot state;state.walloper=arsenal_.native_walloper();
    for(unsigned i=0;i<8;++i){const int id=read<int>(global(0x141ea0)+i*4);if(id>=0&&id<=36)state.quick[i]=unsigned(id);}
    const int held=read<int>(global(0x141660));state.equipped=held>0&&held<=36?unsigned(held):8;
    if(state.equipped==18&&!state.walloper.owned)state.equipped=8;
    return state;
}
void NativeArsenalSave::begin_save(){
    // Native autosave declines to serialize while another operation is queued.
    // Preserve the snapshot belonging to that write instead of pairing newer
    // logical inventory with the older native packet.
    const auto& layout=bindings().memcard();
    const int operation=read<int>(native_state()+layout.operation),queued=read<int>(native_state()+layout.pending_operation);
    if(pending_||operation>=3||queued>=0){captured_.reset();return;}
    arsenal_.commit();captured_=arsenal_.registry().snapshot();
}
void NativeArsenalSave::cancel_prepare() {
    captured_.reset();
}
void NativeArsenalSave::after_prepare(){
    const auto& layout=bindings().memcard();
    const int operation=read<int>(native_state()+layout.operation),pending=read<int>(native_state()+layout.pending_operation);
    if(!captured_||!((operation>=15&&operation<=20)||(pending>=15&&pending<=20)))return;
    const auto path=active_save_path();
    if(!path.empty()){pending_=std::move(captured_);pending_path_=path;}
    captured_.reset();
}
void NativeArsenalSave::after_io(){
    if(!pending_)return;
    const auto& layout=bindings().memcard();
    const int operation=read<int>(native_state()+layout.operation),pending=read<int>(native_state()+layout.pending_operation),error=read<int>(native_state()+layout.error);
    if(operation>=3||pending>=0)return;
    if(error==0){
        arsenal::StoreStatus status;
        arsenal::save(pending_path_,arsenal::sidecar_path(pending_path_,mod_data_),*pending_,&status);
        report("save",status);
    }
    pending_.reset();pending_path_.clear();
}
void NativeArsenalSave::import_native_state(){
    arsenal_.registry().set_walloper(arsenal_.native_walloper());
}
void NativeArsenalSave::after_restore_data(unsigned descriptors){
    if(descriptors!=global(0x1a04c0))return;
    const auto& layout=bindings().memcard();
    const int operation=read<int>(native_state()+layout.operation),pending=read<int>(native_state()+layout.pending_operation);
    if(operation!=13&&operation!=14&&pending!=13&&pending!=14)return;
    auto state=vanilla_snapshot();const auto native_walloper=state.walloper;
    const auto path=active_save_path();arsenal::StoreStatus status=arsenal::StoreStatus::Missing;
    if(!path.empty()&&arsenal::load(path,arsenal::sidecar_path(path,mod_data_),state,&status))
        state.walloper=native_walloper;
    if(!arsenal_.registry().restore(state)){
        state=vanilla_snapshot();arsenal_.registry().restore(state);status=arsenal::StoreStatus::InvalidSnapshot;
    }
    arsenal_.mark_snapshot_restored();
    if(status!=arsenal::StoreStatus::Missing)report("load",status);
}
void NativeArsenalSave::reset_campaign(bool preserve_custom){
    if(preserve_custom){import_native_state();return;}
    captured_.reset();pending_.reset();pending_path_.clear();
    const auto state=vanilla_snapshot();arsenal_.registry().restore(state);arsenal_.mark_snapshot_restored();
}

bool NativeArsenalSave::merge_new_game_plus() {
    const auto previous = arsenal_.registry().snapshot();
    auto next = vanilla_snapshot();
    next.records = previous.records;
    for (unsigned slot = 0; slot < next.quick.size(); ++slot) {
        const auto key = previous.quick[slot];
        if (!next.quick[slot] && arsenal_.registry().descriptor(key) && arsenal_.registry().owned(key)) {
            next.quick[slot] = key;
        }
    }
    if (!arsenal_.registry().restore(next)) return false;
    arsenal_.mark_snapshot_restored();
    if (captured_) captured_ = std::move(next);
    return true;
}
}

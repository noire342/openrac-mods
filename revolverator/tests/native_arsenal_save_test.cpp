// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_arsenal_save.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

namespace fs=std::filesystem;
using namespace revolverator;
namespace {
unsigned checks{};
void require(bool good,const char* why){++checks;if(!good){std::fprintf(stderr,"native arsenal save: %s\n",why);std::exit(1);}}
struct View final:ArsenalIdentityView {void select_identity(bool)override{}};
std::vector<unsigned char> read_file(const fs::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
struct Fixture;
Fixture* live{};
struct Fixture {
    const RegionBindings& region;
    fs::path root;
    std::vector<fs::path> files,dirs;
    std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
    openrac_api api{};arsenal::Registry registry;View view;
    std::unique_ptr<NativeArsenal> arsenal;
    unsigned logs{};
    explicit Fixture(const char* version):region(*RegionBindings::for_version(version)){
        const auto temp=fs::canonical(fs::temp_directory_path());
        const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<64;++i){root=temp/("openrac-native-save-test-"+std::to_string(tick)+"-"+std::to_string(i));if(fs::create_directory(root))break;}
        require(fs::is_directory(root),"an isolated owned card root exists");dirs.push_back(root);
        live=this;api.game="rac1";api.version=version;api.memory=ram.data();
        api.relocate_data=+[](unsigned address){return address;};
        api.relocate_level_data=+[](int,unsigned address){return address;};
        api.loaded_level=+[]{return 1;};
        api.log=+[](openrac_mod*,int,const char*){++live->logs;};
        require(registry.register_weapon({arsenal::RevolveratorKey,"noire342.revolverator",18,60037,20,15000,12}),"register the logical weapon");
        arsenal=std::make_unique<NativeArsenal>(&api,registry,view);
        put<unsigned char>(region.data(0x13d4c0)+18,1);
        // This canonical array is native skill-point progress, not weapon unlocks.
        put<unsigned char>(region.data(0x13d408)+18,7);
        put<unsigned char>(region.data(0x13d4e8)+18,2);
        put<int>(region.data(0x13d428)+18*4,0);
        put<unsigned char>(region.data(0x13e520)+18,3);
        registry.set_walloper(arsenal->native_walloper());
        put(region.data(0x141ea0),std::array<unsigned,8>{18,11,0,0,0,0,0,0});put<int>(region.data(0x141660),18);
        state(0,-1,0);put<int>(region.data(0x13d290)+region.memcard().active_card,0);put<int>(region.data(0x13d290),0);put<int>(region.data(0x13d290)+0x14,1);
    }
    template<class T>void put(unsigned at,const T& value){std::memcpy(ram.data()+at,&value,sizeof value);}
    void state(int operation,int queued,int error){const auto at=region.data(0x13d290);const auto& layout=region.memcard();put(at+layout.operation,operation);put(at+layout.pending_operation,queued);put(at+layout.error,error);}
    void directory(const fs::path& path){if(fs::is_directory(path))return;directory(path.parent_path());require(fs::create_directory(path),"create owned card directory");dirs.push_back(path);}
    fs::path native(int port=0,int slot=1){auto card=root;if(port)card+=fs::path(".slot2");return card/region.card_directory()/("save"+std::to_string(slot)+".bin");}
    void create_native(int port=0,int slot=1){
        const auto path=native(port,slot);directory(path.parent_path());
        std::array<unsigned char,184> bytes{};bytes[0]=16;bytes[4]=8;bytes[20]=0x5a;
        std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"write isolated native-prefix fixture");
        files.push_back(path);files.push_back(arsenal::sidecar_path(path));
    }
    void grant(unsigned ammo){require(registry.set_state(arsenal::RevolveratorKey,{true,ammo,39}),"set logical ammo");require(registry.equip(arsenal::RevolveratorKey),"equip logical identity");require(registry.assign_quick(1,arsenal::RevolveratorKey),"assign a logical favourite");}
    arsenal::Snapshot load(int port=0,int slot=1){arsenal::Snapshot value;arsenal::StoreStatus status;require(arsenal::load(native(port,slot),value,&status)&&status==arsenal::StoreStatus::Ok,"read a matching sidecar");return value;}
    static unsigned ammo(const arsenal::Snapshot& snapshot){for(const auto& record:snapshot.records)if(record.key==arsenal::RevolveratorKey)return record.state.ammo;return 0;}
    ~Fixture(){arsenal.reset();std::error_code error;for(auto i=files.rbegin();i!=files.rend();++i)fs::remove(*i,error);for(auto i=dirs.rbegin();i!=dirs.rend();++i)fs::remove(*i,error);}
};
void regional_save_checks(const char* version){
    Fixture f(version);f.create_native();const auto original=read_file(f.native());f.grant(7);
    NativeArsenalSave save(&f.api,nullptr,*f.arsenal,{f.root,false});
    save.begin_save();f.state(15,-1,0);save.after_prepare();f.grant(3);
    save.begin_save(); // A duplicate request must not replace the snapshot already queued.
    f.state(0,-1,0);save.after_io();
    const auto saved=f.load();require(Fixture::ammo(saved)==7,"queued seven-round snapshot survives a later three-round request");
    require(saved.walloper.unlocked==0,"the legacy codec unlock byte is reserved zero, never skill-point progress");
    require(read_file(f.native())==original,"sidecar persistence never modifies the native save");
    const auto other=f.root/(std::strcmp(version,"pal")==0?"BASCUS-97199RATCHET":"BESCES-50916RATCHET");
    require(!fs::exists(other),"saving never creates the other region's card folder");
    f.registry.set_state(arsenal::RevolveratorKey,{false,0,0});f.registry.equip(8);
    f.state(13,-1,0);save.after_restore_data(f.region.data(0x1a04c0));
    require(f.registry.owned(arsenal::RevolveratorKey)&&f.registry.state(arsenal::RevolveratorKey)->ammo==7,"matching regional load restores logical ownership and ammo");
    require(f.registry.equipped()==arsenal::RevolveratorKey&&f.registry.quick()[1]==arsenal::RevolveratorKey,"matching regional load restores equipment and favourites");
    require(f.registry.walloper().owned&&f.registry.walloper().ammo==0,"native Walloper state remains independent");
    save.reset_campaign(true);require(f.registry.state(arsenal::RevolveratorKey)->ammo==7,"new game plus keeps custom progress");
    save.reset_campaign(false);require(!f.registry.owned(arsenal::RevolveratorKey),"new campaign clears custom progress");
    f.grant(5);f.state(0,-1,0);save.begin_save();f.state(15,-1,0);save.after_prepare();f.state(0,-1,1);save.after_io();
    require(Fixture::ammo(f.load())==7,"failed native IO never overwrites a matching sidecar");
    auto bytes=original;bytes[20]^=1;std::ofstream changed(f.native(),std::ios::binary|std::ios::trunc);changed.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());changed.close();
    f.state(13,-1,0);save.after_restore_data(f.region.data(0x1a04c0));
    require(!f.registry.owned(arsenal::RevolveratorKey),"a stale sidecar cannot attach to different native bytes");
    require(f.ram[f.region.data(0x13d408)+18]==7,"save, restore and new-game scopes preserve the original skill-point byte");
}
void pal_layout_decoys(){
    Fixture f("pal");f.create_native();f.grant(7);
    const auto mc=f.region.data(0x13d290);const auto& layout=f.region.memcard();
    require(layout.operation==0xdc&&layout.pending_operation==0xe4&&layout.error==0xec,"PAL save observes the independently verified driver layout");
    // NTSC operation is PAL busy; a wrong implementation would refuse capture.
    f.put<int>(mc+0xd4,99);
    NativeArsenalSave save(&f.api,nullptr,*f.arsenal,{f.root,false});
    save.begin_save();f.state(19,-1,0);save.after_prepare();f.state(2,-1,0);save.after_io();
    require(Fixture::ammo(f.load())==7,"PAL busy decoy does not replace the operation state");
    f.grant(3);f.state(2,-1,0);save.begin_save();f.state(19,-1,0);save.after_prepare();
    f.state(2,20,0);save.after_io();
    require(Fixture::ammo(f.load())==7,"queued PAL IO is not mistaken for NTSC error or idle state");
    f.state(2,-1,8);save.after_io();
    require(Fixture::ammo(f.load())==7,"actual PAL error atEC prevents a sidecar write");
    for(int bad_active:{-1,1}){
        f.state(2,-1,0);f.put<int>(mc+layout.active_card,bad_active);save.begin_save();f.state(19,-1,0);save.after_prepare();f.state(2,-1,0);save.after_io();
        require(Fixture::ammo(f.load())==7,"absent or out-of-range card selection cannot attach a sidecar");
    }
}
void ports_and_disabling(const char* version){
    Fixture f(version);f.create_native(1,4);f.grant(9);f.put<int>(f.region.data(0x13d290),1);f.put<int>(f.region.data(0x13d290)+0x14,4);
    NativeArsenalSave save(&f.api,nullptr,*f.arsenal,{f.root,false});save.begin_save();f.state(15,-1,0);save.after_prepare();f.state(0,-1,0);save.after_io();
    require(Fixture::ammo(f.load(1,4))==9,"second memory-card port retains the .slot2 suffix and regional directory");
    require(!fs::exists(f.root/f.region.card_directory()),"second-port save does not create a first-port save folder");
    for(int bad_slot:{-1,5}){f.put<int>(f.region.data(0x13d290)+0x14,bad_slot);save.begin_save();f.state(15,-1,0);save.after_prepare();f.state(0,-1,0);save.after_io();}
    require(Fixture::ammo(f.load(1,4))==9,"invalid save slots leave the valid sidecar intact");
    NativeArsenalSave disabled(&f.api,nullptr,*f.arsenal,{{},true});disabled.begin_save();f.state(15,-1,0);disabled.after_prepare();f.state(0,-1,0);disabled.after_io();
    require(Fixture::ammo(f.load(1,4))==9,"no-card mode never writes an existing sidecar");
    bool rejected=false;try{NativeArsenalSave invalid(&f.api,nullptr,*f.arsenal,{{},false});}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"an explicit empty card root is rejected");
}
}
int main(){pal_layout_decoys();for(const char* version:{"ntsc","pal"}){regional_save_checks(version);ports_and_disabling(version);}std::printf("%u native arsenal save checks passed (PAL/NTSC paths, queued saves, load, NG+, stale data, two ports)\n",checks);}

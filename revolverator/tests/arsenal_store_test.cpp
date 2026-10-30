// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arsenal_store.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
using namespace arsenal;
namespace fs=std::filesystem;
namespace {
using Bytes=std::vector<std::uint8_t>;
int checks;
void require(bool good,const char* reason){++checks;if(!good){std::fprintf(stderr,"arsenal store: %s\n",reason);std::exit(1);}}
void write(const fs::path& path,const Bytes& bytes){std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));require(bool(file),"test fixture file writes successfully");}
Bytes read(const fs::path& path){std::ifstream file(path,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());}
void put32(Bytes& bytes,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(value>>(i*8));}
std::uint32_t u32(const Bytes& bytes,std::size_t at){return std::uint32_t(bytes.at(at))|(std::uint32_t(bytes.at(at+1))<<8)|(std::uint32_t(bytes.at(at+2))<<16)|(std::uint32_t(bytes.at(at+3))<<24);}
// Independent table-driven CRC oracle, including the standard known vector.
std::uint32_t reference_crc(const std::uint8_t* bytes,std::size_t size){
    std::array<std::uint32_t,256> table{};
    for(unsigned i=0;i<table.size();++i){auto word=i;for(unsigned bit=0;bit<8;++bit)word=(word&1u)?0xedb88320u^(word>>1):word>>1;table[i]=word;}
    std::uint32_t result=0xffffffffu;for(std::size_t i=0;i<size;++i)result=table[(result^bytes[i])&255]^(result>>8);return ~result;
}
void checksum(Bytes& bytes){put32(bytes,bytes.size()-4,reference_crc(bytes.data(),bytes.size()-4));}
Bytes native_bytes(std::uint32_t main=48,std::uint32_t record=12){
    Bytes bytes(8+main+20*record);put32(bytes,0,main);put32(bytes,4,record);
    for(std::size_t i=8;i<bytes.size();++i)bytes[i]=std::uint8_t(i*17+31);
    return bytes;
}
Snapshot snapshot(){
    Snapshot value;value.walloper={true,7,2,0,4};
    value.records={{RevolveratorKey,"noire342.revolverator",{true,7,39}},
                   {0x20403,"removed.mod.weapon",{true,91,123456}}};
    value.quick[0]=18;value.quick[1]=RevolveratorKey;value.quick[7]=0x20403;value.equipped=RevolveratorKey;return value;
}
bool equal(const Snapshot& a,const Snapshot& b){
    if(a.format!=b.format||a.equipped!=b.equipped||a.quick!=b.quick||a.records.size()!=b.records.size())return false;
    if(a.walloper.owned!=b.walloper.owned||a.walloper.unlocked!=b.walloper.unlocked||a.walloper.acquired!=b.walloper.acquired||
       a.walloper.ammo!=b.walloper.ammo||a.walloper.text_variant!=b.walloper.text_variant)return false;
    for(std::size_t i=0;i<a.records.size();++i){const auto& x=a.records[i];const auto& y=b.records[i];
        if(x.key!=y.key||x.stable_id!=y.stable_id||x.state.owned!=y.state.owned||x.state.ammo!=y.state.ammo||x.state.xp!=y.state.xp)return false;}
    return true;
}
struct TempDirectory {
    fs::path parent,path;
    TempDirectory(){parent=fs::absolute(fs::temp_directory_path());
        const auto clock=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<64;++i){path=parent/("openrac-arsenal-store-test-"+std::to_string(clock)+"-"+std::to_string(i));
            std::error_code error;if(fs::create_directory(path,error))return;}
        require(false,"reserve a fresh private test directory");}
    ~TempDirectory(){
        // No recursive delete: cleanup only the individual files of this newly
        // created, verified private directory. Never enumerate native user data.
        if(path.parent_path()!=parent||path.filename().u8string().rfind("openrac-arsenal-store-test-",0)!=0)return;
        std::error_code ignored;
        for(const auto& entry:fs::directory_iterator(path,ignored))if(!entry.is_directory(ignored))fs::remove(entry.path(),ignored);
        fs::remove(path,ignored);
    }
};
}
int main(){
    require(reference_crc(reinterpret_cast<const std::uint8_t*>("123456789"),9)==0xcbf43926u,"CRC oracle matches the published CRC32 check vector");
    TempDirectory directory;const auto native=directory.path/"save0.bin",sidecar=sidecar_path(native);
    const Bytes original=native_bytes();const Snapshot expected=snapshot();write(native,original);
    require(sidecar.filename()=="save0.bin.arsenal","our path only appends .arsenal to the exact native filename");
    StoreStatus status{};Snapshot loaded;
    require(!load(native,loaded,&status)&&status==StoreStatus::Missing,"an absent sidecar has an explicit non-destructive result");
    require(save(native,expected,&status)&&status==StoreStatus::Ok,"save writes a sidecar for a valid native prefix");
    require(read(native)==original,"saving never modifies native save bytes");
    require(load(native,loaded,&status)&&status==StoreStatus::Ok&&equal(loaded,expected),"codec round trip preserves native flags, both logical favourites and orphan state");
    const Bytes valid_sidecar=read(sidecar);
    require(u32(valid_sidecar,valid_sidecar.size()-4)==reference_crc(valid_sidecar.data(),valid_sidecar.size()-4),"sidecar checksum uses standard CRC32");
    auto invalid=expected;invalid.records[0].state.xp=Registry::MaximumXp+1;
    require(!save(native,invalid,&status)&&status==StoreStatus::InvalidSnapshot&&read(sidecar)==valid_sidecar&&read(native)==original,
            "invalid input cannot overwrite an older valid sidecar or native save");
    const auto reject=[&](Bytes malformed,StoreStatus wanted,const char* reason){write(sidecar,malformed);Snapshot output=expected;
        require(!load(native,output,&status)&&status==wanted&&equal(output,expected),reason);require(read(native)==original,"corrupt sidecar never writes native input");};
    auto bad=valid_sidecar;bad[100]^=0x20;reject(bad,StoreStatus::Corrupt,"CRC mismatch is rejected with unchanged output");
    bad=valid_sidecar;bad[0]^=1;checksum(bad);reject(bad,StoreStatus::Corrupt,"unknown magic is rejected even with a matching CRC");
    bad=valid_sidecar;put32(bad,8,99);checksum(bad);reject(bad,StoreStatus::Corrupt,"unknown format version is rejected");
    bad=valid_sidecar;put32(bad,12,1);checksum(bad);reject(bad,StoreStatus::Corrupt,"payload length cannot hide trailing garbage");
    bad=valid_sidecar;put32(bad,44,257);checksum(bad);reject(bad,StoreStatus::Corrupt,"record count is bounded before parser allocation");
    bad=valid_sidecar;bad[88]=129;bad[89]=0;checksum(bad);reject(bad,StoreStatus::Corrupt,"identity length larger than128 is rejected");
    bad=valid_sidecar;bad[100]=0;checksum(bad);reject(bad,StoreStatus::Corrupt,"embedded NUL identity is rejected");
    bad=valid_sidecar;bad[90]=2;checksum(bad);reject(bad,StoreStatus::Corrupt,"owned field must be canonical boolean, not any nonzero byte");
    bad=valid_sidecar;put32(bad,92,65536);checksum(bad);reject(bad,StoreStatus::Corrupt,"generic orphan ammunition is bounded without a descriptor");
    bad=valid_sidecar;put32(bad,96,Registry::MaximumXp+1);checksum(bad);reject(bad,StoreStatus::Corrupt,"decoded XP is bounded before restoration");
    bad=valid_sidecar;bad.pop_back();reject(bad,StoreStatus::Corrupt,"truncated checksum fails safely");
    bad=valid_sidecar;bad.resize(MaximumSidecarBytes+1);reject(bad,StoreStatus::TooLarge,"oversized sidecar is rejected before its bytes are loaded");
    write(sidecar,valid_sidecar);bad=original;bad[16]^=1;write(native,bad);loaded=expected;
    require(!load(native,loaded,&status)&&status==StoreStatus::Stale&&equal(loaded,expected),"a different native prefix cannot receive an older logical inventory");
    require(read(native)==bad&&read(sidecar)==valid_sidecar,"stale detection writes neither native nor sidecar files");
    write(native,original);require(load(native,loaded,&status)&&equal(loaded,expected),"the matching native prefix restores again without rebuilding the sidecar");
    bad=original;bad.insert(bad.end(),{'o','l','d','-','t','r','a','i','l','e','r'});write(native,bad);
    require(load(native,loaded,&status)&&equal(loaded,expected),"an unrelated native trailer is excluded from the digest");
    require(save(native,expected,&status)&&read(native)==bad,"saving with a native trailer retains the entire original native file");
    bad.back()='X';bad.insert(bad.end(),2048,0xa5);write(native,bad);
    require(load(native,loaded,&status)&&equal(loaded,expected),"changing only native trailer length and bytes does not stale the sidecar");
    write(native,original);const auto saved=read(sidecar);
    for(const auto sizes:std::array<std::array<std::uint32_t,2>,4>{{{{0,1}},{{1,0}},{{0xffffffffu,1}},{{1,0xffffffffu}}}}){
        Bytes broken(8);put32(broken,0,sizes[0]);put32(broken,4,sizes[1]);write(native,broken);
        require(!save(native,expected,&status)&&status==StoreStatus::InvalidNative&&read(native)==broken&&read(sidecar)==saved,
                "invalid native packet sizes never trigger sidecar replacement");}
    write(native,Bytes{1,2,3});require(!save(native,expected,&status)&&status==StoreStatus::InvalidNative,"short native headers are rejected");
    write(native,original);fs::remove(native);loaded=expected;
    require(!load(native,loaded,&status)&&status==StoreStatus::NativeMissing&&equal(loaded,expected),"a sidecar cannot restore without its native save");
    require(!save(native,expected,&status)&&status==StoreStatus::NativeMissing&&read(sidecar)==saved,"saving never creates a missing native save file");
    write(native,original);
    const auto unicode=directory.path/fs::u8path("save_\xc3\xa0_\xe7\x8c\xab.bin");write(unicode,original);
    require(save(unicode,expected,&status)&&load(unicode,loaded,&status)&&equal(loaded,expected)&&read(unicode)==original,
            "native Unicode paths round trip through platform-correct atomic filesystem calls");
    const auto blocked=directory.path/"blocked.bin";write(blocked,original);fs::create_directory(sidecar_path(blocked));
    require(!save(blocked,expected,&status)&&status==StoreStatus::IoError&&read(blocked)==original,
            "failed atomic replacement leaves the native file untouched and reports I/O failure");
    fs::remove(sidecar_path(blocked));
    std::atomic<unsigned> successful{0};std::vector<std::thread> writers;
    for(unsigned i=0;i<4;++i)writers.emplace_back([&,i]{auto value=expected;value.records[0].state.ammo=i+1;if(save(native,value))++successful;});
    for(auto& writer:writers)writer.join();
    require(successful>0&&load(native,loaded,&status)&&loaded.records[0].state.ammo>=1&&loaded.records[0].state.ammo<=4&&read(native)==original,
            "concurrent atomic writers produce one complete valid sidecar without modifying native data");
    bool clean=true;for(const auto& entry:fs::directory_iterator(directory.path))if(entry.path().filename().u8string().find(".tmp.")!=std::string::npos)clean=false;
    require(clean,"successful and failed stores clean up only their own temporary files");
    require(store_status_message(StoreStatus::Stale)[0]&&store_status_message(StoreStatus::IoError)[0],"status messages are stable non-throwing diagnostics");
    std::printf("arsenal store: %d codec, native-digest, atomic-filesystem and failure-isolation checks passed\n",checks);
}

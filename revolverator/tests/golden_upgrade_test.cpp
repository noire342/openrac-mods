// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arsenal_state.hpp"
#include "arsenal_store.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
using namespace arsenal;
namespace fs=std::filesystem;
namespace {
int checks;
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"golden upgrade: %s\n",why);std::exit(1);}}
using Bytes=std::vector<unsigned char>;
Bytes read(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void write(const fs::path& path,const Bytes& bytes){std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));require(bool(f),"fixture writes successfully");}
unsigned u32(const Bytes& bytes,unsigned at){return unsigned(bytes.at(at))|(unsigned(bytes.at(at+1))<<8)|(unsigned(bytes.at(at+2))<<16)|(unsigned(bytes.at(at+3))<<24);}
void put32(Bytes& bytes,unsigned at,unsigned value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=static_cast<unsigned char>(value>>(8*i));}
void crc(Bytes& bytes){unsigned value=0xffffffffu;for(std::size_t i=0;i<bytes.size()-4;++i){value^=bytes[i];for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&(0u-(value&1u)));}put32(bytes,unsigned(bytes.size()-4),~value);}
Descriptor definition(){return {RevolveratorKey,"noire342.revolverator",18,60037,20,15000,12};}
struct Directory {
 fs::path path;
 Directory(){const auto parent=fs::canonical(fs::temp_directory_path());const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
  bool fresh=false;for(unsigned i=0;i<64;++i){path=parent/("revolverator-golden-test-"+std::to_string(tick)+"-"+std::to_string(i));if(fs::create_directory(path)){fresh=true;break;}}require(fresh,"owned temporary fixture reserved");}
 ~Directory(){std::error_code ec;fs::remove(path/"save.bin.arsenal",ec);fs::remove(path/"save.bin",ec);fs::remove(path,ec);}
};
}
int main(){
 Registry registry;require(registry.register_weapon(definition()),"base descriptor registered once");int bolts=100000;
 require(registry.purchase_golden(18,bolts,50000)==GoldenPurchaseResult::UnknownWeapon&&bolts==100000,"native Walloper cannot be upgraded through custom API");
 require(registry.purchase_golden(RevolveratorKey,bolts,50000)==GoldenPurchaseResult::BaseMissing,"Golden requires already owned base weapon");
 require(registry.set_walloper({true,7,2,0,5})&&registry.set_state(RevolveratorKey,{true,7,3456}),"base progress and native Walloper seeded independently");
 registry.assign_quick(0,18);registry.assign_quick(1,RevolveratorKey);registry.equip(RevolveratorKey);
 const auto initial=registry.snapshot();const auto revision=registry.revision();
 int poor=49999;require(registry.purchase_golden(RevolveratorKey,poor,50000)==GoldenPurchaseResult::InsufficientBolts&&poor==49999&&registry.revision()==revision,"insufficient funds fail atomically");
 require(registry.purchase_golden(RevolveratorKey,bolts,0)==GoldenPurchaseResult::InvalidPrice&&registry.purchase_golden(RevolveratorKey,bolts,0xffffffffu)==GoldenPurchaseResult::InvalidPrice,"zero and overflowing prices fail closed");
 require(registry.purchase_golden(RevolveratorKey,bolts,50000)==GoldenPurchaseResult::Purchased&&bolts==50000&&registry.golden(RevolveratorKey),"one purchase sets Golden and spends only ordinary balance");
 const auto upgraded=registry.snapshot();
 require(upgraded.records.size()==initial.records.size()&&registry.definitions().size()==1&&upgraded.equipped==initial.equipped&&upgraded.quick==initial.quick,"upgrade does not add a wheel item or change bindings");
 require(upgraded.records[0].state.ammo==7&&upgraded.records[0].state.xp==3456&&upgraded.walloper.text_variant==5,"ammunition XP and native Gold variant remain untouched");
 const auto bought_revision=registry.revision();
 require(registry.purchase_golden(RevolveratorKey,bolts,50000)==GoldenPurchaseResult::AlreadyOwned&&bolts==50000&&registry.revision()==bought_revision,"replayed input cannot double charge or change progress");
 require(!registry.set_state(RevolveratorKey,{false,7,3456,true})&&registry.golden(RevolveratorKey),"unowned Golden state is invalid rather than partially applied");
 Registry absent;require(absent.restore(upgraded),"unknown custom record retains Golden through absent mod");
 require(absent.snapshot().records[0].state.golden&&absent.register_weapon(definition())&&absent.golden(RevolveratorKey),"reinstallation reactivates Golden without new purchase");
 Directory directory;const auto native=directory.path/"save.bin";
 Bytes native_bytes(8+48+20*12,0x57);put32(native_bytes,0,48);put32(native_bytes,4,12);write(native,native_bytes);
 StoreStatus status{};require(save(native,upgraded,&status),"Golden snapshot writes codec2");
 const auto sidecar=sidecar_path(native);const auto encoded=read(sidecar);
 require(u32(encoded,8)==2&&encoded.at(91)==1,"codec2 uses only the former reserved per-record byte for Golden");
 Snapshot loaded;require(load(native,loaded,&status)&&loaded.records[0].state.golden&&loaded.records[0].state.ammo==7&&loaded.records[0].state.xp==3456,"Golden survives exact sidecar round trip");
 require(read(native)==native_bytes,"native save bytes remain unchanged");
 auto legacy=encoded;put32(legacy,8,1);legacy.at(91)=0;crc(legacy);write(sidecar,legacy);
 require(load(native,loaded,&status)&&!loaded.records[0].state.golden&&loaded.records[0].state.ammo==7&&loaded.records[0].state.xp==3456,"real codec1 layout migrates with Golden false and all progress retained");
 auto unknown=encoded;unknown.at(91)=2;crc(unknown);write(sidecar,unknown);const auto before=loaded;
 require(!load(native,loaded,&status)&&status==StoreStatus::Corrupt&&loaded.records[0].state.golden==before.records[0].state.golden,"unknown flag bits are rejected without mutating loaded state");
 auto invalid_legacy=encoded;put32(invalid_legacy,8,1);crc(invalid_legacy);write(sidecar,invalid_legacy);
 require(!load(native,loaded,&status)&&status==StoreStatus::Corrupt,"codec1 reserved-byte invariant remains strict");
 require(read(native)==native_bytes,"all migration and corruption checks preserve original native bytes");
 std::printf("golden upgrade: %d state, purchase and codec migration checks passed\n",checks);
}

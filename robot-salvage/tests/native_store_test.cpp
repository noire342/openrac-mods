// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_store.hpp"
#include "builtin_catalog.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>
using namespace salvage;
namespace {
unsigned checks;
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"salvage store: %s\n",message);std::exit(1);}}
using Bytes=std::vector<unsigned char>;using Path=std::filesystem::path;
Bytes bytes(const Path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const Path& p,const Bytes& b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));require(bool(f),"test fixture write");}
void put32(Bytes& b,unsigned at,unsigned value){for(unsigned i=0;i<4;++i)b[at+i]=static_cast<unsigned char>(value>>(i*8));}
PartDescriptor descriptor(){PartDescriptor p;p.id=0xfedcba9876543210ull;p.stable_id="rac1.robot.459.fragment.460";p.name_it="Testa robot";p.name_en="Robot head";p.source_it="Robot esploratore";p.source_en="Scout robot";p.source_class=459;p.fragment_class=460;p.model_key=p.stable_id;return p;}
struct Host {
 std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);openrac_api api{};
 static unsigned relocate(unsigned value){return value;}
 static unsigned relocate_level(int,unsigned value){return value;}
 static void log(openrac_mod*,int,const char*){}
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 Host(){api.memory=ram.data();api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.log=log;api.version="pal";put<int>(NativeMemory::Memcard+0xc8,0);put<int>(NativeMemory::Memcard+0xe4,-1);}
};
}
int main(){
 const auto root=std::filesystem::current_path()/("salvage-store-fixture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 require(!std::filesystem::exists(root),"fixture output is new");std::filesystem::create_directories(root);
 struct Cleanup{Path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{root};
 const auto native=root/"save0.bin";Bytes packet(8+32+20*8+16,0);put32(packet,0,32);put32(packet,4,8);for(unsigned i=8;i<packet.size();++i)packet[i]=static_cast<unsigned char>(i);
 write(native,packet);Catalog catalog;const auto desc=descriptor();require(catalog.register_part(desc)==Status::Ok,"valid descriptor");
 const PartId unknown=0xf111222233334444ull;const InventorySnapshot original{{desc.id,7},{unknown,std::numeric_limits<Quantity>::max()-3}};
 StoreStatus status;require(save_inventory(native,catalog,original,&status)&&status==StoreStatus::Ok,"save paired catalog/inventory");
 require(bytes(native)==packet,"save never modifies native packet");
 Catalog restored;InventorySnapshot inventory{{1,1}};require(load_inventory(native,restored,inventory,&status)&&inventory==original,"all64 bits of IDs and counts survive");
 require(restored.find(desc.id)&&restored.find(desc.id)->name_it==desc.name_it&&restored.find(desc.id)->model_key==desc.model_key,"catalog restores recognisable model/name/source independently of current level");
 require(!restored.find(unknown)&&inventory.at(unknown)==std::numeric_limits<Quantity>::max()-3,"unknown optional IDs retained");
 const auto sidecar=salvage_sidecar_path(native);const auto saved=bytes(sidecar);packet.back()^=7;write(native,packet);
 require(load_inventory(native,restored,inventory,&status),"options outside native campaign prefix do not invalidate inventory");
 packet[10]^=4;write(native,packet);const auto before=inventory;
 require(!load_inventory(native,restored,inventory,&status)&&status==StoreStatus::Stale&&inventory==before,"old/mismatched native campaign cannot load newer inventory or mutate output");
 packet[10]^=4;write(native,packet);
 auto corrupt=saved;corrupt[40]^=1;write(sidecar,corrupt);
 Catalog empty;InventorySnapshot unchanged{{42,9007199254740993ull}};
 require(!load_inventory(native,empty,unchanged,&status)&&status==StoreStatus::Corrupt&&empty.entries().empty()&&unchanged.at(42)==9007199254740993ull,"corrupt catalog and inventory load is atomic");
 write(sidecar,Bytes(saved.begin(),saved.begin()+20));require(!load_inventory(native,empty,unchanged,&status)&&status==StoreStatus::Corrupt,"truncated sidecar rejected");
 write(sidecar,saved);require(!load_inventory(root/"missing.bin",empty,unchanged,&status)&&status==StoreStatus::Missing,"missing sidecar is not an I/O corruption");
 const auto denied=root/"denied.bin";write(denied,packet);std::filesystem::create_directory(salvage_sidecar_path(denied));
 require(!save_inventory(denied,catalog,original,&status)&&status==StoreStatus::IoError&&std::filesystem::is_directory(salvage_sidecar_path(denied)),"failed atomic replace preserves existing target");
 for(const auto& entry:std::filesystem::directory_iterator(root))require(entry.path().filename().string().find(".tmp.")==std::string::npos,"failed write cleans only its own temporary");
 require(!save_inventory(native,catalog,InventorySnapshot{{0,2}},&status)&&status==StoreStatus::InvalidSnapshot&&bytes(sidecar)==saved,"invalid snapshot never overwrites verified sidecar");
 {
  Catalog legacy;auto canonical=builtin_part(459,1736);require(bool(canonical),"canonical built-in available for label migration");
  auto old=*canonical;old.name_it="Componente A";old.name_en="Component A";
  old.radius=.73f;old.capsule_half_height=.15f;old.mass=3.25f;legacy.register_part(old);
  auto custom=old;custom.stable_id="custom.mechanic.birdbot.core";custom.id=stable_part_id(custom.stable_id);
  custom.name_it="Nucleo personalizzato";custom.name_en="Custom core";legacy.register_part(custom);
  const auto path=root/"legacy-labels.bin";write(path,packet);const InventorySnapshot original_labels{{old.id,37}};
  require(save_inventory(path,legacy,original_labels,&status),"legacy display labels saved without changing identity");
  Catalog renamed;InventorySnapshot quantities;
  require(load_inventory(path,renamed,quantities,&status)&&renamed.find(old.id)&&renamed.find(old.id)->name_it=="Testa di Birdbot"&&renamed.find(old.id)->name_en=="Birdbot head"&&quantities==original_labels,"generic labels migrate to reviewed names while original model, IDs and quantities survive");
  const auto* restored=renamed.find(old.id);
  require(restored->radius==old.radius&&restored->capsule_half_height==old.capsule_half_height&&restored->mass==old.mass&&restored->model_key==old.model_key&&restored->source_class==old.source_class&&restored->fragment_class==old.fragment_class,"renaming preserves exact saved physical dimensions and original model/source metadata");
  require(renamed.find(custom.id)&&renamed.find(custom.id)->name_it==custom.name_it&&renamed.find(custom.id)->name_en==custom.name_en,"custom producer names sharing an original source/model pair are never migrated");
 }
 {
  Host host;Inventory ledger;Catalog types;require(types.register_part(desc)==Status::Ok,"adapter catalog");require(ledger.restore({{desc.id,3}})==Status::Ok,"adapter starting inventory");
  const auto cards=root/"cards";NativeStore store(&host.api,nullptr,types,ledger,{cards,false});
  host.put<int>(NativeMemory::Memcard+0x14,2);const auto active=cards/"BESCES-50916RATCHET/save2.bin";
  require(store.active_save_path()==active,"PAL physical card/slot path");host.put<int>(NativeMemory::Memcard,1);
  require(store.active_save_path()==Path(cards.string()+".slot2")/"BESCES-50916RATCHET/save2.bin","second physical card uses suffix, not second struct");
  host.put<int>(NativeMemory::Memcard,0);host.put<int>(NativeMemory::Memcard+0xc8,-1);require(store.active_save_path().empty(),"inactive card has no save path");host.put<int>(NativeMemory::Memcard+0xc8,0);
  std::filesystem::create_directories(active.parent_path());write(active,packet);
  store.begin_save();host.put<int>(NativeMemory::Memcard+0xdc,19);store.after_prepare();
  ledger.restore({{desc.id,9}});store.begin_save();host.put<int>(NativeMemory::Memcard+0xdc,0);store.after_io();
  InventorySnapshot captured;Catalog captured_catalog;require(load_inventory(active,captured_catalog,captured,&status)&&captured.at(desc.id)==3,"pending write retains exact pre-serialization snapshot, not newer inventory");
  unsigned restores=0;store.set_restored_callback([&]{++restores;});host.put<int>(NativeMemory::Memcard+0xdc,13);
  store.after_restore_data(0x1a04c0);require(ledger.count(desc.id)==9&&!restores,"NTSC/foreign descriptor cannot trigger PAL restore");
  store.after_restore_data(0x1a05c0);require(ledger.count(desc.id)==3&&restores==1,"PAL explicit restore updates inventory and visit callback");
  host.put<int>(NativeMemory::Memcard+0xdc,0);ledger.restore({{desc.id,6}});store.begin_save();host.put<int>(NativeMemory::Memcard+0xdc,19);store.after_prepare();
  const auto verified=bytes(salvage_sidecar_path(active));host.put<int>(NativeMemory::Memcard+0xdc,0);host.put<int>(NativeMemory::Memcard+0xec,-1);store.after_io();
  require(bytes(salvage_sidecar_path(active))==verified,"native I/O failure never commits sidecar");
  store.reset_campaign(true);require(ledger.count(desc.id)==6&&restores==2,"NG+ preserves inventory but ends visit");
  store.reset_campaign(false);require(ledger.snapshot().empty()&&restores==3,"new campaign clears inventory and visit");
  NativeStore disabled(&host.api,nullptr,types,ledger,{cards,true});require(disabled.active_save_path().empty(),"no-card mode never touches saves");
 }
 {
  Host host;Inventory ledger;Catalog types;types.register_part(desc);
  const auto cards=root/"nested-cards",active=cards/"BESCES-50916RATCHET/save0.bin";
  std::filesystem::create_directories(active.parent_path());write(active,packet);
  NativeStore store(&host.api,nullptr,types,ledger,{cards,false});
  for(bool queued:{false,true}){
   host.put<int>(NativeMemory::Memcard+0xdc,0);host.put<int>(NativeMemory::Memcard+0xe4,-1);
   ledger.restore({{desc.id,3}});store.begin_save();
   host.put<int>(NativeMemory::Memcard+(queued?0xe4:0xdc),19);
   ledger.restore({{desc.id,9}});store.begin_save();store.after_prepare();
   host.put<int>(NativeMemory::Memcard+0xdc,0);host.put<int>(NativeMemory::Memcard+0xe4,-1);store.after_io();
   Catalog captured_catalog;InventorySnapshot captured;
   require(load_inventory(active,captured_catalog,captured,&status)&&captured.at(desc.id)==3,
    queued?"nested serialization with queued save preserves the outer snapshot":"nested serialization while native save is busy preserves the outer snapshot");
  }
  ledger.restore({{desc.id,5}});store.begin_save();store.after_prepare(); // Native request refused: no save state was queued.
  ledger.restore({{desc.id,11}});store.begin_save();host.put<int>(NativeMemory::Memcard+0xe4,19);store.after_prepare();
  ledger.restore({{desc.id,22}});host.put<int>(NativeMemory::Memcard+0xe4,-1);store.after_io();
  Catalog captured_catalog;InventorySnapshot captured;
  require(load_inventory(active,captured_catalog,captured,&status)&&captured.at(desc.id)==11,
   "a refused prepare cannot contaminate the next request with an older captured inventory");
 }
 std::printf("salvage native store: %u checks passed\n",checks);
}

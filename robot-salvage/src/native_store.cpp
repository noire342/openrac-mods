// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_store.hpp"
#include "builtin_catalog.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace salvage {
namespace {
using Path=std::filesystem::path;using Bytes=std::vector<std::uint8_t>;
constexpr std::array<std::uint8_t,8> Magic{{'O','R','S','A','L','V','G',0}};
constexpr std::size_t MaxBytes=64u*1024u*1024u,MaxNativeBytes=4u*1024u*1024u;
struct Fingerprint {std::uint32_t bytes{};std::uint64_t hash{};};
bool finish(StoreStatus value,StoreStatus* output){if(output)*output=value;return value==StoreStatus::Ok;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
void put32(Bytes& out,std::uint32_t value){for(unsigned i=0;i<4;++i)out.push_back(std::uint8_t(value>>(i*8)));}
void put64(Bytes& out,std::uint64_t value){put32(out,std::uint32_t(value));put32(out,std::uint32_t(value>>32));}
void putfloat(Bytes& out,float value){std::uint32_t bits{};std::memcpy(&bits,&value,4);put32(out,bits);}
void putstr(Bytes& out,const std::string& text){put32(out,std::uint32_t(text.size()));out.insert(out.end(),text.begin(),text.end());}
std::uint32_t crc32(const Bytes& bytes,std::size_t size){std::uint32_t c=~0u;for(std::size_t i=0;i<size;++i){c^=bytes[i];for(unsigned k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
StoreStatus failure(const Path& path,StoreStatus missing){std::error_code error;return !std::filesystem::exists(path,error)&&!error?missing:StoreStatus::IoError;}
StoreStatus fingerprint(const Path& path,Fingerprint& result){
 std::ifstream file(path,std::ios::binary);if(!file)return failure(path,StoreStatus::NativeMissing);
 std::array<std::uint8_t,8> header{};file.read(reinterpret_cast<char*>(header.data()),8);
 if(file.gcount()!=8)return StoreStatus::InvalidNative;
 const auto main=u32(header.data()),record=u32(header.data()+4);const std::uint64_t total=8ull+main+20ull*record;
 if(!main||!record||total>MaxNativeBytes)return StoreStatus::InvalidNative;
 file.seekg(0,std::ios::end);const auto size=file.tellg();if(size<0)return StoreStatus::IoError;
 if(std::uint64_t(size)<total)return StoreStatus::InvalidNative;
 file.seekg(0);Bytes bytes(static_cast<std::size_t>(total));file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(total));
 if(file.gcount()!=std::streamsize(total))return StoreStatus::IoError;
 std::uint64_t hash=14695981039346656037ull;for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}
 result={std::uint32_t(total),hash};return StoreStatus::Ok;
}
bool valid_snapshot(const Catalog& catalog,const InventorySnapshot& items){
 Inventory validated;if(validated.restore(items)!=Status::Ok)return false;
 for(const auto& [id,p]:catalog.entries())if(id!=p.id||!id||p.stable_id.empty()||p.stable_id.size()>256||
  p.name_it.size()>512||p.name_en.size()>512||p.source_it.size()>512||p.source_en.size()>512||p.model_key.size()>95)return false;
 return catalog.entries().size()<1000000&&items.size()<1000000;
}
Bytes encode(const Catalog& catalog,const InventorySnapshot& items,const Fingerprint& native){
 Bytes out;out.insert(out.end(),Magic.begin(),Magic.end());put32(out,1);put32(out,native.bytes);put64(out,native.hash);
 put32(out,std::uint32_t(catalog.entries().size()));put32(out,std::uint32_t(items.size()));
 for(const auto& [id,p]:catalog.entries()){
  put64(out,id);putstr(out,p.stable_id);putstr(out,p.name_it);putstr(out,p.name_en);putstr(out,p.source_it);putstr(out,p.source_en);
  put32(out,p.source_class);put32(out,p.fragment_class);putfloat(out,p.radius);putfloat(out,p.capsule_half_height);putfloat(out,p.mass);putstr(out,p.model_key);
 }
 for(const auto& [id,count]:items){put64(out,id);put64(out,count);}
 put32(out,crc32(out,out.size()));return out;
}
struct Reader {
 const Bytes& bytes;std::size_t at=0;
 bool room(std::size_t n)const{return at<=bytes.size()-4&&n<=bytes.size()-4-at;}
 bool get32(std::uint32_t& v){if(!room(4))return false;v=u32(bytes.data()+at);at+=4;return true;}
 bool get64(std::uint64_t& v){std::uint32_t a,b;if(!get32(a)||!get32(b))return false;v=std::uint64_t(a)|(std::uint64_t(b)<<32);return true;}
 bool getfloat(float& v){std::uint32_t bits;if(!get32(bits))return false;std::memcpy(&v,&bits,4);return true;}
 bool getstr(std::string& text,std::size_t maximum){std::uint32_t n;if(!get32(n)||n>maximum||!room(n))return false;text.assign(reinterpret_cast<const char*>(bytes.data()+at),n);at+=n;return text.find('\0')==std::string::npos;}
};
bool decode(const Bytes& bytes,Catalog& catalog,InventorySnapshot& items,Fingerprint& native){
 if(bytes.size()<36||bytes.size()>MaxBytes||!std::equal(Magic.begin(),Magic.end(),bytes.begin())||u32(bytes.data()+bytes.size()-4)!=crc32(bytes,bytes.size()-4))return false;
 Reader read{bytes,8};std::uint32_t version,catalog_count,item_count;
 if(!read.get32(version)||version!=1||!read.get32(native.bytes)||native.bytes<29||native.bytes>MaxNativeBytes||!read.get64(native.hash)||
    !read.get32(catalog_count)||catalog_count>=1000000||!read.get32(item_count)||item_count>=1000000)return false;
 std::set<PartId> seen;
 for(std::uint32_t i=0;i<catalog_count;++i){
  PartDescriptor p;if(!read.get64(p.id)||!p.id||!seen.insert(p.id).second||!read.getstr(p.stable_id,256)||p.stable_id.empty()||
   !read.getstr(p.name_it,512)||!read.getstr(p.name_en,512)||!read.getstr(p.source_it,512)||!read.getstr(p.source_en,512)||
   !read.get32(p.source_class)||!read.get32(p.fragment_class)||!read.getfloat(p.radius)||!read.getfloat(p.capsule_half_height)||!read.getfloat(p.mass)||!read.getstr(p.model_key,95))return false;
  Catalog validator;if(validator.register_part(p)!=Status::Ok)return false;
  // Display labels may evolve without changing item identity or quantities.
  // Migrate only our canonical built-ins; custom mods retain their own names.
  if(const auto canonical=builtin_part(p.source_class,p.fragment_class,p.radius,p.capsule_half_height,p.mass);
     canonical&&canonical->id==p.id&&canonical->stable_id==p.stable_id){p.name_it=canonical->name_it;p.name_en=canonical->name_en;}
  const auto* current=catalog.find(p.id);
  if(current){if(current->stable_id!=p.stable_id)return false;}else if(catalog.register_part(std::move(p))!=Status::Ok)return false;
 }
 for(std::uint32_t i=0;i<item_count;++i){PartId id;Quantity count;if(!read.get64(id)||!read.get64(count)||!id||!count||!items.emplace(id,count).second)return false;}
 return read.at==bytes.size()-4&&valid_snapshot(catalog,items);
}
bool atomic_write(const Path& target,const Bytes& bytes){
 static std::atomic<std::uint64_t> serial{};const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
 for(unsigned attempt=0;attempt<16;++attempt){
  auto temporary=target;temporary+=".tmp."+std::to_string(stamp)+"."+std::to_string(++serial);
#ifdef _WIN32
  HANDLE handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
  if(handle==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_EXISTS)continue;return false;}
  bool ok=true;std::size_t at=0;while(at<bytes.size()){DWORD written=0;if(!WriteFile(handle,bytes.data()+at,DWORD(bytes.size()-at),&written,nullptr)||!written){ok=false;break;}at+=written;}
  if(ok)ok=FlushFileBuffers(handle)!=0;ok=CloseHandle(handle)!=0&&ok;
  if(ok)ok=MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
  if(!ok)DeleteFileW(temporary.c_str());return ok;
#else
  const int fd=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(fd<0){if(errno==EEXIST)continue;return false;}
  bool ok=true;std::size_t at=0;while(at<bytes.size()){const auto n=::write(fd,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=std::size_t(n);}
  if(ok)ok=::fsync(fd)==0;ok=::close(fd)==0&&ok;if(ok)ok=::rename(temporary.c_str(),target.c_str())==0;
  if(!ok)::unlink(temporary.c_str());return ok;
#endif
 }
 return false;
}
std::vector<Path> arguments(){std::vector<Path> result;
#ifdef _WIN32
 int count=0;auto** values=CommandLineToArgvW(GetCommandLineW(),&count);if(values){for(int i=0;i<count;++i)result.emplace_back(values[i]);LocalFree(values);}
#elif defined(__linux__)
 std::ifstream input("/proc/self/cmdline",std::ios::binary);std::string value;while(std::getline(input,value,'\0'))result.emplace_back(value);
#endif
 return result;
}
Path default_root(){
#ifdef _WIN32
 wchar_t* value=nullptr;std::size_t length=0;_wdupenv_s(&value,&length,L"LOCALAPPDATA");const auto root=value&&*value?Path(value)/"openrac/memcard/SCES_509.16":Path{};std::free(value);return root;
#else
 const char* root=std::getenv("XDG_DATA_HOME");if(root&&*root)return Path(root)/"openrac/memcard/SCES_509.16";
 const char* home=std::getenv("HOME");return home&&*home?Path(home)/".local/share/openrac/memcard/SCES_509.16":Path{};
#endif
}
}
const char* store_status_message(StoreStatus status)noexcept{
 switch(status){case StoreStatus::Ok:return "saved/loaded matching inventory";case StoreStatus::Missing:return "no component inventory for this save";
 case StoreStatus::NativeMissing:return "native save is missing";case StoreStatus::InvalidNative:return "native save packet is invalid";
 case StoreStatus::Stale:return "component inventory belongs to different native save contents";case StoreStatus::Corrupt:return "component inventory checksum/format is invalid";
 case StoreStatus::TooLarge:return "component inventory file is too large";case StoreStatus::IoError:return "component inventory file operation failed";
 case StoreStatus::InvalidSnapshot:return "component inventory snapshot is invalid";}return "unknown component inventory status";
}
Path salvage_sidecar_path(const Path& native_save){auto result=native_save;result+=".salvage";return result;}
bool save_inventory(const Path& path,const Catalog& catalog,const InventorySnapshot& items,StoreStatus* status)noexcept{
 try{if(!valid_snapshot(catalog,items))return finish(StoreStatus::InvalidSnapshot,status);Fingerprint native;const auto result=fingerprint(path,native);
  if(result!=StoreStatus::Ok)return finish(result,status);const auto bytes=encode(catalog,items,native);
  if(bytes.size()>MaxBytes)return finish(StoreStatus::TooLarge,status);return finish(atomic_write(salvage_sidecar_path(path),bytes)?StoreStatus::Ok:StoreStatus::IoError,status);
 }catch(...){return finish(StoreStatus::IoError,status);}
}
bool load_inventory(const Path& path,Catalog& catalog,InventorySnapshot& items,StoreStatus* status)noexcept{
 try{const auto sidecar=salvage_sidecar_path(path);std::ifstream file(sidecar,std::ios::binary);if(!file)return finish(failure(sidecar,StoreStatus::Missing),status);
  file.seekg(0,std::ios::end);const auto n=file.tellg();if(n<0)return finish(StoreStatus::IoError,status);if(std::uint64_t(n)>MaxBytes)return finish(StoreStatus::TooLarge,status);
  if(n<36)return finish(StoreStatus::Corrupt,status);file.seekg(0);Bytes bytes(static_cast<std::size_t>(n));file.read(reinterpret_cast<char*>(bytes.data()),n);
  if(file.gcount()!=n)return finish(StoreStatus::IoError,status);Catalog next_catalog=catalog;InventorySnapshot next_items;Fingerprint saved,current;
  if(!decode(bytes,next_catalog,next_items,saved))return finish(StoreStatus::Corrupt,status);const auto checked=fingerprint(path,current);if(checked!=StoreStatus::Ok)return finish(checked,status);
  if(saved.bytes!=current.bytes||saved.hash!=current.hash)return finish(StoreStatus::Stale,status);catalog=std::move(next_catalog);items=std::move(next_items);return finish(StoreStatus::Ok,status);
 }catch(...){return finish(StoreStatus::IoError,status);}
}
NativeStore::NativeStore(const openrac_api* api,openrac_mod* mod,Catalog& catalog,Inventory& inventory)
 :NativeStore(api,mod,catalog,inventory,Paths{default_root(),false}){
 const auto args=arguments();for(std::size_t i=1;i<args.size();++i){if(args[i]==Path("--no-card"))paths_.no_card=true;if(args[i]==Path("--cards")&&i+1<args.size())paths_.card_root=args[++i];}
}
NativeStore::NativeStore(const openrac_api* api,openrac_mod* mod,Catalog& catalog,Inventory& inventory,Paths paths)
 :NativeMemory(api),mod_(mod),catalog_(catalog),inventory_(inventory),paths_(std::move(paths)){}
Path NativeStore::active_save_path()const{
 if(paths_.no_card||paths_.card_root.empty())return {};const auto state=g(Memcard);
 if(read<int>(state+0xc8)!=0)return {};const int port=read<int>(state),slot=read<int>(state+0x14);
 if(port<0||port>1||slot<0||slot>4)return {};auto root=paths_.card_root;if(port)root+=".slot2";
 return root/"BESCES-50916RATCHET"/("save"+std::to_string(slot)+".bin");
}
void NativeStore::report(const char* operation,StoreStatus status)const{if(!api_->log)return;std::string text=std::string("components ")+operation+": "+store_status_message(status);api_->log(mod_,status==StoreStatus::Ok?OPENRAC_LOG_INFO:OPENRAC_LOG_WARN,text.c_str());}
void NativeStore::begin_save(){const auto state=g(Memcard);if(pending_||read<int>(state+0xdc)>=3||read<int>(state+0xe4)>=0)return;captured_=Captured{catalog_,inventory_.snapshot()};}
void NativeStore::after_prepare(){const auto state=g(Memcard);const int operation=read<int>(state+0xdc),queued=read<int>(state+0xe4);if(!captured_)return;if(!((operation>=15&&operation<=20)||(queued>=15&&queued<=20))){captured_.reset();return;}
 const auto path=active_save_path();if(!path.empty()){pending_=std::move(captured_);pending_path_=path;}captured_.reset();}
void NativeStore::after_io(){if(!pending_)return;const auto state=g(Memcard);if(read<int>(state+0xdc)>=3||read<int>(state+0xe4)>=0)return;
 if(read<int>(state+0xec)==0){StoreStatus status;save_inventory(pending_path_,pending_->catalog,pending_->inventory,&status);report("save",status);}pending_.reset();pending_path_.clear();}
void NativeStore::after_restore_data(unsigned descriptors){if(descriptors!=g(0x1a05c0))return;const auto state=g(Memcard);const int operation=read<int>(state+0xdc),queued=read<int>(state+0xe4);
 if(operation!=13&&operation!=14&&queued!=13&&queued!=14)return;Catalog next_catalog=catalog_;InventorySnapshot next;const auto path=active_save_path();StoreStatus status=StoreStatus::Missing;
 if(!path.empty()&&load_inventory(path,next_catalog,next,&status)){catalog_=std::move(next_catalog);inventory_.restore(next);}else inventory_.restore({});
 captured_.reset();pending_.reset();pending_path_.clear();if(restored_)restored_();if(status!=StoreStatus::Missing)report("load",status);
}
void NativeStore::reset_campaign(bool preserve){captured_.reset();pending_.reset();pending_path_.clear();if(!preserve)inventory_.restore({});if(restored_)restored_();}
}

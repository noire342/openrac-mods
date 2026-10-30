// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arsenal_store.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace arsenal {
namespace {
using Bytes=std::vector<std::uint8_t>;
constexpr std::array<std::uint8_t,8> magic{{'O','R','A','R','S','N','L',0}};
constexpr std::uint32_t codec_version=1;
constexpr std::size_t header_bytes=32,minimum_bytes=header_bytes+52+4;
struct Fingerprint {std::uint32_t bytes{};std::uint64_t hash{};};
bool finish(StoreStatus value,StoreStatus* output){if(output)*output=value;return value==StoreStatus::Ok;}
std::uint32_t read_u32(const std::uint8_t* b){
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
std::uint32_t crc32(const std::uint8_t* bytes,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
    return ~crc;
}
std::uint64_t fnv64(const Bytes& bytes){
    std::uint64_t hash=14695981039346656037ull;
    for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}return hash;
}
StoreStatus open_failure(const std::filesystem::path& path,StoreStatus missing){
    std::error_code error;const bool present=std::filesystem::exists(path,error);
    return !error&&!present?missing:StoreStatus::IoError;
}
StoreStatus native_fingerprint(const std::filesystem::path& path,Fingerprint& result){
    std::ifstream file(path,std::ios::binary);
    if(!file)return open_failure(path,StoreStatus::NativeMissing);
    std::array<std::uint8_t,8> header{};
    file.read(reinterpret_cast<char*>(header.data()),std::streamsize(header.size()));
    if(file.gcount()!=std::streamsize(header.size()))return file.bad()?StoreStatus::IoError:StoreStatus::InvalidNative;
    const auto main=read_u32(header.data()),record=read_u32(header.data()+4);
    const std::uint64_t total=8ull+main+20ull*record;
    if(!main||!record||total>MaximumNativePrefixBytes)return StoreStatus::InvalidNative;
    file.seekg(0,std::ios::end);const auto size=file.tellg();
    if(size<0)return StoreStatus::IoError;
    if(std::uint64_t(size)<total)return StoreStatus::InvalidNative;
    file.seekg(8,std::ios::beg);if(!file)return StoreStatus::IoError;
    Bytes prefix(static_cast<std::size_t>(total));
    std::copy(header.begin(),header.end(),prefix.begin());
    file.read(reinterpret_cast<char*>(prefix.data()+8),std::streamsize(prefix.size()-8));
    if(file.gcount()!=std::streamsize(prefix.size()-8))return StoreStatus::IoError;
    result={std::uint32_t(prefix.size()),fnv64(prefix)};return StoreStatus::Ok;
}
StoreStatus read_sidecar(const std::filesystem::path& path,Bytes& result){
    std::ifstream file(path,std::ios::binary);
    if(!file)return open_failure(path,StoreStatus::Missing);
    file.seekg(0,std::ios::end);const auto size=file.tellg();
    if(size<0)return StoreStatus::IoError;
    if(std::uint64_t(size)>MaximumSidecarBytes)return StoreStatus::TooLarge;
    if(std::uint64_t(size)<minimum_bytes)return StoreStatus::Corrupt;
    file.seekg(0,std::ios::beg);if(!file)return StoreStatus::IoError;
    result.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(result.data()),std::streamsize(result.size()));
    return file.gcount()==std::streamsize(result.size())?StoreStatus::Ok:StoreStatus::IoError;
}
void u8(Bytes& bytes,std::uint8_t value){bytes.push_back(value);}
void u16(Bytes& bytes,std::uint16_t value){u8(bytes,std::uint8_t(value));u8(bytes,std::uint8_t(value>>8));}
void u32(Bytes& bytes,std::uint32_t value){for(unsigned i=0;i<4;++i)u8(bytes,std::uint8_t(value>>(8*i)));}
void u64(Bytes& bytes,std::uint64_t value){for(unsigned i=0;i<8;++i)u8(bytes,std::uint8_t(value>>(8*i)));}
bool generic_valid(const Snapshot& snapshot){
    if(snapshot.records.size()>Registry::MaxSnapshotRecords)return false;
    for(const auto& record:snapshot.records)if(record.stable_id.size()>128)return false;
    Registry validation;return validation.restore(snapshot);
}
Bytes encode(const Snapshot& snapshot,const Fingerprint& native){
    Bytes payload;
    u32(payload,snapshot.format);
    u8(payload,snapshot.walloper.owned?1:0);u8(payload,snapshot.walloper.unlocked);
    u8(payload,snapshot.walloper.acquired);u8(payload,snapshot.walloper.text_variant);
    u32(payload,std::uint32_t(snapshot.walloper.ammo));
    u32(payload,std::uint32_t(snapshot.records.size()));u32(payload,snapshot.equipped);
    for(auto key:snapshot.quick)u32(payload,key);
    for(const auto& record:snapshot.records){
        u32(payload,record.key);u16(payload,std::uint16_t(record.stable_id.size()));
        u8(payload,record.state.owned?1:0);u8(payload,0);
        u32(payload,record.state.ammo);u32(payload,record.state.xp);
        payload.insert(payload.end(),record.stable_id.begin(),record.stable_id.end());
    }
    Bytes result;result.reserve(header_bytes+payload.size()+4);
    result.insert(result.end(),magic.begin(),magic.end());u32(result,codec_version);
    u32(result,std::uint32_t(payload.size()));u32(result,native.bytes);u64(result,native.hash);u32(result,0);
    result.insert(result.end(),payload.begin(),payload.end());u32(result,crc32(result.data(),result.size()));return result;
}
class Reader {
public:
    Reader(const Bytes& bytes,std::size_t limit):bytes_(bytes),limit_(limit){}
    bool get8(std::uint8_t& value){if(!room(1))return false;value=bytes_[at_++];return true;}
    bool get16(std::uint16_t& value){std::uint8_t a,b;if(!get8(a)||!get8(b))return false;value=std::uint16_t(a)|(std::uint16_t(b)<<8);return true;}
    bool get32(std::uint32_t& value){if(!room(4))return false;value=read_u32(bytes_.data()+at_);at_+=4;return true;}
    bool get64(std::uint64_t& value){std::uint32_t a,b;if(!get32(a)||!get32(b))return false;value=std::uint64_t(a)|(std::uint64_t(b)<<32);return true;}
    bool text(std::string& value,std::size_t count){if(!room(count))return false;value.assign(reinterpret_cast<const char*>(bytes_.data()+at_),count);at_+=count;return true;}
    bool skip(std::size_t count){if(!room(count))return false;at_+=count;return true;}
    bool done()const{return at_==limit_;}
private:
    const Bytes& bytes_;std::size_t limit_,at_{};
    bool room(std::size_t count)const{return at_<=limit_&&count<=limit_-at_;}
};
bool decode(const Bytes& bytes,Snapshot& result,Fingerprint& native){
    if(bytes.size()<minimum_bytes||bytes.size()>MaximumSidecarBytes||
       !std::equal(magic.begin(),magic.end(),bytes.begin())||
       read_u32(bytes.data()+bytes.size()-4)!=crc32(bytes.data(),bytes.size()-4))return false;
    Reader read(bytes,bytes.size()-4);std::uint32_t version{},length{},reserved{},count{},ammo{};
    if(!read.skip(8)||!read.get32(version)||version!=codec_version||!read.get32(length)||
       length!=bytes.size()-header_bytes-4||!read.get32(native.bytes)||native.bytes<29||native.bytes>MaximumNativePrefixBytes||
       !read.get64(native.hash)||!read.get32(reserved)||reserved)return false;
    std::uint8_t owned{};
    if(!read.get32(result.format)||!read.get8(owned)||owned>1||!read.get8(result.walloper.unlocked)||
       !read.get8(result.walloper.acquired)||!read.get8(result.walloper.text_variant)||!read.get32(ammo)||
       ammo>std::uint32_t(std::numeric_limits<int>::max())||!read.get32(count)||count>Registry::MaxSnapshotRecords||
       !read.get32(result.equipped))return false;
    result.walloper.owned=owned!=0;result.walloper.ammo=int(ammo);
    for(auto& key:result.quick)if(!read.get32(key))return false;
    result.records.reserve(count);
    for(std::uint32_t i=0;i<count;++i){
        Record record;std::uint16_t text_size{};std::uint8_t flag{},pad{};
        if(!read.get32(record.key)||!read.get16(text_size)||!text_size||text_size>128||
           !read.get8(flag)||flag>1||!read.get8(pad)||pad||!read.get32(record.state.ammo)||
           !read.get32(record.state.xp)||!read.text(record.stable_id,text_size))return false;
        record.state.owned=flag!=0;result.records.push_back(std::move(record));
    }
    return read.done()&&generic_valid(result);
}
std::filesystem::path temporary_path(const std::filesystem::path& target){
    static std::atomic<std::uint64_t> serial{0};
#ifdef _WIN32
    const auto pid=std::uint64_t(GetCurrentProcessId());
#else
    const auto pid=std::uint64_t(getpid());
#endif
    const auto time=std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
    auto result=target;result+=".tmp."+std::to_string(pid)+"."+std::to_string(time)+"."+std::to_string(serial++);return result;
}
#ifdef _WIN32
struct Temporary {
    const std::filesystem::path& path;HANDLE handle=INVALID_HANDLE_VALUE;bool committed=false;
    ~Temporary(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);if(!committed)DeleteFileW(path.c_str());}
};
bool atomic_write(const std::filesystem::path& target,const Bytes& bytes){
    for(unsigned attempt=0;attempt<16;++attempt){
        const auto path=temporary_path(target);
        const auto handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
        if(handle==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_EXISTS||GetLastError()==ERROR_ALREADY_EXISTS)continue;return false;}
        Temporary owned{path,handle};
        std::size_t at=0;while(at<bytes.size()){
            DWORD written=0;
            if(!WriteFile(handle,bytes.data()+at,DWORD(bytes.size()-at),&written,nullptr)||!written)return false;
            at+=written;
        }
        if(!FlushFileBuffers(handle))return false;
        owned.handle=INVALID_HANDLE_VALUE;if(!CloseHandle(handle))return false;
        if(!MoveFileExW(path.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
        owned.committed=true;return true;
    }
    return false;
}
#else
struct Temporary {
    const std::filesystem::path& path;int descriptor=-1;bool committed=false;
    ~Temporary(){if(descriptor>=0)::close(descriptor);if(!committed)::unlink(path.c_str());}
};
bool atomic_write(const std::filesystem::path& target,const Bytes& bytes){
    for(unsigned attempt=0;attempt<16;++attempt){
        const auto path=temporary_path(target);
        const int descriptor=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(descriptor<0){if(errno==EEXIST)continue;return false;}
        Temporary owned{path,descriptor};
        std::size_t at=0;while(at<bytes.size()){
            const auto written=::write(descriptor,bytes.data()+at,bytes.size()-at);
            if(written<0){if(errno==EINTR)continue;return false;}
            if(!written)return false;
            at+=std::size_t(written);
        }
        if(::fsync(descriptor)!=0)return false;
        owned.descriptor=-1;if(::close(descriptor)!=0)return false;
        if(::rename(path.c_str(),target.c_str())!=0)return false;
        owned.committed=true;return true;
    }
    return false;
}
#endif
}

std::filesystem::path sidecar_path(const std::filesystem::path& native_save){auto path=native_save;path+=".arsenal";return path;}
const char* store_status_message(StoreStatus status)noexcept {
    switch(status){
    case StoreStatus::Ok:return "Arsenal sidecar saved or loaded successfully";
    case StoreStatus::Missing:return "No Arsenal sidecar exists for this save";
    case StoreStatus::Stale:return "Arsenal sidecar belongs to different native save contents";
    case StoreStatus::Corrupt:return "Arsenal sidecar format or checksum is invalid";
    case StoreStatus::NativeMissing:return "The native save file does not exist";
    case StoreStatus::InvalidNative:return "The native save header or packet sizes are invalid";
    case StoreStatus::InvalidSnapshot:return "The logical Arsenal snapshot is invalid";
    case StoreStatus::TooLarge:return "Arsenal sidecar exceeds its size limit";
    case StoreStatus::IoError:return "Arsenal sidecar file operation failed";
    }
    return "Unknown Arsenal sidecar status";
}
bool save(const std::filesystem::path& native_save,const Snapshot& snapshot,StoreStatus* status)noexcept {
    try {
        if(!generic_valid(snapshot))return finish(StoreStatus::InvalidSnapshot,status);
        Fingerprint native;const auto checked=native_fingerprint(native_save,native);
        if(checked!=StoreStatus::Ok)return finish(checked,status);
        const auto bytes=encode(snapshot,native);
        if(bytes.size()>MaximumSidecarBytes)return finish(StoreStatus::TooLarge,status);
        return finish(atomic_write(sidecar_path(native_save),bytes)?StoreStatus::Ok:StoreStatus::IoError,status);
    }catch(...){return finish(StoreStatus::IoError,status);}
}
bool load(const std::filesystem::path& native_save,Snapshot& output,StoreStatus* status)noexcept {
    try {
        Bytes bytes;const auto read=read_sidecar(sidecar_path(native_save),bytes);
        if(read!=StoreStatus::Ok)return finish(read,status);
        Snapshot decoded;Fingerprint stored;
        if(!decode(bytes,decoded,stored))return finish(StoreStatus::Corrupt,status);
        Fingerprint current;const auto checked=native_fingerprint(native_save,current);
        if(checked!=StoreStatus::Ok)return finish(checked,status);
        if(stored.bytes!=current.bytes||stored.hash!=current.hash)return finish(StoreStatus::Stale,status);
        output=std::move(decoded);return finish(StoreStatus::Ok,status);
    }catch(...){return finish(StoreStatus::IoError,status);}
}
}

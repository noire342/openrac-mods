// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace spacevoyages {
// Persistent identity, not a menu row, registry handle or native program alone.
// Several custom worlds can use the same native program without aliasing.
enum class RegisteredDestinationKind {Campaign,Added};
struct RegisteredDestination {
    static constexpr std::size_t id_capacity=128;
    RegisteredDestinationKind kind=RegisteredDestinationKind::Campaign;
    int native_level=-1;
    std::array<char,id_capacity> manifest_id{};
    static constexpr RegisteredDestination campaign(int level)noexcept {
        RegisteredDestination result;result.native_level=level;return result;
    }
    static constexpr RegisteredDestination added(const char* id,int level)noexcept {
        RegisteredDestination result;result.kind=RegisteredDestinationKind::Added;result.native_level=level;
        if(!id)return {};
        std::size_t i=0;for(;i<id_capacity&&id[i];++i)result.manifest_id[i]=id[i];
        return i<id_capacity?result:RegisteredDestination{};
    }
    constexpr bool valid()const noexcept {
        if(native_level<0||native_level>18)return false;
        if(kind==RegisteredDestinationKind::Campaign)return native_level>0&&manifest_id[0]==0;
        if(kind!=RegisteredDestinationKind::Added||!manifest_id[0])return false;
        bool ended=false;
        for(char c:manifest_id){
            if(!c){ended=true;continue;}
            if(ended||!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
               (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
        }
        return ended;
    }
};
constexpr bool operator==(const RegisteredDestination& a,const RegisteredDestination& b)noexcept {
    if(a.kind!=b.kind||a.native_level!=b.native_level)return false;
    for(std::size_t i=0;i<a.id_capacity;++i)if(a.manifest_id[i]!=b.manifest_id[i])return false;
    return true;
}
constexpr bool operator!=(const RegisteredDestination& a,const RegisteredDestination& b)noexcept{return !(a==b);}
}

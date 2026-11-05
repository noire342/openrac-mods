// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <string_view>

namespace file_overlay {
inline bool supported_mod_context(unsigned major,unsigned minor,std::string_view game,
    std::string_view disc,std::string_view platform,bool prefix,bool hook,bool function,bool memory) noexcept {
    return prefix&&major==1&&minor>=12&&game=="rac1"&&(disc=="pal"||disc=="ntsc")&&
        platform=="windows-x86_64"&&hook&&function&&memory;
}
// Exact unchanged official release binaries. A missing API version is allowed
// only for the original standalone C ABI, which has no OpenRAC API table.
inline bool supported_client(std::string_view digest,const char* api_version=nullptr) noexcept {
    const char* actual=nullptr;
    if(digest=="b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081"||
       digest=="f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b")actual="ntsc";
    else if(digest=="e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e"||
            digest=="5f7f6d73744a22923695150d99966f82bc41bb99ddb830757b427b68c23c9789")actual="pal";
    return actual&&(!api_version||std::string_view(api_version)==actual);
}
} // namespace file_overlay

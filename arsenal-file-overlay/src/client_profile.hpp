// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <string_view>

namespace file_overlay {
// Exact unchanged official release binaries. A missing API version is allowed
// only for the original standalone C ABI, which has no OpenRAC API table.
inline bool supported_client(std::string_view digest,const char* api_version=nullptr) noexcept {
    const char* actual=nullptr;
    if(digest=="b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081"||
       digest=="f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b")actual="ntsc";
    else if(digest=="e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e")actual="pal";
    return actual&&(!api_version||std::string_view(api_version)==actual);
}
} // namespace file_overlay

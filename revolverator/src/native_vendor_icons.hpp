// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"

namespace revolverator {
class NativeVendor;

// Replaces only Gadgetron's existing icon-strip pass. NativeVendor resolves
// logical row identities; this adapter preserves the original screen geometry.
class NativeVendorIcons final:public Memory {
public:
    NativeVendorIcons(const openrac_api* api,NativeVendor& vendor);
    // False leaves the original pass responsible for unsupported/stale state.
    // No inventory, item IDs, selection or shared item definitions are changed.
    bool draw();
private:
    NativeVendor& vendor_;
    using Rectangle=void(*)(std::int32_t,std::int32_t,std::int32_t,std::int32_t,
                            std::int32_t,std::int32_t,std::uint32_t);
    using Frame=std::int32_t(*)(std::int32_t,std::int32_t);
    using Sprite=void(*)(std::int32_t,std::int32_t,std::int32_t,std::int32_t,
                         std::int32_t,std::int32_t);
    Rectangle rectangle_{};
    Frame frame_{};
    Sprite sprite_{};
};
}

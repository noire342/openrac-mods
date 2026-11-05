// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <openrac/openrac_mod.h>
#include "arsenal_hooks_api.h"
#include <string>
#include <array>

namespace revolverator {
// Only the extra hook boundary is adapted. The original SDK table and its
// official hook chains remain owned by OpenRAC.
class NativeRuntimeHooks {
public:
    using Resolver = openrac_arsenal_hooks_query_fn;
    NativeRuntimeHooks(const openrac_api*, openrac_mod*, Resolver = nullptr);
    ~NativeRuntimeHooks();
    NativeRuntimeHooks(const NativeRuntimeHooks&) = delete;
    NativeRuntimeHooks& operator=(const NativeRuntimeHooks&) = delete;
    int bind(const char* id, const char* signature, void* replacement, void** original);
    void commit();
    std::string diagnostic() const;
private:
    const openrac_api* api_{};
    openrac_mod* mod_{};
    const arsenal_hooks_api_v1* provider_{};
    bool official_{};
    bool claimed_{};
    struct NativeBridge {void* target{};void* consumer{};bool enabled{};};
    std::array<NativeBridge,2> bridges_{};
    int bind_private(const char*,const char*,void*,void**);
    void release_private() noexcept;
};
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <openrac/openrac_mod.h>
#include <filesystem>
#include <memory>
namespace revolverator {
// Private utility libraries ship inside the single installed weapon package.
// They do not add launcher entries or change the port's global SDK table.
class NativeCompatibilityModules {
public:
    NativeCompatibilityModules(const openrac_api*, openrac_mod*, const std::filesystem::path& overlay_configuration);
    ~NativeCompatibilityModules();
    NativeCompatibilityModules(const NativeCompatibilityModules&) = delete;
    NativeCompatibilityModules& operator=(const NativeCompatibilityModules&) = delete;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}

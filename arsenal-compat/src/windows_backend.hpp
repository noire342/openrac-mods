// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "provider.hpp"

namespace arsenal_compat {
class WindowsResolver final : public Resolver {
public:
    using Lookup = void* (*)(const char*);
    explicit WindowsResolver(Lookup lookup) : lookup_(lookup) {}
    bool executable_digest(std::string& digest) override;
    void* resolve(const char* symbol) override;
    bool main_executable_target(void* address) override;
private:
    Lookup lookup_;
};
class MinHookBackend final : public Backend {
public:
    int initialize() override;
    int create(void* target, void* replacement, void** original) override;
    int queue_enable(void* target) override;
    int queue_disable(void* target) override;
    int apply() override;
    int disable(void* target) override;
    int remove(void* target) override;
    void pin_remaining() noexcept;
private:
    struct Owned { void* target{}; void* replacement{}; void* module{}; };
    std::array<Owned, OPENRAC_ARSENAL_REQUIRED_HOOKS> owned_{};
};
} // namespace arsenal_compat

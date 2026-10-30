// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_hooks_api.h"
#include <array>
#include <cstdint>
#include <string>
#include <thread>

namespace arsenal_compat {
struct HookSpec { const char* id; const char* signature; const char* symbol; };
const std::array<HookSpec, OPENRAC_ARSENAL_REQUIRED_HOOKS>& hook_specs() noexcept;
inline constexpr const char* SupportedExecutable =
    "b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081";

class Resolver {
public:
    virtual ~Resolver() = default;
    virtual bool executable_digest(std::string& digest) = 0;
    virtual void* resolve(const char* symbol) = 0;
    virtual bool main_executable_target(void* address) = 0;
};

// Each backend instance owns only its own hook records. No all-hooks API.
class Backend {
public:
    virtual ~Backend() = default;
    virtual int initialize() = 0;
    virtual int create(void* target, void* replacement, void** original) = 0;
    virtual int queue_enable(void* target) = 0;
    virtual int queue_disable(void* target) = 0;
    virtual int apply() = 0;
    virtual int disable(void* target) = 0;
    virtual int remove(void* target) = 0;
};

class Provider final {
public:
    Provider(Resolver& resolver, Backend& backend);
    ~Provider();
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    int preflight();
    int claim(const char* consumer);
    int bind(const char* id, const char* signature, void* replacement, void** original);
    int commit();
    int rollback();
    int diagnostics(arsenal_hooks_diagnostics_v1* out) const;
    static bool owns_id(const char* id) noexcept;
    bool ready() const noexcept;
private:
    struct Binding { void* target{}; void* trampoline{}; bool created{}; bool may_be_active{}; };
    Resolver& resolver_;
    Backend& backend_;
    std::array<Binding, OPENRAC_ARSENAL_REQUIRED_HOOKS> bindings_{};
    arsenal_hooks_diagnostics_v1 diagnostic_{};
    std::thread::id thread_;
    bool busy_{};
    bool backend_ready_{};
    bool claimed_{};
    bool enter() noexcept;
    int fail(int result, const char* text, const char* id = "", int backend = 0) noexcept;
    int cleanup() noexcept;
};
} // namespace arsenal_compat

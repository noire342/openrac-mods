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
// Only the original prefix is required by ABI v1. Additional entries are optional.
inline constexpr std::size_t HookCatalogSize = OPENRAC_ARSENAL_REQUIRED_HOOKS + 1u;
enum class Region { Ntsc, Pal };
const std::array<HookSpec, HookCatalogSize>& hook_specs() noexcept;
const std::array<HookSpec, HookCatalogSize>& hook_specs(Region region) noexcept;
inline constexpr const char* SupportedExecutable =
    "b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081";
inline constexpr std::array<const char*, 2> SupportedExecutables{{
    SupportedExecutable,
    "f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b"
}};
inline constexpr const char* SupportedPalExecutable =
    "e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e";

class Resolver {
public:
    virtual ~Resolver() = default;
    // Private C++ interface only: old test/standalone resolvers remain NTSC.
    virtual const char* version() const noexcept { return "ntsc"; }
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
    Region region_{Region::Ntsc};
    std::array<Binding, HookCatalogSize> bindings_{};
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

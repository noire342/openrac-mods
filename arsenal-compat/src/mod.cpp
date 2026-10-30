// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <openrac/openrac_mod.h>
#include "windows_backend.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {
struct Session {
    const openrac_api& api;
    openrac_mod* mod;
    arsenal_compat::WindowsResolver resolver;
    arsenal_compat::MinHookBackend backend;
    arsenal_compat::Provider provider;
    Session(const openrac_api& functions, openrac_mod* owner)
        : api(functions), mod(owner), resolver(functions.function), provider(resolver, backend) {}
    void log(int level) noexcept {
        arsenal_hooks_diagnostics_v1 diagnostic{}; diagnostic.size = sizeof(diagnostic);
        if (api.log && !provider.diagnostics(&diagnostic)) api.log(mod, level, diagnostic.message);
    }
};
std::unique_ptr<Session> session;
template<class F> int32_t invoke(F&& call) noexcept {
    if (!session) return ARSENAL_HOOKS_NOT_READY;
    try { return call(session->provider); }
    catch (...) { return ARSENAL_HOOKS_BACKEND; }
}
int32_t claim(const char* consumer) { return invoke([&](auto& provider) { return provider.claim(consumer); }); }
int32_t bind(const char* id, const char* signature, void* replacement, void** original) {
    return invoke([&](auto& provider) { return provider.bind(id, signature, replacement, original); });
}
int32_t commit() { return invoke([](auto& provider) { return provider.commit(); }); }
int32_t rollback() { return invoke([](auto& provider) { return provider.rollback(); }); }
int32_t diagnostics(arsenal_hooks_diagnostics_v1* out) {
    return invoke([&](auto& provider) { return provider.diagnostics(out); });
}
int32_t owns_id(const char* id) { return arsenal_compat::Provider::owns_id(id) ? 1 : 0; }
const arsenal_hooks_api_v1 service{
    sizeof(service), OPENRAC_ARSENAL_HOOKS_ABI_V1, claim, bind, commit, rollback, diagnostics, owns_id};
}
extern "C" { OPENRAC_MOD_DEFINE("arsenal-compat", "0.1.0") }
extern "C" OPENRAC_MOD_EXPORT const arsenal_hooks_api_v1* openrac_arsenal_hooks_query(uint32_t version) {
    return version == OPENRAC_ARSENAL_HOOKS_ABI_V1 && session && session->provider.ready() ? &service : nullptr;
}
extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    try {
        if (!api || api->size < offsetof(openrac_api, function) + sizeof(api->function) ||
            !api->function || !api->game || !api->version || !api->platform ||
            std::strcmp(api->game, "rac1") || std::strcmp(api->version, "ntsc") ||
            std::strcmp(api->platform, "windows-x86_64")) {
            if (api && api->fail) api->fail(mod, "arsenal-compat requires stock OpenRAC R&C1 NTSC-U on Windows x64");
            return 1;
        }
        if (session) {
            if (api->fail) api->fail(mod, "arsenal-compat is already loaded");
            return 1;
        }
        auto prepared = std::make_unique<Session>(*api, mod);
        const int result = prepared->provider.preflight();
        if (result) {
            arsenal_hooks_diagnostics_v1 diagnostic{}; diagnostic.size = sizeof(diagnostic);
            prepared->provider.diagnostics(&diagnostic);
            if (api->fail) api->fail(mod, diagnostic.message);
            prepared->log(OPENRAC_LOG_ERROR);
            return 1;
        }
        session = std::move(prepared);
        session->log(OPENRAC_LOG_INFO);
        return 0;
    } catch (...) {
        if (api && api->fail) api->fail(mod, "arsenal-compat could not initialize its owned native-hook provider");
        return 1;
    }
}
extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    if (!session) return;
    const int result = session->provider.rollback();
    if (result) {
        session->log(OPENRAC_LOG_ERROR);
        session->backend.pin_remaining();
        HMODULE pinned{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&service), &pinned);
        // The loader cannot accept an unload refusal. Retain this failed
        // transaction and the modules containing any still-owned detours.
        session.release();
        return;
    }
    session.reset();
}

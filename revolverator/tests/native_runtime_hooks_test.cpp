// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_runtime_hooks.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace revolverator;
namespace {
unsigned checks{}, claims{}, binds{}, commits{}, rollbacks{}, sdk_calls{};
int claim_status{}, commit_status{};
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "failed line " << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
void original() {}
void replacement() {}
int claim(const char* name) { ++claims; CHECK(std::strcmp(name, "revolverator") == 0); return claim_status; }
int owns(const char* id) { return id && std::strcmp(id, "rac1.menu.item_grid.draw") == 0; }
int bind(const char* id, const char* signature, void* target, void** next) {
    ++binds; CHECK(owns(id)); CHECK(std::strcmp(signature, "i32(u32)") == 0);
    CHECK(target == reinterpret_cast<void*>(replacement));
    *next = reinterpret_cast<void*>(original); return ARSENAL_HOOKS_OK;
}
int commit() { ++commits; return commit_status; }
int rollback() { ++rollbacks; return ARSENAL_HOOKS_OK; }
int diagnostic(arsenal_hooks_diagnostics_v1* out) {
    CHECK(out && out->size == sizeof(*out));
    out->last_status = claim_status ? claim_status : commit_status;
    std::strcpy(out->message, "fixture transaction failed");
    std::strcpy(out->hook_id, "rac1.menu.item_grid.draw");
    return ARSENAL_HOOKS_OK;
}
arsenal_hooks_api_v1 service{sizeof service, 1, claim, bind, commit, rollback, diagnostic, owns};
const arsenal_hooks_api_v1* query(uint32_t version) { CHECK(version == 1); return &service; }
const arsenal_hooks_api_v1* absent(uint32_t) { return nullptr; }
int sdk_hook(openrac_mod*, const char* id, const char* signature, void* target, void** next) {
    ++sdk_calls; CHECK(std::strcmp(id, "rac1.item.give") == 0);
    CHECK(std::strcmp(signature, "void(i32,i32)") == 0);
    CHECK(target == reinterpret_cast<void*>(replacement));
    *next = reinterpret_cast<void*>(original); return OPENRAC_HOOK_OK;
}
template<class Function> std::string throws(Function action) {
    try { action(); } catch (const std::runtime_error& error) { return error.what(); }
    CHECK(false); return {};
}
}
int main() {
    openrac_api api{}; api.hook = sdk_hook;
    CHECK(throws([&] { NativeRuntimeHooks h(&api, nullptr, absent); }).find("Arsenal Compatibility") != std::string::npos);
    CHECK(claims == 0 && sdk_calls == 0);
    const auto bytes = service.size;
    service.size = 8;
    CHECK(!throws([&] { NativeRuntimeHooks h(&api, nullptr, query); }).empty());
    CHECK(claims == 0);
    service.size = bytes; service.owns_id = nullptr;
    CHECK(!throws([&] { NativeRuntimeHooks h(&api, nullptr, query); }).empty());
    CHECK(claims == 0); service.owns_id = owns;
    claim_status = ARSENAL_HOOKS_CONSUMER;
    CHECK(throws([&] { NativeRuntimeHooks h(&api, nullptr, query); }).find("fixture transaction failed") != std::string::npos);
    CHECK(claims == 1 && rollbacks == 0);
    claim_status = 0;
    {
        NativeRuntimeHooks hooks(&api, nullptr, query);
        void* next{};
        CHECK(hooks.bind("rac1.item.give", "void(i32,i32)", reinterpret_cast<void*>(replacement), &next) == 0);
        CHECK(next == reinterpret_cast<void*>(original));
        CHECK(sdk_calls == 1 && binds == 0 && commits == 0);
        next = nullptr;
        CHECK(hooks.bind("rac1.menu.item_grid.draw", "i32(u32)", reinterpret_cast<void*>(replacement), &next) == 0);
        CHECK(next == reinterpret_cast<void*>(original));
        CHECK(sdk_calls == 1 && binds == 1 && commits == 0);
        hooks.commit(); CHECK(commits == 1);
    }
    CHECK(rollbacks == 1);
    commit_status = ARSENAL_HOOKS_INCOMPLETE;
    CHECK(throws([&] { NativeRuntimeHooks hooks(&api, nullptr, query); hooks.commit(); }).find("rac1.menu.item_grid.draw") != std::string::npos);
    CHECK(rollbacks == 2 && commits == 2);
    CHECK(api.hook == sdk_hook);
    std::cout << checks << " native runtime adapter checks passed\n";
}

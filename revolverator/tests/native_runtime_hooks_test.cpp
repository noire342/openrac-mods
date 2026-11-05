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
unsigned resolver_calls{};
bool official{};
bool missing_strip{};
#if defined(_WIN32)
volatile unsigned strip_original_calls{},strip_replacement_calls{};
volatile unsigned pose_original_value{},pose_replacement_value{};
__declspec(noinline) void strip_original(){strip_original_calls=strip_original_calls+1;strip_original_calls=strip_original_calls+2;}
__declspec(noinline) void strip_replacement(){strip_replacement_calls=strip_replacement_calls+1;}
__declspec(noinline) void pose_original(unsigned value){pose_original_value=value;pose_original_value=pose_original_value+1;}
__declspec(noinline) void pose_replacement(unsigned value){pose_replacement_value=value;}
void* strip_lookup(const char* name){
    if(name&&!std::strcmp(name,"func_00239A00"))return reinterpret_cast<void*>(strip_original);
    if(name&&!std::strcmp(name,"func_00225E00"))return reinterpret_cast<void*>(pose_original);
    return nullptr;
}
#endif
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
const arsenal_hooks_api_v1* query(uint32_t version) { ++resolver_calls; CHECK(version == 1); return &service; }
const arsenal_hooks_api_v1* absent(uint32_t) { ++resolver_calls; return nullptr; }
int sdk_hook(openrac_mod*, const char* id, const char* signature, void* target, void** next) {
    ++sdk_calls;
    if(missing_strip&&id&&!std::strcmp(id,"rac1.vendor.icon_strip.draw"))return OPENRAC_HOOK_UNKNOWN;
    if (official && std::strcmp(id, "rac1.menu.item_grid.draw") == 0)
        CHECK(std::strcmp(signature, "i32(u32)") == 0);
    else {CHECK(std::strcmp(id, "rac1.item.give") == 0); CHECK(std::strcmp(signature, "void(i32,i32)") == 0);}
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
    for(unsigned minor:{12u,13u}){
        api.api_major=1;api.api_minor=static_cast<unsigned short>(minor);official=true;
        const auto prior_resolves=resolver_calls,prior_claims=claims,prior_binds=binds,prior_commits=commits,prior_rollbacks=rollbacks,prior_sdk=sdk_calls;
        {
            NativeRuntimeHooks hooks(&api,nullptr,query);void* next{};
            CHECK(hooks.bind("rac1.menu.item_grid.draw","i32(u32)",reinterpret_cast<void*>(replacement),&next)==OPENRAC_HOOK_OK);
            CHECK(next==reinterpret_cast<void*>(original));
            hooks.commit();CHECK(hooks.diagnostic().find("Official OpenRAC")!=std::string::npos);
        }
        CHECK(resolver_calls==prior_resolves&&claims==prior_claims&&binds==prior_binds&&commits==prior_commits&&rollbacks==prior_rollbacks);
        CHECK(sdk_calls==prior_sdk+1);
    }
    api.api_major=1;api.api_minor=11;official=false;
    CHECK(throws([&] { NativeRuntimeHooks hooks(&api,nullptr,absent); }).find("Arsenal Compatibility")!=std::string::npos);
    api.api_major=2;api.api_minor=12;
    CHECK(throws([&] { NativeRuntimeHooks hooks(&api,nullptr,absent); }).find("Arsenal Compatibility")!=std::string::npos);
#if defined(_WIN32)
    api.api_major=1;api.api_minor=13;api.version="pal";api.function=strip_lookup;missing_strip=true;
    void* strip_next{};
    {
        NativeRuntimeHooks hooks(&api,nullptr,absent);
        const auto sdk_before=sdk_calls;void* pose_next{};
        CHECK(hooks.bind("rac1.vendor.icon_strip.draw","i32()",reinterpret_cast<void*>(strip_replacement),&strip_next)==OPENRAC_HOOK_SIGNATURE);
        CHECK(hooks.bind("rac1.vendor.icon_strip.draw","void()",reinterpret_cast<void*>(strip_replacement),&strip_next)==OPENRAC_HOOK_OK);
        CHECK(hooks.bind("rac1.menu.preview.pose","void(u32)",reinterpret_cast<void*>(pose_replacement),&pose_next)==OPENRAC_HOOK_OK);
        CHECK(sdk_calls==sdk_before); // Unknown SDK calls poison a real loader transaction.
        CHECK(strip_next!=nullptr);hooks.commit();
        reinterpret_cast<void(*)()>(api.function("func_00239A00"))();
        CHECK(strip_replacement_calls==1&&strip_original_calls==0);
        reinterpret_cast<void(*)()>(strip_next)();CHECK(strip_original_calls==3);
        reinterpret_cast<void(*)(unsigned)>(api.function("func_00225E00"))(0x12345678u);
        CHECK(pose_replacement_value==0x12345678u&&pose_original_value==0);
        reinterpret_cast<void(*)(unsigned)>(pose_next)(99u);CHECK(pose_original_value==100u);
    }
    reinterpret_cast<void(*)()>(api.function("func_00239A00"))();
    CHECK(strip_original_calls==6&&strip_replacement_calls==1);
    reinterpret_cast<void(*)(unsigned)>(api.function("func_00225E00"))(77u);CHECK(pose_original_value==78u);
#endif
    CHECK(api.hook == sdk_hook);
    std::cout << checks << " native runtime adapter checks passed\n";
}

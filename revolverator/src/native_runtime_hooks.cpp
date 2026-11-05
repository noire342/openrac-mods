// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_runtime_hooks.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
#endif

namespace revolverator {
namespace {
const arsenal_hooks_api_v1* existing_provider(std::uint32_t version) {
#if defined(_WIN32)
    const auto image = GetModuleHandleW(L"arsenal_compat.dll");
    const auto query = image ? reinterpret_cast<openrac_arsenal_hooks_query_fn>(
        GetProcAddress(image, OPENRAC_ARSENAL_HOOKS_QUERY)) : nullptr;
    return query ? query(version) : nullptr;
#else
    (void)version;
    return nullptr;
#endif
}
}

NativeRuntimeHooks::NativeRuntimeHooks(const openrac_api* api, openrac_mod* mod, Resolver resolver)
    : api_(api), mod_(mod) {
    // Use official chains wherever exposed. The typed vendor icon-strip
    // adapters below cover the two missing public points on interface1.12.
    if (api_ && api_->api_major == 1 && api_->api_minor >= 12 && api_->hook) {
        official_ = true;
        return;
    }
    provider_ = (resolver ? resolver : existing_provider)(OPENRAC_ARSENAL_HOOKS_ABI_V1);
    constexpr auto required = offsetof(arsenal_hooks_api_v1, owns_id) + sizeof(arsenal_hooks_api_v1{}.owns_id);
    if (!api_ || !api_->hook || !provider_ || provider_->size < required ||
        provider_->version != OPENRAC_ARSENAL_HOOKS_ABI_V1 || !provider_->claim || !provider_->bind ||
        !provider_->commit || !provider_->rollback || !provider_->diagnostics || !provider_->owns_id)
        throw std::runtime_error("Enable the matching Arsenal Compatibility mod before Revolverator; its native hook API v1 is required");
    const auto result = provider_->claim("revolverator");
    if (result != ARSENAL_HOOKS_OK)
        throw std::runtime_error("Cannot claim Arsenal Compatibility: " + diagnostic());
    claimed_ = true;
}

NativeRuntimeHooks::~NativeRuntimeHooks() {
    release_private();
    if (!claimed_ || provider_->rollback() == ARSENAL_HOOKS_OK) return;
    // If a platform refuses rollback, keep this image resident so an outstanding
    // native detour never points into unloaded consumer code.
#if defined(_WIN32)
    HMODULE pinned{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&existing_provider), &pinned);
#endif
    if (api_->log) api_->log(mod_, OPENRAC_LOG_ERROR, "Arsenal Compatibility could not remove every detour; consumer image retained until process exit");
}

int NativeRuntimeHooks::bind(const char* id, const char* signature, void* replacement, void** original) {
    if(!official_&&provider_->owns_id(id))return provider_->bind(id,signature,replacement,original);
    // A refused official hook poisons the loader transaction. Do not probe
    // a known private point through api.hook before installing its adapter.
    if(official_&&id&&(!std::strcmp(id,"rac1.vendor.icon_strip.draw")||!std::strcmp(id,"rac1.menu.preview.pose")))return bind_private(id,signature,replacement,original);
    return api_->hook(mod_,id,signature,replacement,original);
}

void NativeRuntimeHooks::commit() {
    if (official_) {
#if defined(_WIN32)
        for(auto& bridge:bridges_)if(bridge.target&&!bridge.enabled){
            if(MH_EnableHook(bridge.target)!=MH_OK)throw std::runtime_error("Cannot activate the owned native presentation adapter");
            bridge.enabled=true;
        }
#endif
        return;
    }
    const auto result = provider_->commit();
    if (result != ARSENAL_HOOKS_OK)
        throw std::runtime_error("Cannot activate Arsenal Compatibility: " + diagnostic());
}

std::string NativeRuntimeHooks::diagnostic() const {
    if (official_) return "Official OpenRAC hook chains with typed native icon-strip and preview-pose adapters";
    arsenal_hooks_diagnostics_v1 info{};
    info.size = sizeof info;
    if (provider_->diagnostics(&info) != ARSENAL_HOOKS_OK) return "diagnostics unavailable";
    info.message[sizeof info.message - 1] = 0;
    info.hook_id[sizeof info.hook_id - 1] = 0;
    return std::string(info.message) + (info.hook_id[0] ? std::string(" [") + info.hook_id + "]" : "") +
        " (" + std::to_string(info.last_status) + ")";
}
int NativeRuntimeHooks::bind_private(const char* id,const char* signature,void* replacement,void** original){
    const bool strip=id&&!std::strcmp(id,"rac1.vendor.icon_strip.draw");
    const bool pose=id&&!std::strcmp(id,"rac1.menu.preview.pose");
    if(!strip&&!pose)return OPENRAC_HOOK_UNKNOWN;
    if(!signature||std::strcmp(signature,strip?"void()":"void(u32)"))return OPENRAC_HOOK_SIGNATURE;
#if defined(_WIN32)
    auto& bridge=bridges_[strip?0:1];
    if(bridge.target||!replacement||!original||!api_->function||!api_->version)return OPENRAC_HOOK_UNKNOWN;
    const char* symbol=nullptr;
    if(!std::strcmp(api_->version,"pal"))symbol=strip?"func_00239A00":"func_00225E00";
    if(!std::strcmp(api_->version,"ntsc"))symbol=strip?"FUN_002386e8":"FUN_00224b70";
    auto* target=symbol?api_->function(symbol):nullptr;
    MEMORY_BASIC_INFORMATION region{};
    if(!target||!VirtualQuery(target,&region,sizeof region)||region.State!=MEM_COMMIT||region.Type!=MEM_IMAGE||
       region.AllocationBase!=GetModuleHandleW(nullptr)||region.Protect&(PAGE_GUARD|PAGE_NOACCESS))return OPENRAC_HOOK_UNKNOWN;
    const auto protection=region.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return OPENRAC_HOOK_UNKNOWN;
    HMODULE consumer{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(replacement),&consumer))return OPENRAC_HOOK_UNKNOWN;
    const auto initialized=MH_Initialize();
    if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED){FreeLibrary(consumer);return OPENRAC_HOOK_UNKNOWN;}
    const auto created=MH_CreateHook(target,replacement,original);
    if(created!=MH_OK){FreeLibrary(consumer);return OPENRAC_HOOK_UNKNOWN;}
    bridge={target,consumer,false};return OPENRAC_HOOK_OK;
#else
    (void)replacement;(void)original;return OPENRAC_HOOK_UNKNOWN;
#endif
}
void NativeRuntimeHooks::release_private()noexcept{
#if defined(_WIN32)
    for(auto& bridge:bridges_){
    if(!bridge.target)continue;
    const auto disabled=MH_DisableHook(bridge.target);
    if((disabled==MH_OK||disabled==MH_ERROR_DISABLED)&&MH_RemoveHook(bridge.target)==MH_OK){
        if(bridge.consumer)FreeLibrary(static_cast<HMODULE>(bridge.consumer));
        bridge={};continue;
    }
    // Keep consumer code mapped rather than leave a dangling native callback.
    if(bridge.consumer){HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(bridge.consumer),&pinned);}
    if(api_&&api_->log)api_->log(mod_,OPENRAC_LOG_ERROR,"Native presentation adapter could not be removed; consumer retained");
    }
#endif
}
}

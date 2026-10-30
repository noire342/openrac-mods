// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_runtime_hooks.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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
    : api_(api), mod_(mod), provider_((resolver ? resolver : existing_provider)(OPENRAC_ARSENAL_HOOKS_ABI_V1)) {
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
    return provider_->owns_id(id) ? provider_->bind(id, signature, replacement, original) :
        api_->hook(mod_, id, signature, replacement, original);
}

void NativeRuntimeHooks::commit() {
    const auto result = provider_->commit();
    if (result != ARSENAL_HOOKS_OK)
        throw std::runtime_error("Cannot activate Arsenal Compatibility: " + diagnostic());
}

std::string NativeRuntimeHooks::diagnostic() const {
    arsenal_hooks_diagnostics_v1 info{};
    info.size = sizeof info;
    if (provider_->diagnostics(&info) != ARSENAL_HOOKS_OK) return "diagnostics unavailable";
    info.message[sizeof info.message - 1] = 0;
    info.hook_id[sizeof info.hook_id - 1] = 0;
    return std::string(info.message) + (info.hook_id[0] ? std::string(" [") + info.hook_id + "]" : "") +
        " (" + std::to_string(info.last_status) + ")";
}
}

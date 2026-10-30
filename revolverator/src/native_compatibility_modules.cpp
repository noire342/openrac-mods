// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_compatibility_modules.hpp"
#include "file_overlay_api.h"
#include <stdexcept>
#include <cstring>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#endif

namespace revolverator {
#if defined(_WIN32)
namespace {
std::filesystem::path utility_directory() {
    HMODULE own{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&utility_directory), &own))
        throw std::runtime_error("Cannot locate the Revolverator package");
    std::vector<wchar_t> path(32768);
    const auto count = GetModuleFileNameW(own, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) throw std::runtime_error("Cannot read the Revolverator package path");
    return std::filesystem::path(path.data()).parent_path();
}
HMODULE load_private(const std::filesystem::path& file) {
    const auto image = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!image) throw std::runtime_error("Incomplete Revolverator package: cannot load " + file.filename().string());
    return image;
}
void pin(HMODULE image) noexcept {
    if (!image) return;
    HMODULE retained{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(image), &retained);
}
}
struct NativeCompatibilityModules::State {
    HMODULE provider{}, overlay{};
    bool owned_provider{}, provider_started{}, overlay_started{};
    openrac_mod_unload_fn provider_unload{};
    openrac_file_overlay_remove_fn remove_overlay{};
    ~State() {
        if (overlay_started && remove_overlay && remove_overlay() != FILE_OVERLAY_OK) pin(overlay);
        if (overlay) FreeLibrary(overlay);
        if (owned_provider) {
            if (provider_started && provider_unload) provider_unload();
            if (provider) FreeLibrary(provider);
        }
    }
};
NativeCompatibilityModules::NativeCompatibilityModules(const openrac_api* api, openrac_mod* mod,
        const std::filesystem::path& configuration) : state_(std::make_unique<State>()) {
    const auto directory = utility_directory();
    auto& state = *state_;
    state.provider = GetModuleHandleW(L"arsenal_compat.dll");
    if (!state.provider) {
        state.provider = load_private(directory / L"arsenal_compat.dll");
        state.owned_provider = true;
        const auto query = reinterpret_cast<openrac_mod_query_fn>(GetProcAddress(state.provider, OPENRAC_MOD_QUERY));
        const auto start = reinterpret_cast<openrac_mod_load_fn>(GetProcAddress(state.provider, OPENRAC_MOD_LOAD));
        state.provider_unload = reinterpret_cast<openrac_mod_unload_fn>(GetProcAddress(state.provider, OPENRAC_MOD_UNLOAD));
        openrac_mod_info information{}; information.size = sizeof information;
        if (query) query(&information);
        if (!query || !start || !state.provider_unload || !information.id || !information.version ||
            std::strcmp(information.id, "arsenal-compat") || std::strcmp(information.version, "0.1.0") ||
            information.api_major != api->api_major || information.api_minor > api->api_minor)
            throw std::runtime_error("The bundled Arsenal Compatibility library is incompatible");
        if (start(api, mod)) throw std::runtime_error("Arsenal Compatibility refused this client build; use the supported official OpenRAC release");
        state.provider_started = true;
    }
    state.overlay = load_private(directory / L"arsenal_file_overlay.dll");
    const auto install = reinterpret_cast<openrac_file_overlay_install_fn>(GetProcAddress(state.overlay, "openrac_file_overlay_install"));
    state.remove_overlay = reinterpret_cast<openrac_file_overlay_remove_fn>(GetProcAddress(state.overlay, "openrac_file_overlay_remove"));
    const auto error = reinterpret_cast<openrac_file_overlay_last_error_fn>(GetProcAddress(state.overlay, "openrac_file_overlay_last_error"));
    if (!install || !state.remove_overlay || !error)
        throw std::runtime_error("The bundled asset-loading library is incompatible");
    if (install(configuration.c_str()) != FILE_OVERLAY_OK)
        throw std::runtime_error(std::string("Cannot mount Revolverator assets: ") + error());
    state.overlay_started = true;
}
#else
struct NativeCompatibilityModules::State {};
NativeCompatibilityModules::NativeCompatibilityModules(const openrac_api*, openrac_mod*, const std::filesystem::path&) {
    throw std::runtime_error("The bundled compatibility modules currently support Windows x64 only");
}
#endif
NativeCompatibilityModules::~NativeCompatibilityModules() = default;
}

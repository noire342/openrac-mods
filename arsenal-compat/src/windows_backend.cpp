// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "windows_backend.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <array>
#include <algorithm>
#include <vector>

namespace arsenal_compat {
namespace {
struct File {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~File() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Algorithm {
    BCRYPT_ALG_HANDLE value{};
    ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
};
struct Hash {
    BCRYPT_HASH_HANDLE value{};
    ~Hash() { if (value) BCryptDestroyHash(value); }
};
bool executable(void* address, MEMORY_BASIC_INFORMATION& region) noexcept {
    if (!address || !VirtualQuery(address, &region, sizeof(region))) return false;
    if (region.State != MEM_COMMIT || region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    const DWORD access = region.Protect & 0xff;
    return access == PAGE_EXECUTE || access == PAGE_EXECUTE_READ ||
        access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}
}
bool WindowsResolver::executable_digest(std::string& digest) {
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return false;
    File file;
    file.value = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) return false;
    Algorithm algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD object_bytes{}, returned{};
    if (BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_bytes), sizeof(object_bytes), &returned, 0) < 0) return false;
    std::vector<unsigned char> object(object_bytes);
    Hash hash;
    if (BCryptCreateHash(algorithm.value, &hash.value, object.data(), object_bytes, nullptr, 0, 0) < 0) return false;
    std::array<unsigned char, 65536> chunk{};
    for (;;) {
        DWORD bytes{};
        if (!ReadFile(file.value, chunk.data(), static_cast<DWORD>(chunk.size()), &bytes, nullptr)) return false;
        if (!bytes) break;
        if (BCryptHashData(hash.value, chunk.data(), bytes, 0) < 0) return false;
    }
    std::array<unsigned char, 32> result{};
    if (BCryptFinishHash(hash.value, result.data(), static_cast<ULONG>(result.size()), 0) < 0) return false;
    static constexpr char Hex[] = "0123456789abcdef";
    digest.clear();
    digest.reserve(64);
    for (const auto byte : result) { digest += Hex[byte >> 4]; digest += Hex[byte & 15]; }
    return true;
}
void* WindowsResolver::resolve(const char* symbol) { return lookup_ ? lookup_(symbol) : nullptr; }
bool WindowsResolver::main_executable_target(void* address) {
    MEMORY_BASIC_INFORMATION region{};
    const auto module = GetModuleHandleW(nullptr);
    if (!executable(address, region) || region.Type != MEM_IMAGE || region.AllocationBase != module) return false;
    const auto* base = reinterpret_cast<const unsigned char*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    return value >= start && value - start < nt->OptionalHeader.SizeOfImage;
}
int MinHookBackend::initialize() {
    const auto status = MH_Initialize();
    return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED ? 0 : static_cast<int>(status);
}
int MinHookBackend::create(void* target, void* replacement, void** original) {
    MEMORY_BASIC_INFORMATION region{};
    if (!executable(replacement, region)) return static_cast<int>(MH_ERROR_NOT_EXECUTABLE);
    const auto slot = std::find_if(owned_.begin(), owned_.end(), [](const Owned& value) { return !value.target; });
    if (slot == owned_.end()) return static_cast<int>(MH_ERROR_MEMORY_ALLOC);
    HMODULE consumer{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(replacement), &consumer)) return static_cast<int>(MH_ERROR_NOT_EXECUTABLE);
    const auto status = MH_CreateHook(target, replacement, original);
    if (status != MH_OK) { FreeLibrary(consumer); return static_cast<int>(status); }
    *slot = {target, replacement, consumer};
    return 0;
}
int MinHookBackend::queue_enable(void* target) { return static_cast<int>(MH_QueueEnableHook(target)); }
int MinHookBackend::queue_disable(void* target) { return static_cast<int>(MH_QueueDisableHook(target)); }
int MinHookBackend::apply() { return static_cast<int>(MH_ApplyQueued()); }
int MinHookBackend::disable(void* target) {
    const auto status = MH_DisableHook(target);
    return status == MH_OK || status == MH_ERROR_DISABLED ? 0 : static_cast<int>(status);
}
int MinHookBackend::remove(void* target) {
    const auto status = MH_RemoveHook(target);
    if (status != MH_OK) return static_cast<int>(status);
    for (auto& owned : owned_) if (owned.target == target) {
        if (owned.module) FreeLibrary(static_cast<HMODULE>(owned.module));
        owned = {};
        break;
    }
    return 0;
}
void MinHookBackend::pin_remaining() noexcept {
    for (const auto& owned : owned_) if (owned.target) {
        HMODULE pinned{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(owned.replacement), &pinned);
    }
}
} // namespace arsenal_compat

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "windows_overlay.hpp"
#include "overlay_core.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <sstream>

namespace file_overlay {
namespace {
using Create = HANDLE (WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
constexpr char SupportedHash[] = "b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081";
std::mutex lifecycle;
std::array<char, 512> error_text{};
std::shared_ptr<const PathMap> mapping;
std::atomic<Create> next_create{};
std::atomic<std::uint64_t> redirected{}, passthrough{}, active_calls{};
std::atomic<bool> installed{};
Create target{}; // Mutated only under lifecycle.
bool created{};
thread_local bool recursive{};
std::atomic<bool> trace_enabled{};
std::mutex trace_mutex;
struct ManifestTrace { std::wstring path; DWORD access{}, disposition{}, flags{}; bool eligible{}, matched{}; };
std::vector<ManifestTrace> manifest_traces;
std::atomic<std::uint64_t> manifest_candidates{}, manifest_matches{};

int error(int status, const char* message) noexcept {
    const auto count = std::min(std::strlen(message), error_text.size() - 1);
    std::memcpy(error_text.data(), message, count); error_text[count] = 0;
    return status;
}
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Algorithm { BCRYPT_ALG_HANDLE value{}; ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); } };
struct Hash { BCRYPT_HASH_HANDLE value{}; ~Hash() { if (value) BCryptDestroyHash(value); } };
bool digest_executable(std::string& digest) {
    std::vector<wchar_t> path(32768);
    const auto count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) return false;
    Handle file; file.value = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) return false;
    Algorithm algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD object_size{}, got{};
    if (BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &got, 0) < 0) return false;
    std::vector<unsigned char> object(object_size); Hash hash;
    if (BCryptCreateHash(algorithm.value, &hash.value, object.data(), object_size, nullptr, 0, 0) < 0) return false;
    std::array<unsigned char, 65536> buffer{};
    for (;;) {
        DWORD bytes{};
        if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr)) return false;
        if (!bytes) break;
        if (BCryptHashData(hash.value, buffer.data(), bytes, 0) < 0) return false;
    }
    std::array<unsigned char, 32> result{};
    if (BCryptFinishHash(hash.value, result.data(), static_cast<ULONG>(result.size()), 0) < 0) return false;
    constexpr char Hex[] = "0123456789abcdef"; digest.clear();
    for (const auto byte : result) { digest += Hex[byte >> 4]; digest += Hex[byte & 15]; }
    return true;
}
std::wstring utf16(const std::string& text) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) throw std::runtime_error("Alias paths must be valid UTF-8");
    std::wstring result(static_cast<std::size_t>(count), 0);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count))
        throw std::runtime_error("Cannot decode alias path");
    return result;
}
std::string utf8(const std::wstring& text) {
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "<invalid UTF-16 path>";
    std::string result(static_cast<std::size_t>(count), 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}
std::wstring normalize(std::wstring path, bool absolute, bool fold) {
    if (path.empty() || path.size() >= 32768) throw std::runtime_error("Invalid alias path length");
    for (auto& c : path) { if (c < 32) throw std::runtime_error("Control characters are forbidden in paths"); if (c == '/') c = '\\'; }
    if (path.compare(0, 8, L"\\\\?\\UNC\\") == 0) path = L"\\\\" + path.substr(8);
    else if (path.compare(0, 4, L"\\\\?\\") == 0) path.erase(0, 4);
    if (path.compare(0, 4, L"\\\\.\\") == 0 || path.compare(0, 4, L"\\??\\") == 0) throw std::runtime_error("Device paths are not file aliases");
    const bool drive = path.size() > 2 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' && path[2] == '\\';
    const bool unc = path.size() > 2 && path[0] == '\\' && path[1] == '\\';
    if (absolute && !drive && !unc) throw std::runtime_error("Configuration and alias paths must be absolute");
    const DWORD count = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!count || count >= 32768) throw std::runtime_error("Cannot normalize absolute file path");
    std::wstring result(count, 0);
    const auto written = GetFullPathNameW(path.c_str(), count, result.data(), nullptr);
    if (!written || written >= count) throw std::runtime_error("Cannot normalize file path");
    result.resize(written);
    if (fold) {
        const int needed = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, result.data(), static_cast<int>(result.size()), nullptr, 0, nullptr, nullptr, 0);
        if (needed <= 0) throw std::runtime_error("Cannot normalize filename case");
        std::wstring lower(static_cast<std::size_t>(needed), 0);
        if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, result.data(), static_cast<int>(result.size()), lower.data(), needed, nullptr, nullptr, 0))
            throw std::runtime_error("Cannot normalize filename case");
        return lower;
    }
    return result;
}
void record_manifest(const std::wstring& path, DWORD access, DWORD disposition, DWORD flags, bool eligible, bool matched) noexcept {
    if (!trace_enabled.load(std::memory_order_relaxed) || path.size() < 13 || path.compare(path.size() - 13, 13, L"manifest.json")) return;
    manifest_candidates.fetch_add(1, std::memory_order_relaxed);
    if (matched) manifest_matches.fetch_add(1, std::memory_order_relaxed);
    try {
        std::lock_guard<std::mutex> guard(trace_mutex);
        if (manifest_traces.size() == 24) manifest_traces.erase(manifest_traces.begin());
        manifest_traces.push_back({path, access, disposition, flags, eligible, matched});
    } catch (...) {}
}
void require_file(const std::wstring& path) {
    const auto flags = GetFileAttributesW(path.c_str());
    if (flags == INVALID_FILE_ATTRIBUTES || flags & FILE_ATTRIBUTE_DIRECTORY) throw std::runtime_error("Each alias must name an existing original and staged regular file");
}
std::shared_ptr<const PathMap> read_mapping(const wchar_t* config_file) {
    const auto config = normalize(config_file, true, false);
    Handle file; file.value = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot read file-overlay.json");
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.value, &length) || length.QuadPart <= 0 || length.QuadPart > MaximumConfigBytes)
        throw std::runtime_error("File-overlay config must be1..256KiB");
    std::string text(static_cast<std::size_t>(length.QuadPart), 0); DWORD got{};
    if (!ReadFile(file.value, text.data(), static_cast<DWORD>(text.size()), &got, nullptr) || got != text.size()) throw std::runtime_error("Cannot read complete file-overlay config");
    std::vector<PathPair> pairs;
    for (const auto& alias : parse_config(text)) {
        auto original = normalize(utf16(alias.original), true, false);
        auto replacement = normalize(utf16(alias.replacement), true, false);
        require_file(original); require_file(replacement);
        const auto original_key = normalize(original, true, true);
        if (original_key == normalize(replacement, true, true)) throw std::runtime_error("Original and replacement cannot name the same path");
        pairs.push_back({original_key, std::move(replacement)});
    }
    return std::make_shared<const PathMap>(std::move(pairs));
}
HANDLE WINAPI file_hook(LPCWSTR path, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD flags, HANDLE template_file) noexcept {
    const DWORD incoming_error = GetLastError();
    active_calls.fetch_add(1, std::memory_order_seq_cst);
    const auto original = next_create.load(std::memory_order_seq_cst);
    HANDLE result = INVALID_HANDLE_VALUE;
    DWORD native_error = incoming_error;
    {
        std::shared_ptr<const PathMap> snapshot;
        const wchar_t* selected = path;
        const bool nested = recursive;
        if (!nested) {
            recursive = true;
            try {
                snapshot = std::atomic_load_explicit(&mapping, std::memory_order_acquire);
                if (snapshot && path && (read_only_open(access, disposition, flags) || trace_enabled.load(std::memory_order_relaxed))) {
                    const auto key = normalize(path, false, true);
                    const bool eligible = read_only_open(access, disposition, flags);
                    const auto* replacement = eligible ? snapshot->find(key) : nullptr;
                    if (replacement) { selected = replacement->c_str(); redirected.fetch_add(1, std::memory_order_relaxed); }
                    record_manifest(key, access, disposition, flags, eligible, replacement != nullptr);
                }
            } catch (...) { selected = path; }
        }
        if (selected == path) passthrough.fetch_add(1, std::memory_order_relaxed);
        SetLastError(incoming_error);
        if (original) result = original(selected, access, sharing, security, disposition, flags, template_file);
        else SetLastError(ERROR_INVALID_FUNCTION);
        native_error = GetLastError();
        if (!nested) recursive = false;
    }
    active_calls.fetch_sub(1, std::memory_order_seq_cst);
    SetLastError(native_error);
    return result;
}
void pin_self() noexcept {
    HMODULE own{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&file_hook), &own);
}
}
int install(const wchar_t* config_file) noexcept {
    try {
        std::lock_guard<std::mutex> guard(lifecycle);
        if (created) return error(FILE_OVERLAY_ALREADY_INSTALLED, "A file overlay is already installed or awaiting safe removal");
        if (!config_file || !*config_file) return error(FILE_OVERLAY_ARGUMENT, "An absolute configuration file path is required");
        std::string digest;
        if (!digest_executable(digest) || digest != SupportedHash)
            return error(FILE_OVERLAY_BUILD, "Unsupported client: file overlay requires the unchanged official NTSC-U Windows x64 executable SHA256 b7f3762a...df94081");
        auto prepared = read_mapping(config_file);
        // UCRT imports the API-set entry, which resolves directly to KernelBase.
        // kernel32 has a different wrapper address and misses those calls.
        const auto kernel = GetModuleHandleW(L"kernelbase.dll");
        target = kernel ? reinterpret_cast<Create>(GetProcAddress(kernel, "CreateFileW")) : nullptr;
        if (!target) return error(FILE_OVERLAY_OS, "Cannot resolve the system CreateFileW function");
        const auto initial = MH_Initialize();
        if (initial != MH_OK && initial != MH_ERROR_ALREADY_INITIALIZED) return error(FILE_OVERLAY_BACKEND, "Cannot initialize the owned detour backend");
        void* trampoline{};
        const auto prepared_hook = MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&file_hook), &trampoline);
        if (prepared_hook != MH_OK || !trampoline) return error(FILE_OVERLAY_BACKEND, "Cannot prepare the disabled CreateFileW hook");
        created = true;
        pin_self(); // Late worker callbacks must never execute unloaded code.
        next_create.store(reinterpret_cast<Create>(trampoline), std::memory_order_seq_cst);
        redirected.store(0); passthrough.store(0);
        std::atomic_store_explicit(&mapping, std::move(prepared), std::memory_order_release);
        const auto enabled = MH_EnableHook(reinterpret_cast<void*>(target));
        if (enabled != MH_OK) {
            std::atomic_store_explicit(&mapping, std::shared_ptr<const PathMap>{}, std::memory_order_release);
            const auto disabled = MH_DisableHook(reinterpret_cast<void*>(target));
            if (disabled == MH_OK || disabled == MH_ERROR_DISABLED) {
                next_create.store(target, std::memory_order_seq_cst);
                if (MH_RemoveHook(reinterpret_cast<void*>(target)) == MH_OK) created = false;
            }
            return error(FILE_OVERLAY_BACKEND, "Cannot activate the owned CreateFileW hook");
        }
        installed.store(true, std::memory_order_release);
        return error(FILE_OVERLAY_OK, "19 read-only exact-path manifest aliases installed on the unchanged stock client");
    } catch (const std::exception& exception) {
        std::lock_guard<std::mutex> guard(lifecycle); return error(FILE_OVERLAY_CONFIG, exception.what());
    } catch (...) { return FILE_OVERLAY_OS; }
}
int remove() noexcept {
    try {
        std::lock_guard<std::mutex> guard(lifecycle);
        if (!created) return error(FILE_OVERLAY_OK, "No file overlay is installed");
        std::atomic_store_explicit(&mapping, std::shared_ptr<const PathMap>{}, std::memory_order_release);
        const auto disabled = MH_DisableHook(reinterpret_cast<void*>(target));
        if (disabled != MH_OK && disabled != MH_ERROR_DISABLED) return error(FILE_OVERLAY_BACKEND, "Cannot disable the owned file hook; pinned module retained");
        installed.store(false, std::memory_order_release);
        next_create.store(target, std::memory_order_seq_cst);
        const auto until = GetTickCount64() + 1000;
        while (active_calls.load(std::memory_order_seq_cst) && GetTickCount64() < until) Sleep(1);
        if (active_calls.load(std::memory_order_seq_cst)) return error(FILE_OVERLAY_BUSY, "File hook disabled; in-flight callbacks retain the trampoline for a later remove retry");
        if (MH_RemoveHook(reinterpret_cast<void*>(target)) != MH_OK) return error(FILE_OVERLAY_BACKEND, "File hook disabled but removal failed; pinned module retained");
        created = false;
        return error(FILE_OVERLAY_OK, "Owned file hook removed; originals remain unchanged");
    } catch (...) { return FILE_OVERLAY_OS; }
}
const char* last_error() noexcept {
    thread_local std::array<char, 512> copy{};
    try { std::lock_guard<std::mutex> guard(lifecycle); copy = error_text; return copy.data(); }
    catch (...) { return "File-overlay diagnostics unavailable"; }
}
int stats(file_overlay_stats_v1* out) noexcept {
    if (!out || out->size < sizeof(*out)) return FILE_OVERLAY_ARGUMENT;
    const auto snapshot = std::atomic_load_explicit(&mapping, std::memory_order_acquire);
    *out = {sizeof(*out), 1, static_cast<std::uint32_t>(snapshot ? snapshot->size() : 0),
        installed.load(std::memory_order_acquire) ? 1u : 0u, redirected.load(std::memory_order_relaxed), passthrough.load(std::memory_order_relaxed)};
    return FILE_OVERLAY_OK;
}
std::wstring default_config_file() {
    HMODULE own{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&file_hook), &own)) throw std::runtime_error("Cannot locate file-overlay module");
    std::vector<wchar_t> buffer(32768);
    const auto count = GetModuleFileNameW(own, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!count || count >= buffer.size()) throw std::runtime_error("Cannot locate file-overlay module path");
    std::wstring path(buffer.data(), count);
    for (unsigned i = 0; i < 3; ++i) {
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) throw std::runtime_error("Expected modroot/lib/windows-x86_64/plugin layout");
        path.resize(slash);
    }
    return path + L"\\file-overlay.json";
}
void enable_diagnostics(bool enabled) noexcept { trace_enabled.store(enabled, std::memory_order_release); }
std::vector<std::string> diagnostic_lines() {
    std::vector<std::string> lines;
    std::ostringstream counters;
    counters << "FILE OVERLAY CreateFileW=KernelBase redirects=" << redirected.load()
        << " passthrough=" << passthrough.load() << " manifest_candidates=" << manifest_candidates.load()
        << " manifest_matches=" << manifest_matches.load() << " active_calls=" << active_calls.load();
    lines.push_back(counters.str());
    std::lock_guard<std::mutex> guard(trace_mutex);
    for (const auto& trace : manifest_traces) {
        std::ostringstream line;
        line << "FILE OVERLAY manifest eligible=" << trace.eligible << " matched=" << trace.matched
            << " access=0x" << std::hex << trace.access << " disposition=" << std::dec << trace.disposition
            << " flags=0x" << std::hex << trace.flags << " path=" << utf8(trace.path);
        lines.push_back(line.str());
    }
    manifest_traces.clear();
    return lines;
}
} // namespace file_overlay

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "store.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>

#include <sys/file.h>
#endif

namespace weaponxp {

Store::~Store() {
    close();
}

void Store::close() {
    if (m_lock != -1) {
#if defined(_WIN32)
        CloseHandle(reinterpret_cast<HANDLE>(m_lock));
#else
        ::close(static_cast<int>(m_lock));
#endif
        m_lock = -1;
    }
    m_path.clear();
}

void Store::open(const std::filesystem::path& path, Progression& state) {
    close();
    std::filesystem::create_directories(path.parent_path());
    const auto lock_path = std::filesystem::path(path).concat(".lock");
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(
        lock_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("weapon XP profile is locked or inaccessible");
    }
    m_lock = reinterpret_cast<std::intptr_t>(handle);
#else
    const int handle = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
    if (handle == -1 || flock(handle, LOCK_EX | LOCK_NB) != 0) {
        if (handle != -1) {
            ::close(handle);
        }
        throw std::runtime_error("weapon XP profile is locked or inaccessible");
    }
    m_lock = handle;
#endif
    m_path = path;
    if (!std::filesystem::exists(path)) {
        state = {};
        return;
    }
    if (std::filesystem::file_size(path) > 4096) {
        throw std::runtime_error("weapon XP profile is invalid; preserved without changes");
    }
    std::ifstream input(path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)), {});
    if (!input.is_open() || input.bad() || !decode(data, state)) {
        throw std::runtime_error("weapon XP profile is invalid; preserved without changes");
    }
    if (state.migrated) {
        const auto backup = std::filesystem::path(path).concat(
            ".v" + std::to_string(state.migration_source) + ".bak"
        );
        if (!std::filesystem::exists(backup)) {
            std::filesystem::copy_file(path, backup);
        }
    }
}

void Store::save(const Progression& state) {
    if (m_lock == -1) {
        throw std::runtime_error("weapon XP profile is not open");
    }
    const auto temp = std::filesystem::path(m_path).concat(".tmp");
    const auto data = encode(state);
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        output.flush();
        output.close();
        if (!output) {
            throw std::runtime_error("cannot save weapon XP; previous file preserved");
        }
    }
#if defined(_WIN32)
    if (!MoveFileExW(
            temp.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        throw std::runtime_error("cannot replace weapon XP file; previous file preserved");
    }
#else
    const int fd = ::open(temp.c_str(), O_RDONLY);
    if (fd == -1 || fsync(fd) != 0) {
        if (fd != -1) {
            ::close(fd);
        }
        throw std::runtime_error("cannot flush weapon XP file; previous file preserved");
    }
    ::close(fd);
    std::filesystem::rename(temp, m_path);
    const int parent = ::open(m_path.parent_path().c_str(), O_RDONLY);
    if (parent != -1) {
        fsync(parent);
        ::close(parent);
    }
#endif
}

std::filesystem::path profile_path(const std::string& disc, const std::string& profile) {
    if (profile.empty() || profile.size() > 32
        || profile.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
           ) != std::string::npos
        || (disc != "pal" && disc != "ntsc")) {
        throw std::runtime_error(
            "profile must contain 1-32 letters, digits, hyphens or underscores"
        );
    }
    std::filesystem::path root;
#if defined(_WIN32)
    // _wgetenv keeps non-ASCII Windows account paths intact.
    wchar_t* value = nullptr;
    std::size_t count = 0;
    if (_wdupenv_s(&value, &count, L"LOCALAPPDATA") == 0 && value) {
        root = value;
    }
    std::free(value);
#elif defined(__APPLE__)
    if (const char* value = std::getenv("HOME")) {
        root = std::filesystem::path(value) / "Library/Application Support";
    }
#else
    if (const char* value = std::getenv("XDG_DATA_HOME")) {
        root = value;
    }
    if (root.empty()) {
        if (const char* value = std::getenv("HOME")) {
            root = std::filesystem::path(value) / ".local/share";
        }
    }
#endif
    if (root.empty() || !root.is_absolute()) {
        throw std::runtime_error("no absolute user data directory for weapon XP");
    }
    return root / "OpenRAC" / "weapon-xp" / (disc + "-" + profile + ".wxp");
}

}  // namespace weaponxp

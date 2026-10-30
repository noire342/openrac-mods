// SPDX-License-Identifier: GPL-3.0-or-later
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
namespace skillpoints {
FileStore::FileStore(const std::filesystem::path& path, Ledger& ledger) : m_path(path) {
    std::filesystem::create_directories(path.parent_path());
    auto lock = path;
    lock += ".lock";
#if defined(_WIN32)
    auto h = CreateFileW(
        lock.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Skill points profile already open or inaccessible");
    }
    m_lock = reinterpret_cast<std::intptr_t>(h);
#else
    int h = ::open(lock.c_str(), O_CREAT | O_RDWR, 0600);
    if (h < 0) {
        throw std::runtime_error("Cannot lock skill points profile");
    }
    if (flock(h, LOCK_EX | LOCK_NB) != 0) {
        ::close(h);
        throw std::runtime_error("Skill points profile already open");
    }
    m_lock = h;
#endif
    try {
        if (std::filesystem::exists(path)) {
            if (std::filesystem::file_size(path) > 32768) {
                throw std::runtime_error("Invalid profile; file preserved");
            }
            std::ifstream in(path, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(in)), {});
            if (!in.is_open() || in.bad()) {
                throw std::runtime_error("Cannot read skill points profile");
            }
            ledger = Ledger::parse(data);
        }
    } catch (...) {
#if defined(_WIN32)
        CloseHandle(reinterpret_cast<HANDLE>(m_lock));
#else
        ::close(static_cast<int>(m_lock));
#endif
        m_lock = -1;
        throw;
    }
}

FileStore::~FileStore() {
    if (m_lock == -1) {
        return;
    }
#if defined(_WIN32)
    CloseHandle(reinterpret_cast<HANDLE>(m_lock));
#else
    ::close(static_cast<int>(m_lock));
#endif
}

void FileStore::save(const Ledger& ledger) {
    auto temp = m_path;
    temp += ".tmp";
    const auto data = ledger.serialize();
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    out.close();
    if (!out) {
        throw std::runtime_error("Cannot write skill points; previous file preserved");
    }
#if defined(_WIN32)
    if (!MoveFileExW(
            temp.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        throw std::runtime_error("Cannot replace skill points; previous file preserved");
    }
#else
    int fd = ::open(temp.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Cannot flush skill points");
    }
    int result = fsync(fd);
    ::close(fd);
    if (result != 0) {
        throw std::runtime_error("Cannot flush skill points");
    }
    std::filesystem::rename(temp, m_path);
#endif
}

std::filesystem::path profile_path(const std::string& profile) {
    if (profile.empty() || profile.size() > 32
        || profile.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
           ) != std::string::npos) {
        throw std::runtime_error(
            "Profile must contain 1-32 letters, digits, underscores or hyphens"
        );
    }
    std::filesystem::path root;
#if defined(_WIN32)
    wchar_t* value = nullptr;
    std::size_t count = 0;
    if (_wdupenv_s(&value, &count, L"LOCALAPPDATA") == 0 && value) {
        root = value;
    }
    std::free(value);
#elif defined(__APPLE__)
    if (auto* value = std::getenv("HOME")) {
        root = std::filesystem::path(value) / "Library/Application Support";
    }
#else
    if (auto* value = std::getenv("XDG_DATA_HOME")) {
        root = value;
    }
    if (root.empty()) {
        if (auto* value = std::getenv("HOME")) {
            root = std::filesystem::path(value) / ".local/share";
        }
    }
#endif
    if (root.empty() || !root.is_absolute()) {
        throw std::runtime_error("No absolute user data directory");
    }
    return root / "OpenRAC" / "skill-points-ra" / ("ntsc-" + profile + ".spra");
}
}  // namespace skillpoints

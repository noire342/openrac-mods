// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include <crt_externs.h>
#include <mach-o/dyld.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace revolverator {

inline std::vector<std::filesystem::path> darwin_process_arguments() {
    const int count = *_NSGetArgc();
    const auto* values = *_NSGetArgv();
    if (!values || count <= 0) return {};
    return std::vector<std::filesystem::path>(values, values + count);
}

inline std::filesystem::path darwin_executable_path() {
    // dyld reports the required buffer size if the application path is long.
    std::vector<char> path(1024);
    auto size = static_cast<std::uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0)
            throw std::runtime_error("Cannot read the macOS game executable path");
    }
    return std::filesystem::canonical(path.data());
}

}

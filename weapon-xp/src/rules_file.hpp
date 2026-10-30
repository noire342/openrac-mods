// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <filesystem>

#include "rules.hpp"

namespace weaponxp {
bool read_rules_file(
    const std::filesystem::path& path, Rules& rules, std::string& error, bool create
);
}

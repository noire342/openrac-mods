// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>

#include "rules.hpp"

namespace weaponxp {
bool read_rules_file(
    const std::filesystem::path& path, Rules& rules, std::string& error, bool create
);
}

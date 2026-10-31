// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "file_overlay_api.h"
#include <string>
#include <vector>
namespace file_overlay {
int install(const wchar_t* config_file,const char* api_version=nullptr) noexcept;
int remove() noexcept;
const char* last_error() noexcept;
int stats(file_overlay_stats_v1* out) noexcept;
std::wstring default_config_file();
void enable_diagnostics(bool enabled) noexcept;
std::vector<std::string> diagnostic_lines();
} // namespace file_overlay

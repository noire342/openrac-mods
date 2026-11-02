// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace file_overlay {
inline constexpr std::size_t RequiredAliases = 19;
inline constexpr std::size_t MaximumConfigBytes = 256 * 1024;
struct Alias { std::string original; std::string replacement; };
std::vector<Alias> parse_config(std::string_view json);
bool read_only_open(std::uint32_t access, std::uint32_t disposition, std::uint32_t flags) noexcept;
struct PathPair { std::wstring normalized_original; std::wstring replacement; };
class PathMap final {
public:
    explicit PathMap(std::vector<PathPair> aliases);
    const std::wstring* find(std::wstring_view normalized_original) const noexcept;
    std::size_t size() const noexcept { return aliases_.size(); }
private:
    std::vector<PathPair> aliases_;
};
} // namespace file_overlay

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "overlay_core.hpp"
#include <algorithm>
#include <stdexcept>

namespace file_overlay {
namespace {
class Parser {
    std::string_view json_;
    std::size_t at_{};
    [[noreturn]] void error(const char* text) const { throw std::runtime_error(text); }
    void space() { while (at_ < json_.size() && (json_[at_] == ' ' || json_[at_] == '\n' || json_[at_] == '\r' || json_[at_] == '\t')) ++at_; }
    bool accept(char value) { space(); if (at_ < json_.size() && json_[at_] == value) { ++at_; return true; } return false; }
    void require(char value) { if (!accept(value)) error("Invalid file-overlay JSON structure"); }
    unsigned hex4() {
        unsigned value{};
        for (unsigned i = 0; i < 4; ++i) {
            if (at_ == json_.size()) error("Truncated JSON Unicode escape");
            const char c = json_[at_++];
            const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10) : 16;
            if (digit == 16) error("Invalid JSON Unicode escape");
            value = value * 16 + digit;
        }
        return value;
    }
    void unicode(std::string& out, unsigned code) {
        if (code >= 0xd800 && code <= 0xdbff) {
            if (at_ + 2 > json_.size() || json_[at_] != '\\' || json_[at_ + 1] != 'u') error("Unpaired JSON Unicode surrogate");
            at_ += 2;
            const auto low = hex4();
            if (low < 0xdc00 || low > 0xdfff) error("Unpaired JSON Unicode surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
        } else if (code >= 0xdc00 && code <= 0xdfff) error("Unpaired JSON Unicode surrogate");
        if (!code) error("NUL bytes are forbidden in alias paths");
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 63)); }
        else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
        else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 63)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
    }
    std::string string() {
        require('"'); std::string out;
        while (at_ < json_.size()) {
            const unsigned char c = static_cast<unsigned char>(json_[at_++]);
            if (c == '"') { if (out.empty()) error("Empty alias paths are forbidden"); return out; }
            if (c < 32) error("Control bytes are forbidden in JSON strings");
            if (c != '\\') out += static_cast<char>(c);
            else {
                if (at_ == json_.size()) error("Truncated JSON escape");
                const char escaped = json_[at_++];
                switch (escaped) {
                case '"': case '\\': case '/': out += escaped; break;
                case 'u': unicode(out, hex4()); break;
                default: error("Only path-safe JSON escapes are allowed");
                }
            }
            if (out.size() > 32768) error("Alias path exceeds the bounded JSON string size");
        }
        error("Unterminated JSON string");
    }
    Alias alias() {
        require('{'); Alias result; bool original{}, replacement{};
        do {
            const auto key = string(); require(':'); const auto value = string();
            if (key == "original" && !original) { original = true; result.original = value; }
            else if (key == "replacement" && !replacement) { replacement = true; result.replacement = value; }
            else error("Unknown or duplicate alias key");
        } while (accept(','));
        require('}');
        if (!original || !replacement) error("Each alias requires original and replacement");
        return result;
    }
public:
    explicit Parser(std::string_view json) : json_(json) {
        if (json_.size() > MaximumConfigBytes) error("File-overlay JSON exceeds256KiB");
        if (json_.size() >= 3 && json_.substr(0, 3) == "\xef\xbb\xbf") at_ = 3;
    }
    std::vector<Alias> parse() {
        require('{'); std::vector<Alias> aliases; bool format{}, list{};
        do {
            const auto key = string(); require(':');
            if (key == "format" && !format) { require('1'); format = true; }
            else if (key == "aliases" && !list) {
                list = true; require('[');
                if (!accept(']')) {
                    do { if (aliases.size() == RequiredAliases) error("Exactly19 aliases are required"); aliases.push_back(alias()); } while (accept(','));
                    require(']');
                }
            } else error("Unknown or duplicate file-overlay configuration key");
        } while (accept(','));
        require('}'); space();
        if (at_ != json_.size() || !format || !list || aliases.size() != RequiredAliases) error("A format1 configuration with exactly19 aliases is required");
        return aliases;
    }
};
}
std::vector<Alias> parse_config(std::string_view json) { return Parser(json).parse(); }
bool read_only_open(std::uint32_t access, std::uint32_t disposition, std::uint32_t flags) noexcept {
    // Stable Win32 API masks, kept portable for host policy tests.
    constexpr std::uint32_t Allowed = 0x80000000u | 0x1u | 0x8u | 0x80u | 0x20000u | 0x100000u;
    return disposition == 3 && (access & (0x80000000u | 1u)) && !(access & ~Allowed) && !(flags & 0x04000000u);
}
PathMap::PathMap(std::vector<PathPair> aliases) : aliases_(std::move(aliases)) {
    if (aliases_.size() != RequiredAliases) throw std::runtime_error("Exactly19 normalized aliases are required");
    std::sort(aliases_.begin(), aliases_.end(), [](const PathPair& a, const PathPair& b) { return a.normalized_original < b.normalized_original; });
    for (std::size_t i = 0; i < aliases_.size(); ++i) {
        const auto& alias = aliases_[i];
        if (alias.normalized_original.empty() || alias.replacement.empty()) throw std::runtime_error("Alias paths must not be empty");
        if (alias.normalized_original == alias.replacement) throw std::runtime_error("A file cannot alias itself");
        if (i && alias.normalized_original == aliases_[i - 1].normalized_original) throw std::runtime_error("Duplicate normalized original alias path");
    }
}
const std::wstring* PathMap::find(std::wstring_view original) const noexcept {
    const auto it = std::lower_bound(aliases_.begin(), aliases_.end(), original,
        [](const PathPair& a, std::wstring_view b) { return a.normalized_original < b; });
    return it != aliases_.end() && it->normalized_original == original ? &it->replacement : nullptr;
}
} // namespace file_overlay

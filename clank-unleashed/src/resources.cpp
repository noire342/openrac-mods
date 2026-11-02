// SPDX-License-Identifier: GPL-3.0-or-later
#include "resources.hpp"
#include "common/json.h"
#include "windows_overlay.hpp"
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
namespace clank {
namespace {
using Value = openrac::json::Value;
using Path = std::filesystem::path;
std::string utf8(const Path &path) {
    auto s = path.generic_u8string();
    return {s.begin(), s.end()};
}
std::string quote(const std::string &input) {
    std::string out = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : input) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += static_cast<char>(c);
    }
    return out + '"';
}
std::string serialize(const Value &v) {
    switch (v.type()) {
    case Value::Type::Null:
        return "null";
    case Value::Type::Bool:
        return v.boolean() ? "true" : "false";
    case Value::Type::Number: {
        const auto number = v.number();
        if (!std::isfinite(number))
            throw std::runtime_error("Non-finite manifest number");
        std::array<char, 64> buffer{};
        const auto result =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), number,
                          std::chars_format::general, std::numeric_limits<double>::max_digits10);
        if (result.ec != std::errc{})
            throw std::runtime_error("Cannot preserve manifest number");
        return {buffer.data(), result.ptr};
    }
    case Value::Type::String:
        return quote(v.string());
    case Value::Type::Array: {
        std::string s = "[";
        for (const auto &child : v.items()) {
            if (s.size() > 1)
                s += ',';
            s += serialize(child);
        }
        return s + ']';
    }
    case Value::Type::Object: {
        std::string s = "{";
        for (const auto &[key, child] : v.members()) {
            if (s.size() > 1)
                s += ',';
            s += quote(key) + ':' + serialize(child);
        }
        return s + '}';
    }
    }
    throw std::runtime_error("Unknown manifest JSON type");
}
std::string replace(const Value &v, const std::map<std::string, std::string> &overrides) {
    if (!v.is_object())
        throw std::runtime_error("Expected manifest object");
    std::string out = "{";
    for (const auto &[key, child] : v.members()) {
        if (out.size() > 1)
            out += ',';
        const auto it = overrides.find(key);
        out += quote(key) + ':' + (it == overrides.end() ? serialize(child) : it->second);
    }
    for (const auto &[key, value] : overrides)
        if (v.members().find(key) == v.members().end()) {
            if (out.size() > 1)
                out += ',';
            out += quote(key) + ':' + value;
        }
    return out + '}';
}
Value document(const Path &path) {
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
        throw std::runtime_error("Manifest size limit exceeded");
    std::ifstream in(path, std::ios::binary);
    std::string data{std::istreambuf_iterator<char>(in), {}};
    Value value;
    std::string error;
    if (!in || !openrac::json::parse(data, value, error))
        throw std::runtime_error("Cannot read manifest: " + utf8(path));
    return value;
}
std::string level(unsigned id) {
    return "level_" + (id < 10 ? std::string("0") : std::string{}) + std::to_string(id);
}
struct Arguments {
    LPWSTR *argv{};
    int argc{};
    Arguments() {
        argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv)
            throw std::runtime_error("Cannot read game paths");
    }
    ~Arguments() { LocalFree(argv); }
    Path get(const wchar_t *key) const {
        for (int i = 1; i + 1 < argc; ++i)
            if (std::wcscmp(argv[i], key) == 0)
                return std::filesystem::absolute(argv[i + 1]);
        return {};
    }
};
} // namespace
Resources::Resources() {
    try {
        Arguments args;
        const auto data = args.get(L"--data");
        auto levels = args.get(L"--levels");
        if (data.empty())
            throw std::runtime_error("Clank requires the launcher's --data path");
        if (levels.empty())
            levels = data / L"port/pal";
        const auto original = document(levels / "level_06/manifest.json");
        const auto &model = original["classes"]["moby"]["87"];
        if (!model.is_object() || !model["mesh"].is_string())
            throw std::runtime_error(
                "Playable Clank model is absent; read levels with OpenRAC 0.10.0 first");
        const auto mesh = levels / "level_06" / std::filesystem::u8path(model["mesh"].string());
        if (!std::filesystem::is_regular_file(mesh))
            throw std::runtime_error("Playable Clank mesh is missing");
        m_payload = read_clank_class(data / L"iso_data/rac1/disc.iso");
        m_gadgebot_icon = read_gadgebot_icon(data / L"iso_data/rac1/disc.iso");
        m_alarm_source = read_alarm_source(data / L"iso_data/rac1/disc.iso");
        wchar_t local[32768]{};
        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768))
            throw std::runtime_error("LOCALAPPDATA is unavailable");
        static std::atomic<unsigned> sequence{};
        m_session = Path(local) / L"OpenRAC/clank-unleashed/cache" /
                    (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++sequence));
        if (!std::filesystem::create_directories(m_session))
            throw std::runtime_error("Clank session directory already exists");
        m_owns_session = true;
        std::string aliases = "{\"format\":1,\"aliases\":[";
        for (unsigned id = 0; id < 19; ++id) {
            const auto source = std::filesystem::absolute(levels / level(id) / "manifest.json");
            const auto current = document(source);
            if (current["format"].number() != 4 || current["version"].string() != "pal" ||
                current["level"].number() != id)
                throw std::runtime_error("Expected PAL format 4 level manifests");
            const auto &classes = current["classes"];
            const auto &mobys = classes["moby"];
            const auto patched =
                mobys["87"].is_object()
                    ? serialize(current)
                    : replace(
                          current,
                          {{"classes",
                            replace(
                                classes,
                                {{"moby",
                                  replace(mobys,
                                          {{"87",
                                            replace(model, {{"mesh", quote(utf8(mesh))}})}})}})}});
            const auto dest = m_session / (level(id) + ".json");
            m_files.push_back(dest);
            std::ofstream out(dest, std::ios::binary);
            out << patched;
            out.close();
            if (!out)
                throw std::runtime_error("Cannot stage Clank manifest");
            if (id)
                aliases += ',';
            aliases += "{\"original\":" + quote(utf8(source)) +
                       ",\"replacement\":" + quote(utf8(dest)) + "}";
        }
        aliases += "]}";
        const auto config = m_session / "file-overlay.json";
        m_files.push_back(config);
        std::ofstream out(config, std::ios::binary);
        out << aliases;
        out.close();
        if (!out)
            throw std::runtime_error("Cannot stage Clank aliases");
        const auto installed = file_overlay::install(config.c_str());
        if (installed != FILE_OVERLAY_OK)
            throw std::runtime_error(file_overlay::last_error());
        m_installed = true;
    } catch (...) {
        if (file_overlay::remove() == FILE_OVERLAY_OK) {
            for (const auto &file : m_files) {
                std::error_code ec;
                std::filesystem::remove(file, ec);
            }
            if (m_owns_session) {
                std::error_code ec;
                std::filesystem::remove(m_session, ec);
            }
        }
        throw;
    }
}
Resources::~Resources() {
    if (m_installed && file_overlay::remove() != FILE_OVERLAY_OK)
        return;
    for (const auto &file : m_files) {
        std::error_code ec;
        std::filesystem::remove(file, ec);
    }
    if (m_owns_session) {
        std::error_code ec;
        std::filesystem::remove(m_session, ec);
    }
}
} // namespace clank

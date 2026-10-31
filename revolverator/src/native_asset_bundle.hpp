// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace openrac::json { class Value; }

namespace revolverator {
namespace asset_bundle {
// Independent serialization of the read-only OpenRAC JSON tree. Numeric
// values round-trip exactly; strings retain their UTF-8 content and escapes.
std::string serialize_json(const openrac::json::Value&);

// Same path precedence as the native program: the last --levels wins;
// otherwise the last --data (or its native environment fallback) supplies
// port/<version>. Paths are resolved against the caller's current directory.
std::filesystem::path levels_from_arguments(
    const std::vector<std::filesystem::path>& arguments,
    const std::filesystem::path& environment_data = {},
    const char* version = "ntsc");
}

class NativeAssetBundle {
public:
    struct Paths {
        std::filesystem::path levels_root;
        std::filesystem::path mod_root;
        // Parent of the unique owned session directory, not a source root.
        std::filesystem::path cache_root;
    };

    // Windows: derive roots from the actual process command line, this module
    // and LOCALAPPDATA. Throws before publishing a configuration on any error.
    explicit NativeAssetBundle(unsigned required_format=0,const char* version="ntsc");
    // Explicit roots provide the same staging contract for host tests/tools.
    explicit NativeAssetBundle(Paths,unsigned required_format=0,const char* version="ntsc");
    ~NativeAssetBundle() noexcept;
    NativeAssetBundle(const NativeAssetBundle&) = delete;
    NativeAssetBundle& operator=(const NativeAssetBundle&) = delete;
    NativeAssetBundle(NativeAssetBundle&&) = delete;
    NativeAssetBundle& operator=(NativeAssetBundle&&) = delete;

    std::filesystem::path configuration() const { return configuration_; }

private:
    void cleanup() noexcept;
    void write_owned(std::filesystem::path, const std::string&);
    std::filesystem::path cache_root_;
    std::filesystem::path session_;
    std::filesystem::path configuration_;
    std::vector<std::filesystem::path> files_;
    bool owns_session_ = false;
};
}

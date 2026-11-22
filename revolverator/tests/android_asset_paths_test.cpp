// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "android_asset_paths.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
using revolverator::android_asset_paths;
namespace fs = std::filesystem;

std::string plugin_directory;

int provide_plugin_directory(char* output, std::uint32_t capacity) {
    if (plugin_directory.size() >= capacity)
        return static_cast<int>(capacity);
    std::memcpy(output, plugin_directory.data(), plugin_directory.size() + 1);
    return static_cast<int>(plugin_directory.size());
}

void require(bool passed, const char* message) {
    if (passed) return;
    std::fprintf(stderr, "Android asset paths: %s\n", message);
    std::exit(1);
}

void require_refusal(const openrac_api* api, const char* version, const char* message) {
    try {
        (void)android_asset_paths(api, version);
    } catch (const std::runtime_error&) {
        return;
    }
    require(false, message);
}
}

int main() {
    const fs::path data = "/data/user/0/dev.openrac.launcher/files/games/active/rac1/data";
    const fs::path native = "/data/app/dev.openrac.launcher/lib/arm64";
    plugin_directory = (data / "plugins").string();
    require(setenv("OPENRAC_NATIVE_LIBRARY_DIR", native.c_str(), 1) == 0, "set the APK library folder");
    require(setenv("PYTHONHOME", "/data/user/0/dev.openrac.launcher/files/openrac/python", 1) == 0,
            "set the APK Python home");

    openrac_api api{};
    api.size = sizeof(api);
    api.api_major = 1;
    api.api_minor = 20;
    api.plugins_folder = provide_plugin_directory;
    for (const auto* version : {"pal", "ntsc"}) {
        const auto paths = android_asset_paths(&api, version);
        require(paths.levels_root == data / "port" / version, "use the selected disc's extracted levels");
        require(paths.mod_root == data / "plugins/revolverator", "use the loader's installed mod directory");
        require(paths.cache_root == data / "cache/revolverator/assets", "keep the cache outside the plugin and levels");
        require(paths.preparation_python == native / "libopenrac-python.so", "use the launcher-bundled interpreter");
        require(paths.use_openrac_assets, "use the official OpenRAC asset path");
    }
    require_refusal(&api, "jp", "refuse unsupported disc");
    require_refusal(nullptr, "pal", "refuse missing SDK");
    api.size = offsetof(openrac_api, plugins_folder);
    require_refusal(&api, "pal", "refuse a truncated SDK table");
    api.size = sizeof(api);
    plugin_directory = "relative/plugins";
    require_refusal(&api, "ntsc", "refuse a relative plugin directory");
    plugin_directory.assign(5000, 'x');
    require_refusal(&api, "ntsc", "refuse a plugin path longer than the SDK output buffer");
    plugin_directory = (data / "plugins").string();
    plugin_directory.insert(plugin_directory.begin() + 1, '\0');
    require_refusal(&api, "ntsc", "refuse a plugin directory with an embedded terminator");
    plugin_directory = (data / "plugins").string();
    require(unsetenv("PYTHONHOME") == 0, "clear the APK Python home");
    require_refusal(&api, "ntsc", "require the bundled Python home");
}

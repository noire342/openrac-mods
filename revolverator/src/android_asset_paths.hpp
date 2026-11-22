// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include "native_asset_bundle.hpp"

#include <openrac/openrac_mod.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace revolverator {

inline NativeAssetBundle::Paths android_asset_paths(const openrac_api* api, const char* version) {
    namespace fs = std::filesystem;
    if (!version || (std::string_view(version) != "pal" && std::string_view(version) != "ntsc"))
        throw std::runtime_error("Revolverator: expected a supported Ratchet & Clank disc");

    // Android passes the launcher's arguments to SDL rather than the app process.
    // The SDK's plugin list identifies the launcher data directory for this game.
    constexpr auto required = offsetof(openrac_api, plugins_folder) + sizeof(openrac_api{}.plugins_folder);
    std::array<char, 4096> plugins_folder{};
    if (!api || api->api_major != 1 || api->api_minor < 14 || api->size < required ||
        !api->plugins_folder || api->plugins_folder(plugins_folder.data(), plugins_folder.size()) <= 0)
        throw std::runtime_error("Revolverator: the Android launcher did not provide the game's plugin directory");

    const fs::path plugins = plugins_folder.data();
    if (!plugins.is_absolute() || plugins.filename() != "plugins")
        throw std::runtime_error("Revolverator: unexpected Android launcher plugin directory");
    const auto data = plugins.parent_path();

    // Executable libraries are installed in the APK's native library directory.
    // The game's launch environment supplies PYTHONHOME for the bundled stdlib.
    const auto* native_dir = std::getenv("OPENRAC_NATIVE_LIBRARY_DIR");
    const auto* python_home = std::getenv("PYTHONHOME");
    if (!native_dir || !fs::path(native_dir).is_absolute() ||
        !python_home || !fs::path(python_home).is_absolute())
        throw std::runtime_error("Revolverator: use the official Android launcher to prepare local graphics");

    return {data / "port" / version, {}, data / "cache/revolverator/assets",
            fs::path(native_dir) / "libopenrac-python.so", true};
}

}

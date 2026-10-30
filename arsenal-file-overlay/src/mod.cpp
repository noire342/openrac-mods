// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include <openrac/openrac_mod.h>
#include "windows_overlay.hpp"
#include <cstring>
#include <exception>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {
struct DiagnosticContext { const openrac_api* api{}; openrac_mod* mod{}; unsigned frames{}; } diagnostic;
void diagnostic_frame(void*) noexcept {
    if (!diagnostic.api) return;
    ++diagnostic.frames;
    if (diagnostic.frames != 1 && diagnostic.frames != 60 && diagnostic.frames % 300) return;
    try {
        for (const auto& line : file_overlay::diagnostic_lines())
            if (diagnostic.api->log) diagnostic.api->log(diagnostic.mod, OPENRAC_LOG_INFO, line.c_str());
    } catch (...) {}
}
}

extern "C" {
OPENRAC_MOD_DEFINE("arsenal-file-overlay", "0.1.0")
OPENRAC_MOD_EXPORT int openrac_file_overlay_install(const wchar_t* config_file) {
    return file_overlay::install(config_file);
}
OPENRAC_MOD_EXPORT int openrac_file_overlay_remove() { return file_overlay::remove(); }
OPENRAC_MOD_EXPORT const char* openrac_file_overlay_last_error() { return file_overlay::last_error(); }
OPENRAC_MOD_EXPORT int openrac_file_overlay_stats(file_overlay_stats_v1* out) { return file_overlay::stats(out); }
OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    try {
        if (!api || !api->game || !api->version || !api->platform || std::strcmp(api->game, "rac1") ||
            std::strcmp(api->version, "ntsc") || std::strcmp(api->platform, "windows-x86_64")) {
            if (api && api->fail) api->fail(mod, "File-overlay POC requires the official R&C1 NTSC-U Windows x64 client");
            return 1;
        }
        const auto path = file_overlay::default_config_file();
        wchar_t trace[2]{};
        const bool diagnostics = GetEnvironmentVariableW(L"OPENRAC_FILE_OVERLAY_DIAGNOSTICS", trace, 2) == 1 && trace[0] == L'1';
        file_overlay::enable_diagnostics(diagnostics);
        const auto result = file_overlay::install(path.c_str());
        if (result) { if (api->fail) api->fail(mod, file_overlay::last_error()); return 1; }
        if (api->log) api->log(mod, OPENRAC_LOG_INFO, file_overlay::last_error());
        if (diagnostics) {
            diagnostic = {api, mod, 0};
            if (!api->on_frame || api->on_frame(mod, diagnostic_frame, nullptr)) {
                diagnostic = {};
                file_overlay::remove();
                if (api->fail) api->fail(mod, "Cannot register explicit file-overlay QA diagnostics");
                return 1;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        if (api && api->fail) api->fail(mod, error.what());
        return 1;
    } catch (...) {
        if (api && api->fail) api->fail(mod, "File-overlay POC could not prepare its immutable alias map");
        return 1;
    }
}
OPENRAC_MOD_EXPORT void openrac_mod_unload() { diagnostic = {}; file_overlay::remove(); }
}

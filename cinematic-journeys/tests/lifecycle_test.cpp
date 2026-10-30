// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"

#include <openrac/openrac_mod.h>

extern "C" int openrac_mod_load(const openrac_api*, openrac_mod*);
extern "C" void openrac_mod_unload();

namespace {
std::vector<std::uint8_t> ram(0x02000000);
int hook_result = 0, pad_result = 0;
std::string failure;

void original(std::uint32_t, std::uint32_t, std::int32_t) {}

int hook(openrac_mod*, const char*, const char*, void*, void** out) {
    *out = reinterpret_cast<void*>(original);
    return hook_result;
}

int pad(openrac_mod*, openrac_pad_fn, void*) {
    return pad_result;
}

void log(openrac_mod*, int, const char*) {}

void fail(openrac_mod*, const char* text) {
    failure = text;
}

int boolean(openrac_mod*, const char*) {
    return 1;
}

double number(openrac_mod*, const char*) {
    return 5;
}

void* function(const char*) {
    return reinterpret_cast<void*>(original);
}

std::uint32_t data(std::uint32_t at) {
    return at;
}

std::uint32_t level_data(int, std::uint32_t at) {
    return at;
}

int loaded() {
    return -1;
}
}  // namespace

int main() {
    CHECK(openrac_mod_load(nullptr, nullptr) != 0);
    openrac_api api{};
    api.size = 4;
    CHECK(openrac_mod_load(&api, nullptr) != 0);
    api.size = sizeof(api);
    api.api_major = 1;
    api.api_minor = 1;
    api.game = "rac1";
    api.version = "ntsc";
    api.memory = ram.data();
    api.log = log;
    api.fail = fail;
    api.setting_bool = boolean;
    api.setting_float = number;
    api.hook = hook;
    api.on_pad = pad;
    api.function = function;
    api.relocate_data = data;
    api.relocate_level_data = level_data;
    api.loaded_level = loaded;
    auto invalid = api;
    invalid.setting_float = nullptr;
    CHECK(openrac_mod_load(&invalid, nullptr) != 0 && !failure.empty());
    hook_result = OPENRAC_HOOK_UNKNOWN;
    CHECK(openrac_mod_load(&api, nullptr) != 0);
    hook_result = 0;
    pad_result = OPENRAC_HOOK_TOO_LATE;
    CHECK(openrac_mod_load(&api, nullptr) != 0);
    pad_result = 0;
    failure.clear();
    CHECK(openrac_mod_load(&api, nullptr) == 0 && failure.empty());
    openrac_mod_unload();
    CHECK(openrac_mod_load(&api, nullptr) == 0);
    openrac_mod_unload();
}

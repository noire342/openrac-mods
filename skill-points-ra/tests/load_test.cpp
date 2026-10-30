#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "skill_points_api.h"

#include <openrac/openrac_mod.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

namespace {
std::vector<std::uint8_t> ram(0x02000000);
std::vector<std::pair<openrac_frame_fn, void*>> callbacks;

void tick() {
    const auto copy = callbacks;
    for (const auto& c : copy) {
        c.first(c.second);
    }
}

unsigned observer_unlocks = 0, observer_stops = 0;
int level = -1;
std::string failed;
unsigned unlocked = 0, stopped = 0;
const sp_api* service = nullptr;
bool kerwan_enabled = false;

std::uint32_t dummy_loader(std::int32_t) {
    return 42;
}

int hook(openrac_mod*, const char* name, const char* signature, void*, void** original) {
    CHECK(std::strcmp(name, "rac1.level.load_gameplay") == 0);
    if (std::strcmp(signature, "u32(i32)")) {
        return OPENRAC_HOOK_SIGNATURE;
    }
    *original = reinterpret_cast<void*>(dummy_loader);
    return 0;
}

int on_frame(openrac_mod*, openrac_frame_fn fn, void* context) {
    callbacks.emplace_back(fn, context);
    return 0;
}

void log(openrac_mod*, int, const char* text) {
    if (!std::strcmp(text, "observer: achievement unlocked")) {
        ++observer_unlocks;
    }
    if (!std::strcmp(text, "observer: producer stopping")) {
        ++observer_stops;
    }
}

void fail(openrac_mod*, const char* text) {
    failed = text;
}

const char* string_setting(openrac_mod*, const char*) {
    return "qa-test";
}

int bool_setting(openrac_mod*, const char* key) {
    return !std::strcmp(key, "kerwan_time_trial") && kerwan_enabled;
}

void* function(const char*) {
    return reinterpret_cast<void*>(dummy_loader);
}

void* function_at(openrac_gaddr) {
    return reinterpret_cast<void*>(dummy_loader);
}

openrac_gaddr relocate(openrac_gaddr a) {
    return a;
}

int loaded() {
    return level;
}

openrac_gaddr push(std::uint32_t, openrac_gaddr* saved) {
    *saved = 0;
    return 0x01F00000;
}

void pop(openrac_gaddr) {}

template <class T>
void set(std::uint32_t a, T v) {
    std::memcpy(ram.data() + a, &v, sizeof v);
}

void listener(const sp_event* event, void*) {
    sp_totals totals{};
    totals.size = sizeof(totals);
    CHECK(service->totals(&totals) == SP_OK);
    if (event->type == SP_EVENT_UNLOCKED) {
        ++unlocked;
        CHECK(event->id == 254397);
        CHECK(totals.unlocked == 1);
        std::uint32_t token = 0;
        CHECK(service->subscribe(listener, nullptr, &token) == SP_BUSY);
    }
    if (event->type == SP_EVENT_STOPPING) {
        ++stopped;
    }
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CHECK(argc >= 2);
        const auto root =
            std::filesystem::temp_directory_path()
            / ("skill-points-dll-test-"
               + std::to_string(
                   std::chrono::high_resolution_clock::now().time_since_epoch().count()
               ));
#if defined(_WIN32)
        SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str());
        auto library = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
        CHECK(library);
        auto symbol = [&](const char* name) {
            return reinterpret_cast<void*>(GetProcAddress(library, name));
        };
#else
        setenv("XDG_DATA_HOME", root.c_str(), 1);
        auto library = dlopen(argv[1], RTLD_NOW);
        CHECK(library);
        auto symbol = [&](const char* name) {
            return dlsym(library, name);
        };
#endif
        const auto load =
            reinterpret_cast<int (*)(const openrac_api*, openrac_mod*)>(symbol("openrac_mod_load"));
        const auto unload = reinterpret_cast<void (*)()>(symbol("openrac_mod_unload"));
        const auto query = reinterpret_cast<sp_query_fn>(symbol("openrac_skill_points_query"));
        CHECK(load && unload && query);
        CHECK(!query(1));
        openrac_api api{};
        api.size = sizeof(api);
        api.api_major = 1;
        api.api_minor = 4;
        api.game = "rac1";
        api.version = "ntsc";
        api.memory = ram.data();
        api.log = log;
        api.fail = fail;
        api.setting_string = string_setting;
        api.setting_bool = bool_setting;
        api.hook = hook;
        api.on_frame = on_frame;
        api.function = function;
        api.function_at = function_at;
        api.relocate_data = relocate;
        api.loaded_level = loaded;
        api.stack_push = push;
        api.stack_pop = pop;
        CHECK(load(nullptr, nullptr) != 0 && !query(1));
        auto incomplete = api;
        incomplete.size = 4;
        CHECK(load(&incomplete, nullptr) != 0 && !query(1));
        incomplete = api;
        incomplete.hook = nullptr;
        CHECK(load(&incomplete, nullptr) != 0 && !query(1));
        incomplete = api;
        incomplete.setting_bool = nullptr;
        CHECK(load(&incomplete, nullptr) != 0 && !query(1));
        incomplete = api;
        incomplete.fail = nullptr;
        CHECK(load(&incomplete, nullptr) != 0 && !query(1));
        failed.clear();
        CHECK(load(&api, nullptr) == 0);
        service = query(1);
        CHECK(service && !query(2));
#if defined(_WIN32)
        CHECK(argc == 3);
        auto observer = LoadLibraryW(std::filesystem::path(argv[2]).c_str());
        CHECK(observer);
        auto observer_load = reinterpret_cast<int (*)(const openrac_api*, openrac_mod*)>(
            GetProcAddress(observer, "openrac_mod_load")
        );
        auto observer_unload =
            reinterpret_cast<void (*)()>(GetProcAddress(observer, "openrac_mod_unload"));
        CHECK(observer_load && observer_unload && observer_load(&api, nullptr) == 0);
        tick();  // C consumer attaches to the already loaded producer.
#endif
        sp_totals totals{};
        totals.size = sizeof(totals);
        CHECK(service->totals(&totals) == 0);
        CHECK(totals.count == 178 && totals.points == 667 && totals.unlocked == 0);
        CHECK(service->is_enabled(264462) == 0 && service->is_enabled(254397) == 1);
        CHECK(service->is_enabled(1) == SP_NOT_FOUND);
        CHECK(
            service->active_totals(&totals) == SP_OK && totals.count == 177 && totals.points == 657
        );
        kerwan_enabled = true;
        tick();
        CHECK(service->is_enabled(264462) == 1);
        CHECK(
            service->active_totals(&totals) == SP_OK && totals.count == 178 && totals.points == 667
        );
        kerwan_enabled = false;
        tick();
        CHECK(service->active_totals(&totals) == SP_OK && totals.count == 177);
        sp_achievement item{};
        item.size = sizeof(item);
        CHECK(service->get(254397, &item) == 0 && item.id == 254397);
        CHECK(service->at(178, &item) == SP_NOT_FOUND);
        CHECK(service->get(1, &item) == SP_NOT_FOUND);
        item.size = 0;
        CHECK(service->get(254397, &item) == SP_INVALID);
        std::uint32_t token;
        CHECK(service->subscribe(listener, nullptr, &token) == 0);
        level = 1;
        set<std::uint8_t>(0x15ED84, 1);
        set<std::uint32_t>(0x15F5C4, 6);
        set<std::uint32_t>(0x141584, 0xffffffffu);
        tick();
        set<std::uint8_t>(0x13DD41, 1);
        tick();
        tick();
        CHECK(unlocked == 1);
        unload();
        CHECK(stopped == 1 && !query(1));
        CHECK(service->totals(&totals) == SP_UNAVAILABLE);
#if defined(_WIN32)
        CHECK(observer_unlocks == 1 && observer_stops == 1);
        observer_unload();
        FreeLibrary(observer);
#endif
        callbacks.clear();
        level = -1;
        CHECK(load(&api, nullptr) == 0);
        service = query(1);
        CHECK(service->totals(&totals) == 0 && totals.unlocked == 1);
        unload();
#if defined(_WIN32)
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        // Only the exact test files created above are removed.
        auto file = root / "OpenRAC/skill-points-ra/ntsc-qa-test.spra";
        std::filesystem::remove(file);
        file += ".lock";
        std::filesystem::remove(file);
        std::cout << "Real DLL load, versioned C API, listener reentry, STOPPING and persistent "
                     "profile reload passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

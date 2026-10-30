// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "discs.hpp"
#include "guest_memory.hpp"
#include "hud_addresses.hpp"
#include "openrac/openrac_mod.h"
#include "store.hpp"
#include "weapon_xp_api.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void* library(const char* path) {
    return LoadLibraryA(path);
}

void* symbol(void* lib, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
}
#else
#include <dlfcn.h>

void* library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* symbol(void* lib, const char* name) {
    return dlsym(lib, name);
}
#endif

using namespace weaponxp;
GuestMemory ram;
std::uint32_t guest_sp = 0xE0100000u;
const Disc* disc;
std::uint32_t hit_address = 0;
std::map<std::string, void*> hooks;
std::string failure;
openrac_frame_fn frame = nullptr;
bool missing_relay = false;
int loaded = 0, hit_calls = 0, death_calls = 0, hud_calls = 0, rectangles = 0, pads = 0;
const weapon_xp_api_v1* service;
int changed = 0, upgrades = 0, hits = 0;
int observed_levels = 0;
std::map<std::pair<std::string, std::string>, std::string> installed_contracts;
int counter_calls = 0;
int take_calls = 0, drone_spawns = 0;
bool hud_enabled = true, scale_damage = false, scale_capacity = false;
std::string test_profile = "integration";
double test_xp_rate = 1;
std::string glyph_text;

std::uint64_t texture(int index) {
    CHECK(index == 2);
    return 0x8000000123456789ull;
}

void quad(
    int, int, int w, int h, int u, int, int uw, int uh, std::uint64_t color, std::uint64_t tex
) {
    CHECK(w == 10 && h == 10 && uw == 16 && uh == 16);
    CHECK((color >> 24) <= 128 && tex == 0x8000000123456789ull);
    glyph_text += static_cast<char>(u);
}

template <class T>
void put(std::uint32_t address, T value) {
    CHECK(address + sizeof(T) <= ram.size());
    std::memcpy(ram.data() + address, &value, sizeof(T));
}

template <class T>
T get(std::uint32_t address) {
    T value;
    std::memcpy(&value, ram.data() + address, sizeof(T));
    return value;
}

std::uint32_t exe(std::uint32_t address) {
    return address + 0x200;
}

std::uint32_t level_data(int level, std::uint32_t address) {
    CHECK(level == 0);
    return address + 0x4000;
}

std::uint32_t ov(std::uint32_t address) {
    return level_data(0, address);
}

void host_log(openrac_mod*, int, const char* text) {
    if (std::strcmp(text, "observer: weapon upgraded") == 0) {
        ++observed_levels;
    }
}

void host_fail(openrac_mod*, const char* text) {
    failure = text;
}

int host_bool(openrac_mod*, const char* key) {
    if (std::strcmp(key, "scale_damage") == 0) {
        return scale_damage;
    }
    if (std::strcmp(key, "scale_capacity") == 0) {
        return scale_capacity;
    }
    if (std::strcmp(key, "level_up_animation") == 0 || std::strcmp(key, "versioned_names") == 0) {
        return true;
    }
    return hud_enabled && std::strcmp(key, "hud") == 0;
}

int original_take_ammo(int, int) {
    ++take_calls;
    return 1;
}

std::uint32_t spawn_drone(int mode, std::uint32_t pos) {
    CHECK(mode == 0 && pos >= 0xE0000000u);
    const auto moby = 0x450000 + 0x100 * drone_spawns++;
    put<unsigned>(moby + 0x78, 0x460000 + 0x80 * drone_spawns);
    auto at = exe(disc->hero) + 0x1FF6;
    put<std::uint8_t>(at, get<std::uint8_t>(at) + 1);
    return moby;
}

double host_float(openrac_mod*, const char* key) {
    if (std::strcmp(key, "max_kill_xp") == 0) {
        return 500;
    }
    return test_xp_rate;
}

const char* host_string(openrac_mod*, const char* key) {
    if (std::strcmp(key, "rules_file") == 0) {
        return "rules.ini";
    }
    return test_profile.c_str();
}

int host_level() {
    return loaded;
}

std::uint32_t original_hit(std::uint32_t, std::int32_t, std::int32_t) {
    ++hit_calls;
    return hit_address;
}

void original_death(std::uint32_t, std::int32_t, std::int32_t) {
    ++death_calls;
}

std::int32_t original_hud(std::uint32_t) {
    ++hud_calls;
    return 72;
}

std::int32_t original_gauge(std::uint32_t element) {
    put<int>(element + 0x58, 64);
    put<int>(element + 0x5C, 256);
    return 64;
}

int position(std::uint32_t, std::uint32_t x, std::uint32_t y) {
    CHECK(x >= 0xE0000000u && y < 0xE0100000u);
    put<int>(x, get<int>(x) + 100);
    put<int>(y, get<int>(y) + 200);
    return 0;
}

void transform(std::uint32_t, std::uint32_t x, std::uint32_t y, int, int) {
    put<int>(x, get<int>(x) + 1);
    put<int>(y, get<int>(y) + 2);
}

void original_counter(std::uint32_t) {
    ++counter_calls;
}

void relay_dispatch(std::uint32_t element) {
    const auto found = hooks.find("rac1.moby.bolt.update");
    if (found != hooks.end()) {
        reinterpret_cast<void (*)(std::uint32_t)>(found->second)(element);
    } else {
        original_counter(element);
    }
}

void base_ammo() {
    for (const auto& weapon : kWeapons) {
        put<std::uint16_t>(ov(disc->item_table) + weapon.item * 24 + 14, 40);
    }
}

std::uint32_t original_load(std::int32_t level) {
    loaded = level;
    base_ammo();
    return 0x123456;
}

int host_hook(
    openrac_mod*, const char* name, const char* signature, void* replacement, void** original
) {
    const std::string key(name);
    const std::map<std::string, std::string> names{
        {"rac1.moby.bolt.update", "Moby_Bolt_Update"},
        {"rac1.combat.get_hit", "Moby_GetHitMessage"},
        {"rac1.moby.record_death", "Moby_RecordDeath"},
        {"rac1.level.load_gameplay", "Level_LoadGameplay"},
        {"rac1.weapon.take_ammo", "Weapon_TakeAmmo"},
        {"rac1.moby.hero_bomb.update", "Moby_Class121_Update"},
        {"rac1.moby.moby305.update", "Moby_Class305_Update"},
        {"rac1.moby.morphoray.update", "Moby_Morphoray_Update"}
    };
    const auto named = names.find(key);
    if (named != names.end()
        && installed_contracts.at({disc->version, named->second}) != signature) {
        return OPENRAC_HOOK_SIGNATURE;
    }
    if (key == "rac1.moby.morphoray.update") {
        *original =
            reinterpret_cast<void*>(+[](unsigned moby) { CHECK(get<float>(moby + 0xC0) == 1.0f); });
    } else if (key == "rac1.weapon.take_ammo") {
        *original = reinterpret_cast<void*>(original_take_ammo);
    } else if (key == "rac1.moby.hero_bomb.update" || key == "rac1.moby.moby305.update") {
        *original = reinterpret_cast<void*>(+[](unsigned) {});
    } else if (key == "rac1.moby.bolt.update") {
        if (missing_relay) {
            return OPENRAC_HOOK_UNKNOWN;
        }
        CHECK(std::strcmp(signature, "void(u32)") == 0);
        *original = reinterpret_cast<void*>(original_counter);
    } else if (key == "rac1.combat.get_hit") {
        *original = reinterpret_cast<void*>(original_hit);
    } else if (key == "rac1.moby.record_death") {
        *original = reinterpret_cast<void*>(original_death);
    } else if (key == "rac1.level.load_gameplay") {
        *original = reinterpret_cast<void*>(original_load);
    } else if (key == "rac1.pad.process_report") {
        CHECK(std::strcmp(signature, "void(u32,u32,i32)") == 0);
        *original = reinterpret_cast<void*>(+[](unsigned, unsigned, int) {});
    } else {
        return OPENRAC_HOOK_UNKNOWN;
    }
    hooks[key] = replacement;
    return OPENRAC_HOOK_OK;
}

int on_frame(openrac_mod*, openrac_frame_fn fn, void*) {
    frame = fn;
    return 0;
}

int on_pad(openrac_mod*, openrac_pad_fn, void*) {
    ++pads;
    return 0;
}

void rectangle(
    std::int32_t x0,
    std::int32_t y0,
    std::int32_t x1,
    std::int32_t y1,
    std::int64_t color,
    std::int32_t
) {
    CHECK(x0 <= x1 && y0 <= y1 && (color >> 24) <= 128);
    ++rectangles;
}

void* host_function(const char* name) {
    if (std::strcmp(name, "func_L00_002AE110") == 0 || std::strcmp(name, "FUN_L00_002ace70") == 0) {
        return reinterpret_cast<void*>(+[](unsigned, unsigned) -> unsigned { return 0; });
    }
    if (std::strcmp(name, "func_L00_001F2BE8") == 0 || std::strcmp(name, "FUN_L00_001f2868") == 0) {
        return reinterpret_cast<void*>(+[](float, unsigned, int, unsigned, unsigned) -> int {
            return 0;
        });
    }
    if (std::strcmp(name, "func_L00_0025AAC0") == 0 || std::strcmp(name, "FUN_L00_00259a88") == 0) {
        return reinterpret_cast<void*>(+[](unsigned, unsigned) {});
    }
    if (std::strcmp(name, "func_001F98C0") == 0 || std::strcmp(name, "FUN_001f96f8") == 0) {
        return reinterpret_cast<void*>(+[](int n) -> int { return n; });
    }
    if (std::strcmp(name, "func_L00_0025F4A8") == 0 || std::strcmp(name, "FUN_L00_0025e450") == 0
        || std::strcmp(name, "func_L00_002ADBB0") == 0
        || std::strcmp(name, "FUN_L00_002ac910") == 0) {
        return reinterpret_cast<void*>(+[]() {});
    }
    if (std::strcmp(name, "func_L00_00217718") == 0 || std::strcmp(name, "FUN_L00_00216f90") == 0) {
        return reinterpret_cast<void*>(+[](unsigned, unsigned, int, int) {});
    }
    if (std::strcmp(name, "func_L00_00264DB8") == 0 || std::strcmp(name, "FUN_L00_00263d40") == 0) {
        return reinterpret_cast<void*>(+[](int, int) {});
    }
    if (std::strcmp(name, "func_001FFAB8") == 0 || std::strcmp(name, "FUN_001ff288") == 0) {
        return reinterpret_cast<void*>(+[](int) -> unsigned { return 0x600000; });
    }
    if (std::strcmp(name, "func_L01_002AFD00") == 0 || std::strcmp(name, "FUN_L01_002aea70") == 0) {
        return reinterpret_cast<void*>(+[](unsigned, int) {});
    }
    if (std::strcmp(name, disc->drone_spawn) == 0) {
        return reinterpret_cast<void*>(spawn_drone);
    }
    if (std::strcmp(name, disc->gauge_draw) == 0) {
        return reinterpret_cast<void*>(original_gauge);
    }
    if (std::strcmp(name, disc->screen_position) == 0) {
        return reinterpret_cast<void*>(position);
    }
    if (std::strcmp(name, disc->transform) == 0) {
        return reinterpret_cast<void*>(transform);
    }
    if (std::strcmp(name, disc->quad) == 0) {
        return reinterpret_cast<void*>(quad);
    }
    if (std::strcmp(name, disc->texture) == 0) {
        return reinterpret_cast<void*>(texture);
    }
    if (std::strcmp(name, disc->rectangle) == 0) {
        return reinterpret_cast<void*>(rectangle);
    }
    if (std::strcmp(name, disc->weapon_draw) == 0) {
        return reinterpret_cast<void*>(original_hud);
    }
    if (std::strcmp(name, disc->relay) == 0) {
        return reinterpret_cast<void*>(relay_dispatch);
    }
    return nullptr;
}

void* host_function_at(std::uint32_t address) {
    if (address == 0x500000) {
        return reinterpret_cast<void*>(original_gauge);
    }
    if (loaded < 0 || loaded >= 19) {
        return nullptr;
    }
    const auto& row = (std::strcmp(disc->version, "pal") == 0 ? kHudPal : kHudNtsc)[loaded];
    if (address == row.weapon) {
        return reinterpret_cast<void*>(original_hud);
    }
    if (address == row.relay) {
        return reinterpret_cast<void*>(relay_dispatch);
    }
    return nullptr;
}

std::uint32_t push(std::uint32_t size, std::uint32_t* saved) {
    CHECK(size < 1024);
    *saved = guest_sp;
    guest_sp -= (size + 15) & ~15u;
    return guest_sp;
}

void pop(std::uint32_t saved) {
    guest_sp = saved;
}

openrac_api make_api(const Disc& selected) {
    disc = &selected;
    openrac_api api{};
    api.size = sizeof api;
    api.api_major = 1;
    api.api_minor = 3;
    api.game = "rac1";
    api.version = selected.version;
    api.platform = "test";
    api.openrac_version = "test";
    api.memory = ram.data();
    api.log = host_log;
    api.fail = host_fail;
    api.setting_bool = host_bool;
    api.setting_float = host_float;
    api.setting_string = host_string;
    api.hook = host_hook;
    api.on_frame = on_frame;
    api.on_pad = on_pad;
    api.function = host_function;
    api.function_at = host_function_at;
    api.relocate_data = exe;
    api.relocate_level_data = level_data;
    api.loaded_level = host_level;
    api.stack_push = push;
    api.stack_pop = pop;
    return api;
}

weapon_xp_stats_v1 stats(int item) {
    weapon_xp_stats_v1 out{};
    out.size = sizeof out;
    CHECK(service->get_stats(item, &out) == 0);
    return out;
}

void observer(void*, const weapon_xp_event_v1* event) {
    CHECK(event && event->size == sizeof(*event));
    if (event->kind == WEAPON_XP_CHANGED) {
        ++changed;
        CHECK(service->award_xp(event->item_id, 1) == WEAPON_XP_BUSY);
    }
    if (event->kind == WEAPON_XP_LEVEL_UP) {
        ++upgrades;
    }
    if (event->kind == WEAPON_XP_HIT) {
        ++hits;
    }
}

void external_combat(weapon_xp_query_fn query) {
    const auto baseline = kThresholds[1] - 10;
    std::uint32_t listener = 0;
    CHECK(service->subscribe(observer, nullptr, &listener) == WEAPON_XP_OK);
    const auto* base = query(WEAPON_XP_ABI_V2);
    CHECK(base && base->size == sizeof(weapon_xp_api_v2) && base->abi_version == 2);
    const auto* v2 = reinterpret_cast<const weapon_xp_api_v2*>(base);
    const auto hit =
        reinterpret_cast<std::uint32_t (*)(std::uint32_t, int, int)>(hooks["rac1.combat.get_hit"]);
    const auto death =
        reinterpret_cast<void (*)(std::uint32_t, int, int)>(hooks["rac1.moby.record_death"]);
    const auto load_level =
        reinterpret_cast<std::uint32_t (*)(int)>(hooks["rac1.level.load_gameplay"]);
    constexpr unsigned target = 0x380000, source = 0x400000;
    hit_address = ov(disc->hit_ring);
    auto fresh_hit = [&] {
        std::memset(ram.data() + hit_address, 0, 64);
        put<short>(target + 0xA6, 133);
        put<unsigned char>(target + 0xB1, 0xFF);
        put<short>(target + 0xB4, 10);
        put<short>(target + 0xB6, 10);
        put<unsigned char>(target + 0xA4, 0);
        put<short>(source + 0xA6, 163);  // Visibomb model: no native class305 override.
        put<unsigned>(hit_address + 0x20, source);
        put<unsigned>(hit_address + 0x24, 0x10000);
        put<float>(hit_address + 0x2C, 4);
        put<unsigned>(hit_address + 0x34, target);
    };
    weapon_xp_external_hit_v2 c{sizeof(c), 3, 991, 1, 77, target, hit_address, 133};
    weapon_xp_prepared_hit_v2 out{sizeof(out), -1, 0, 0};
    fresh_hit();
    CHECK(v2->prepare_local_hit(&c, &out) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(v2->begin_external_scope(c.session_id) == WEAPON_XP_OK);
    CHECK(v2->prepare_local_hit(nullptr, &out) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(v2->prepare_local_hit(&c, nullptr) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(service->award_xp(13, baseline) == WEAPON_XP_OK);
    const auto before_hits = hits;
    CHECK(v2->prepare_local_hit(&c, &out) == WEAPON_XP_OK);
    CHECK(out.item_id == 13 && out.damage == 4 && hits == before_hits + 1);
    CHECK(v2->prepare_local_hit(&c, &out) == WEAPON_XP_OK && hits == before_hits + 1);
    auto wrong = c;
    wrong.target_id++;
    CHECK(v2->prepare_local_hit(&wrong, &out) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(v2->confirm_external_death(&wrong) == WEAPON_XP_NOT_FOUND);
    death(target, 0, 0);  // Replica playback must not award before an owner receipt.
    CHECK(stats(13).xp == baseline);
    const auto before_upgrades = upgrades;
    CHECK(v2->confirm_external_death(&c) == WEAPON_XP_OK);
    CHECK(stats(13).xp == baseline + 20 && upgrades == before_upgrades + 1);  // xp_rate=2.
    CHECK(v2->confirm_external_death(&c) == WEAPON_XP_OK && stats(13).xp == baseline + 20);

    CHECK(v2->begin_external_scope(992) == WEAPON_XP_OK);
    CHECK(v2->confirm_external_death(&c) == WEAPON_XP_INVALID_ARGUMENT);
    c.session_id = 992;
    c.event_id = 2;
    frame(nullptr);  // Clears the previous frame's edit before a genuinely new native hit.
    fresh_hit();
    const auto remote_hits = hits;
    CHECK(v2->mark_replicated_hit(&c) == WEAPON_XP_OK);
    CHECK(hit(target, 0x10000, 0) == hit_address && get<float>(hit_address + 0x2C) == 4);
    CHECK(hits == remote_hits);
    death(target, 0, 0);
    CHECK(stats(13).xp == baseline + 20);
    // Same ring slot/target but changed native payload is not suppressed by a stale marker.
    put<float>(hit_address + 0x10, 1);
    CHECK(hit(target, 0x10000, 0) == hit_address);
    CHECK(
        std::abs(get<float>(hit_address + 0x2C) - (scale_damage ? 4.6f : 4.0f)) < .0001f
        && hits == remote_hits + 1
    );
    death(target, 0, 0);
    CHECK(stats(13).xp == baseline + 40);

    CHECK(v2->begin_external_scope(993) == WEAPON_XP_OK);
    c.session_id = 993;
    frame(nullptr);
    fresh_hit();
    CHECK(v2->mark_replicated_hit(&c) == WEAPON_XP_OK);
    for (int i = 0; i < 9; ++i) {
        frame(nullptr);
    }
    CHECK(hit(target, 0x10000, 0) == 0 && get<unsigned char>(target + 0xA4) == 0xFF);
    CHECK(stats(13).xp == baseline + 40);

    CHECK(v2->begin_external_scope(994) == WEAPON_XP_OK);
    c.session_id = 994;
    c.event_id = 3;
    fresh_hit();
    CHECK(v2->prepare_local_hit(&c, &out) == WEAPON_XP_OK);
    CHECK(load_level(0) == 0x123456);  // Confirm by stable identity after local level unload.
    put<short>(
        target + 0xA6, 6
    );  // Local slot was reused: receipt must not inspect this new actor.
    CHECK(v2->confirm_external_death(&c) == WEAPON_XP_OK && stats(13).xp == baseline + 60);
    CHECK(v2->begin_external_scope(0) == WEAPON_XP_OK);
    CHECK(v2->confirm_external_death(&c) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(service->unsubscribe(listener) == WEAPON_XP_OK);
    std::puts(
        "XP v2: prepared hits, replicated provenance, native ring reuse, receipts, xp_rate and "
        "level lifecycle: ok"
    );
}

void extension_contract(
    weapon_xp_query_fn query,
    const openrac_api& api,
    openrac_mod* handle,
    openrac_mod_load_fn load,
    void (*unload)()
) {
    test_profile = "extension";
    scale_damage = false;
    scale_capacity = false;
    test_xp_rate = 1;
    CHECK(load(&api, handle) == 0);
    const auto* raw = query(3);
    CHECK(raw && raw->abi_version == 3 && raw->size == sizeof(weapon_xp_api_v3));
    const auto* v3 = reinterpret_cast<const weapon_xp_api_v3*>(raw);
    CHECK(query(1)->size == sizeof(weapon_xp_api_v1) && query(2)->size == sizeof(weapon_xp_api_v2));
    weapon_xp_configuration_v3 cfg{};
    cfg.size = sizeof(cfg);
    CHECK(v3->get_configuration(&cfg) == 0 && cfg.revision == 1 && cfg.level_count == 10);
    CHECK(!(
        cfg.flags & (WEAPON_XP_SCALE_DAMAGE | WEAPON_XP_SCALE_CAPACITY | WEAPON_XP_FLIP_MORPH_MODEL)
    ));

    struct ObserveConfiguration {
        const weapon_xp_api_v3* api;
        unsigned calls = 0;
    } observed{v3};

    std::uint32_t configuration_token = 0;
    CHECK(
        service->subscribe(
            +[](void* user, const weapon_xp_event_v1* event) {
                if (event->kind != WEAPON_XP_CONFIGURATION_CHANGED) {
                    return;
                }
                auto& context = *static_cast<ObserveConfiguration*>(user);
                weapon_xp_configuration_v3 snapshot{};
                snapshot.size = sizeof(snapshot);
                CHECK(context.api->get_configuration(&snapshot) == 0);
                CHECK(
                    context.api->set_configuration(&snapshot, snapshot.revision) == WEAPON_XP_BUSY
                );
                CHECK(context.api->flush() == WEAPON_XP_BUSY);
                ++context.calls;
            },
            &observed,
            &configuration_token
        )
        == 0
    );
    cfg.flags &= ~WEAPON_XP_LEVEL_UP_ANIMATION;
    cfg.flags |= WEAPON_XP_FLIP_MORPH_MODEL;
    CHECK(v3->set_configuration(&cfg, cfg.revision) == 0);
    CHECK(v3->set_configuration(&cfg, cfg.revision) == WEAPON_XP_STALE_REVISION);
    CHECK(v3->get_configuration(&cfg) == 0 && cfg.revision == 2);
    CHECK(cfg.flags & WEAPON_XP_FLIP_MORPH_MODEL);
    weapon_xp_rules_v3 rules{};
    rules.size = sizeof(rules);
    CHECK(v3->get_rules(13, &rules) == 0);
    for (unsigned i = 0; i < 10; ++i) {
        rules.thresholds[i] = i * 100;
    }
    rules.thresholds[9] = 100000;
    rules.xp_multiplier = 3;
    CHECK(v3->set_rules(&rules, 1, cfg.revision) == 0);
    CHECK(
        service->award_xp(13, 150) == 0 && stats(13).level == 2 && stats(13).next_level_xp == 200
    );
    CHECK(v3->get_configuration(&cfg) == 0 && cfg.revision == 3);
    weapon_xp_rules_v3 batch[]{rules, rules};
    batch[1].item_id = 15;
    batch[1].damage[9] = NAN;
    CHECK(v3->set_rules(batch, 2, cfg.revision) == WEAPON_XP_INVALID_ARGUMENT);
    weapon_xp_configuration_v3 after{};
    after.size = sizeof(after);
    CHECK(v3->get_configuration(&after) == 0 && after.revision == cfg.revision);
    char error[12]{};
    CHECK(v3->last_error(error, sizeof(error)) > 1 && error[sizeof(error) - 1] == 0);
    rules.xp_multiplier = 0;
    CHECK(v3->set_rules(&rules, 1, cfg.revision) == 0);
    CHECK(service->award_xp(13, 100) == WEAPON_XP_DISABLED && stats(13).xp == 150);
    CHECK(v3->get_configuration(&cfg) == 0);
    rules.xp_multiplier = 3;
    CHECK(v3->set_rules(&rules, 1, cfg.revision) == 0);
    const auto* v2 = &v3->v2;
    CHECK(v3->get_configuration(&cfg) == 0);
    CHECK(v2->begin_external_scope(42) == 0);
    CHECK(v3->set_rules(&rules, 1, cfg.revision) == WEAPON_XP_BUSY);
    CHECK(v2->begin_external_scope(0) == 0);
    // Capacity must restore the last value actually written, even after a new XP level.
    base_ammo();
    put<int>(exe(disc->item_in_hand), 13);
    frame(nullptr);
    cfg.flags |= WEAPON_XP_SCALE_CAPACITY;
    CHECK(v3->set_configuration(&cfg, cfg.revision) == 0);
    frame(nullptr);
    CHECK(get<unsigned short>(ov(disc->item_table) + 13 * 24 + 14) == 45);
    CHECK(service->award_xp(13, 100) == 0 && stats(13).level == 3);
    CHECK(v3->get_configuration(&cfg) == 0);
    cfg.flags &= ~WEAPON_XP_SCALE_CAPACITY;
    CHECK(v3->set_configuration(&cfg, cfg.revision) == 0);
    CHECK(get<unsigned short>(ov(disc->item_table) + 13 * 24 + 14) == 40);
    const auto file = profile_path(disc->version, test_profile).parent_path() / "rules.ini";
    std::ofstream(file) << "format=1\n[weapon.13]\nthresholds=0,100,200,300,400,500,600,700,800,"
                           "100000\nxp_multiplier=3\n";
    CHECK(v3->get_configuration(&cfg) == 0 && v3->reload_rules(cfg.revision) == 0);
    CHECK(service->award_xp(13, 100000) == 0 && stats(13).xp == 100000);
    CHECK(v3->flush() == 0);
    CHECK(v3->get_configuration(&cfg) == 0);
    std::ofstream(file) << "format=77";
    CHECK(v3->reload_rules(cfg.revision) == WEAPON_XP_IO_ERROR && stats(13).xp == 100000);
    CHECK(v3->get_configuration(&after) == 0 && after.revision == cfg.revision);
    std::ofstream(file) << rules_template();
    CHECK(v3->get_configuration(&cfg) == 0);
    cfg.flags &= ~WEAPON_XP_FLIP_MORPH_MODEL;
    CHECK(v3->set_configuration(&cfg, cfg.revision) == 0);
    CHECK(v3->get_configuration(&cfg) == 0 && !(cfg.flags & WEAPON_XP_FLIP_MORPH_MODEL));
    CHECK(observed.calls >= 5 && service->unsubscribe(configuration_token) == 0);
    unload();
    CHECK(v3->get_configuration(&cfg) == WEAPON_XP_NOT_READY);
    CHECK(load(&api, handle) == 0 && stats(13).xp == 100000 && stats(13).level == 10);
    unload();
    std::puts(
        "ABI v3: atomic patches, stale revisions, co-op guards, reload rollback, bonuses and "
        "persistence: ok"
    );
}

int main(int argc, char** argv) {
    CHECK(argc == 2 || argc == 3);
    std::ifstream contracts(WEAPON_XP_CONTRACT_FIXTURE);
    CHECK(contracts.is_open());
    std::string line;
    while (std::getline(contracts, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string version, name, signature;
        CHECK(bool(row >> version >> name >> signature));
        installed_contracts[{version, name}] = signature;
    }
    CHECK(installed_contracts.at({"ntsc", "Moby_Class482_Update"}) == "void()");
    CHECK(installed_contracts.at({"pal", "Moby_Class482_Update"}) == "void(u32)");
    const auto root =
        std::filesystem::current_path()
        / ("weapon-xp-host-test-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
#if defined(_WIN32)
    _wputenv_s(L"LOCALAPPDATA", root.c_str());
#else
    setenv("XDG_DATA_HOME", root.c_str(), 1);
    setenv("HOME", root.c_str(), 1);
#endif
    void* lib = library(argv[1]);
    CHECK(lib);
    const auto load = reinterpret_cast<openrac_mod_load_fn>(symbol(lib, "openrac_mod_load"));
    const auto unload = reinterpret_cast<void (*)()>(symbol(lib, "openrac_mod_unload"));
    const auto query = reinterpret_cast<weapon_xp_query_fn>(symbol(lib, WEAPON_XP_QUERY_SYMBOL));
    const auto mod_query = reinterpret_cast<openrac_mod_query_fn>(symbol(lib, "openrac_mod_query"));
    CHECK(load && unload && query && mod_query);
    openrac_mod_info info{};
    info.size = sizeof info;
    mod_query(&info);
    CHECK(info.api_minor == 1 && std::strcmp(info.id, "weapon-xp") == 0);
    CHECK(std::strcmp(info.version, WEAPON_XP_EXPECTED_VERSION) == 0);
    CHECK(query(999) == nullptr);
    service = query(1);
    CHECK(service && service->size == sizeof(*service) && service->weapon_count() == 12);
    CHECK(service->award_xp(0x0A, 1) == WEAPON_XP_NOT_READY);
    auto* handle = reinterpret_cast<openrac_mod*>(&info);
    openrac_mod_load_fn observer_load = nullptr;
    void (*observer_unload)() = nullptr;
    if (argc == 3) {
        void* observer = library(argv[2]);
        CHECK(observer);
        observer_load = reinterpret_cast<openrac_mod_load_fn>(symbol(observer, "openrac_mod_load"));
        observer_unload = reinterpret_cast<void (*)()>(symbol(observer, "openrac_mod_unload"));
        CHECK(observer_load && observer_unload);
    }
    for (int options = 0; options < 4; ++options) {
        scale_damage = (options & 1) != 0;
        scale_capacity = (options & 2) != 0;
        test_profile = "integration-" + std::to_string(options);
        for (const auto& selected : kDiscs) {
            std::fill(ram.begin(), ram.end(), 0);
            hooks.clear();
            failure.clear();
            frame = nullptr;
            loaded = 0;
            auto api = make_api(selected);
            for (int c = 32; c < 128; ++c) {
                const auto at = exe(disc->small_font_metrics) + 4 * c;
                put<std::uint8_t>(at, static_cast<std::uint8_t>(c));
                put<std::uint8_t>(at + 3, 8);
            }
            missing_relay = true;
            CHECK(load(&api, handle) != 0 && failure.find("bolt.update") != std::string::npos);
            missing_relay = false;
            CHECK(load(&api, handle) == 0 && frame && pads == 0);
            if (observer_load) {
                CHECK(observer_load(&api, handle) == 0);
            }
            std::uint32_t token = 0;
            CHECK(service->subscribe(observer, nullptr, &token) == 0);
            CHECK(service->get_stats(0x0C, nullptr) == WEAPON_XP_INVALID_ARGUMENT);
            base_ammo();
            put<std::int32_t>(exe(disc->item_in_hand), 0x0F);  // Blaster equipped when bomb lands.
            put<float>(exe(disc->tick_seconds), 1.0f / 60.0f);
            frame(nullptr);
            auto hit = reinterpret_cast<std::uint32_t (*)(std::uint32_t, int, int)>(
                hooks["rac1.combat.get_hit"]
            );
            auto death = reinterpret_cast<void (*)(std::uint32_t, int, int)>(
                hooks["rac1.moby.record_death"]
            );
            auto load_gameplay =
                reinterpret_cast<std::uint32_t (*)(int)>(hooks["rac1.level.load_gameplay"]);
            const std::uint32_t source = 0x400000;
            const std::uint32_t owner = 0x401000, vars = 0x402000;
            put<std::int16_t>(source + 0xA6, 121);  // Actual thrown body seen in the native game.
            put<std::uint32_t>(source + 0x78, vars);
            put<std::uint32_t>(vars + 0x50, owner);
            put<std::int16_t>(
                owner + 0xA6, 192
            );  // Originating glove, independent of equipped item.
            hit_address = ov(disc->hit_ring);
            for (int i = 0; i < 16; ++i) {
                const std::uint32_t target = 0x300000 + i * 0x100;
                put<std::int16_t>(target + 0xA6, 133);
                put<unsigned char>(target + 0xB1, 0xFF);
                put<short>(target + 0xB4, 25);
                put<std::uint32_t>(hit_address + 0x20, source);
                put<std::uint32_t>(hit_address + 0x34, target);
                put<float>(hit_address + 0x2C, 4);
                CHECK(hit(target, 0x10000, 0) == hit_address);
                death(target, 0, 0);
                death(target, 0, 0);
            }
            CHECK(stats(0x0A).xp == kThresholds[1] && stats(0x0F).xp == 0);
            CHECK(stats(0x0A).level == 2);
            CHECK(stats(0x0A).damage_multiplier == (scale_damage ? 1.15f : 1.0f));
            CHECK(stats(0x0A).ammo_multiplier == (scale_capacity ? 1.125f : 1.0f));
            CHECK(service->award_xp(0x0F, kThresholds[1]) == 0);
            const std::uint32_t target = 0x310000;
            put<std::int16_t>(source + 0xA6, 305);  // Actual Blaster shot, not its held model.
            put<std::uint32_t>(vars + 0x20, owner);
            put<std::int16_t>(
                owner + 0xA6, 0
            );  // The Blaster shot's owner is the hero, not the gun model.
            put<std::uint32_t>(exe(disc->hero) + 0x2080, owner);
            put<std::int16_t>(target + 0xA6, 133);
            put<std::uint32_t>(hit_address + 0x34, target);
            put<float>(hit_address + 0x2C, 4);
            hit(target, 0x10000, 0);
            CHECK(
                std::abs(get<float>(hit_address + 0x2C) - (scale_damage ? 4.6f : 4.0f)) < 0.0001f
            );
            hit(target, 0x10000, 1);
            CHECK(
                std::abs(get<float>(hit_address + 0x2C) - (scale_damage ? 4.6f : 4.0f)) < 0.0001f
            );
            frame(nullptr);
            CHECK(get<float>(hit_address + 0x2C) == 4);
            CHECK(
                get<std::uint16_t>(ov(disc->item_table) + 0x0F * 24 + 14)
                == (scale_capacity ? 45 : 40)
            );
            CHECK(load_gameplay(0) == 0x123456);
            frame(nullptr);
            CHECK(
                get<std::uint16_t>(ov(disc->item_table) + 0x0F * 24 + 14)
                == (scale_capacity ? 45 : 40)
            );  // No compounding on reload.
            // No XP from an object outside the combat whitelist.
            put<std::int16_t>(target + 0xA6, 6);
            hit(target, 0x10000, 0);
            death(target, 0, 0);
            CHECK(stats(0x0F).xp == kThresholds[1]);
            // An enemy-owned generic bomb must never get credit for the currently held weapon.
            put<std::int16_t>(source + 0xA6, 121);
            put<std::int16_t>(owner + 0xA6, 577);
            put<std::int16_t>(target + 0xA6, 133);
            put<float>(hit_address + 0x2C, 4);
            hit(target, 0x10000, 0);
            death(target, 0, 0);
            CHECK(stats(0x0A).xp == kThresholds[1] && stats(0x0F).xp == kThresholds[1]);
            CHECK(hooks.count("rac1.weapon.take_ammo") == 0);
            CHECK(hooks.count("rac1.moby.hero_bomb.update") == 0);
            CHECK(hooks.count("rac1.moby.moby305.update") == 0);
            CHECK(hooks.size() == 6);  // No projectile or continuous weapon interception.
            // Original tile position/alpha are supplied by the real HUD element, not fixed screen coordinates.
            const std::uint32_t element = exe(disc->hud_bank);
            const auto code = (std::strcmp(disc->version, "pal") == 0 ? kHudPal : kHudNtsc)[loaded];
            put<int>(element + 0x20, 60000 + 0x0F);
            put<std::uint32_t>(element + 0x18, code.weapon);
            put<int>(element + 0x50, 40);
            put<int>(element + 8, 40);
            put<std::uint8_t>(element + 0x70, 8);
            put<int>(ov(disc->hud_parameters), 8);
            put<int>(ov(disc->hud_parameters + 8), 64);
            put<int>(ov(disc->hud_parameters + 32), 16);
            put<std::uint32_t>(ov(disc->hud_parameters + 16), 0x80E0E0E0);
            glyph_text.clear();
            rectangles = 0;
            frame(nullptr);
            CHECK(get<std::uint32_t>(element + 0x18) == code.relay);
            CHECK(get<int>(element + 8) == (scale_capacity ? 45 : 40));
            relay_dispatch(element);
            CHECK(hud_calls > 0 && rectangles >= 4 && glyph_text == "LV2LV2");
            put<std::uint8_t>(element + 0x70, 0);
            glyph_text.clear();
            relay_dispatch(element);
            CHECK(glyph_text.empty());
            relay_dispatch(element + 0x90);
            CHECK(counter_calls > 0);
            // The Suck Cannon uses its own existing vertical gauge and SDK stack positioning.
            const auto gauge = element + 4 * 0x90;
            put<int>(exe(disc->item_in_hand), 9);
            put<int>(gauge + 0x20, 0x753F);
            put<unsigned>(gauge + 0x18, 0x500000);
            put<int>(gauge + 0x50, 400);
            put<int>(gauge + 0x54, 60);
            frame(nullptr);
            CHECK(get<unsigned>(element + 0x18) == code.weapon);
            CHECK(get<unsigned>(gauge + 0x18) == code.relay);
            glyph_text.clear();
            put<unsigned>(
                ov(std::strcmp(disc->version, "pal") == 0 ? 0x161240 : 0x1611C0), 0x610000
            );
            relay_dispatch(gauge);
            CHECK(glyph_text == "LV1LV1" && guest_sp == 0xE0100000u);
            CHECK(service->unsubscribe(token) == 0);
            unload();
            CHECK(get<std::uint16_t>(ov(disc->item_table) + 0x0F * 24 + 14) == 40);
            CHECK(get<std::uint32_t>(element + 0x18) == code.weapon);
            CHECK(get<unsigned>(gauge + 0x18) == 0x500000);
            if (observer_unload) {
                observer_unload();
            }
            CHECK(service->award_xp(0x0F, 1) == WEAPON_XP_NOT_READY);
            hud_enabled = false;
            test_xp_rate = 2;
            CHECK(
                load(&api, handle) == 0 && stats(0x0F).xp == kThresholds[1]
                && stats(0x0A).xp == kThresholds[1]
            );
            put<int>(exe(disc->item_in_hand), 15);
            frame(nullptr);
            CHECK(
                get<std::uint16_t>(ov(disc->item_table) + 0x0F * 24 + 14)
                == (scale_capacity ? 45 : 40)
            );
            external_combat(query);
            CHECK(service->award_xp(15, UINT32_MAX) == 0);
            frame(nullptr);
            CHECK(stats(15).level == 10 && stats(15).xp == 46000);
            CHECK(stats(15).next_level_xp == 46000);
            CHECK(stats(15).damage_multiplier == (scale_damage ? 3.0f : 1.0f));
            CHECK(stats(15).ammo_multiplier == (scale_capacity ? 2.125f : 1.0f));
            CHECK(
                get<std::uint16_t>(ov(disc->item_table) + 15 * 24 + 14)
                == (scale_capacity ? 85 : 40)
            );
            unload();
            CHECK(get<std::uint16_t>(ov(disc->item_table) + 15 * 24 + 14) == 40);
            CHECK(load(&api, handle) == 0 && stats(15).level == 10);
            unload();
            test_xp_rate = 1;
            hud_enabled = true;
        }
    }
    for (const auto& selected : kDiscs) {
        std::fill(ram.begin(), ram.end(), 0);
        hooks.clear();
        loaded = 0;
        auto api = make_api(selected);
        extension_contract(query, api, handle, load, unload);
    }
    CHECK(changed >= 32 && upgrades >= 4 && hits >= 30);
    CHECK(!observer_load || observed_levels >= 4);
    CHECK(root.parent_path() == std::filesystem::current_path());
    std::filesystem::remove_all(root);
    std::puts(
        "DLL, PAL/NTSC relocation, delayed attribution, persistence, native HUD and public API: ok"
    );
}

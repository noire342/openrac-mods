#include <cstdlib>
#include <cstring>
#include <string>

#include "../include/skill_points_api.h"
#include "../src/native_map.hpp"
#include "weapon_xp_api.h"

#include <openrac/openrac_mod.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace {
const openrac_api* api;
openrac_mod* mod;
unsigned ticks = 0;
bool opened = false;
bool selected_goodies_entry = false;
bool dumped_pause = false;
bool dumped_goodies = false, proved_points = false;
unsigned pause_visits = 0;
std::uint32_t previous_page = 0;
std::uint32_t skill_page = 0, input = 0;
unsigned xp_level_events = 0;
const weapon_xp_api_v1* xp = nullptr;
std::uint32_t xp_token = 0;

void on_xp(void*, const weapon_xp_event_v1* e) {
    if (e->kind == WEAPON_XP_LEVEL_LOADED) {
        ++xp_level_events;
    }
}

std::uint32_t read(std::uint32_t a) {
    std::uint32_t v = 0;
    std::memcpy(&v, api->memory + a, 4);
    return v;
}

void write(std::uint32_t a, std::uint32_t v) {
    std::memcpy(api->memory + a, &v, 4);
}

void frame(void*) {
    const int level = api->loaded_level();
    if (level < 0 || level >= 19) {
        return;
    }
    ++ticks;
#if defined(_WIN32)
    if (ticks == 1) {
        if (auto producer = GetModuleHandleW(L"skill_points_ra.dll")) {
            auto query = reinterpret_cast<sp_query_fn>(
                GetProcAddress(producer, "openrac_skill_points_query")
            );
            const auto* service = query ? query(1) : nullptr;
            if (service && service->size >= sizeof(sp_api)) {
                sp_totals totals{};
                totals.size = sizeof(totals);
                if (service->active_totals(&totals) == SP_OK) {
                    const auto line = "QA achievement selection kerwan="
                                      + std::to_string(service->is_enabled(264462))
                                      + " active=" + std::to_string(totals.count)
                                      + " points=" + std::to_string(totals.points);
                    api->log(mod, OPENRAC_LOG_INFO, line.c_str());
                }
            }
        }
        if (auto module = GetModuleHandleW(L"weapon_xp.dll")) {
            if (auto query = reinterpret_cast<weapon_xp_query_fn>(
                    GetProcAddress(module, WEAPON_XP_QUERY_SYMBOL)
                )) {
                if ((xp = query(1))) {
                    const auto result = xp->subscribe(on_xp, nullptr, &xp_token);
                    const auto line = "QA XP observer subscribe=" + std::to_string(result)
                                      + " token=" + std::to_string(xp_token);
                    api->log(
                        mod,
                        result == 0 && xp_token ? OPENRAC_LOG_INFO : OPENRAC_LOG_ERROR,
                        line.c_str()
                    );
                }
            }
        }
    }
#endif
    if (ticks == 150 && std::getenv("SP_QA_GRANT")) {
        using Give = void (*)(int, int);
        if (auto give = reinterpret_cast<Give>(api->function("FUN_L00_00260860"))) {
            give(0x10, 0);
        }
        api->log(
            mod,
            OPENRAC_LOG_INFO,
            "QA controlled native Item_Give(Pyrocitor), isolated profile only"
        );
    }
    if (!skill_page) {
        for (std::uint32_t a = 0x160000; a < 0x260000; a += 4) {
            if (read(a) == skillpoints::kPointInput[level]
                && (read(a + 4) == skillpoints::kPointList[level]
                    || read(a + 4) == skillpoints::kRelay[level])
                && (read(a + 0x30) & 0x200u)) {
                input = a;
            }
        }
        if (input) {
            for (std::uint32_t a = 0x160000; a < 0x260000; a += 4) {
                if (read(a + 0x40) == input && read(a + 0x3C) == 0x24 && read(a + 0x48) == input) {
                    skill_page = a;
                }
            }
        }
    }
    const auto system = api->relocate_data(0x001D5BF0);
    const auto mode = api->relocate_data(0x0015F604);
    const auto current = read(system + 4);
    if (std::getenv("SP_QA_PAUSE") && current && current != previous_page) {
        previous_page = current;
        if (read(current + 0x3C) == 2) {
            ++pause_visits;
            const auto line =
                "QA native pause visit=" + std::to_string(pause_visits)
                + " seventh_visible=" + std::to_string(read(system + 0xD8))
                + " story_completed=" + std::to_string(read(api->relocate_data(0x0015EEA0)));
            api->log(mod, OPENRAC_LOG_INFO, line.c_str());
        }
    }
    if (!current) {
        previous_page = 0;
    }
    if (std::getenv("SP_QA_PAUSE") && skill_page && read(system + 4) == read(skill_page + 0x38)
        && !dumped_goodies) {
        dumped_goodies = true;
        api->log(
            mod,
            OPENRAC_LOG_INFO,
            "QA normal controller route reached Goodies; no page or selection writes"
        );
        const auto f = read(read(system + 4) + 0x40), entries = read(f + 0x34);
        for (unsigned i = 0; i < 32 && (read(entries + 12 * i) & 0xffffu); ++i) {
            const auto line = "QA goodies item=" + std::to_string(i)
                              + " id=" + std::to_string(read(entries + 12 * i) & 0xffffu)
                              + " target=" + std::to_string(read(entries + 12 * i + 4));
            api->log(mod, OPENRAC_LOG_INFO, line.c_str());
        }
    }
    if (std::getenv("SP_QA_PAUSE") && skill_page && read(system + 4) == skill_page
        && !proved_points) {
        proved_points = true;
        api->log(
            mod,
            OPENRAC_LOG_INFO,
            "QA normal controller route reached skill-point list; no page or selection writes"
        );
    }
    if (!dumped_pause && ticks > 240 && read(mode) == 3 && read(system + 4)) {
        dumped_pause = true;
        const auto page = read(system + 4), focus = read(page + 0x40);
        auto line =
            "QA pause page=" + std::to_string(page) + " state=" + std::to_string(read(page + 0x3C))
            + " focus=" + std::to_string(focus) + " enter=" + std::to_string(read(focus + 8))
            + " flags=" + std::to_string(read(focus + 0x30));
        api->log(mod, OPENRAC_LOG_INFO, line.c_str());
        for (unsigned j = 0; j < 14; ++j) {
            const auto s = read(page + 0x44 + 4 * j);
            if (!s) {
                continue;
            }
            line = "QA panel=" + std::to_string(j) + " screen=" + std::to_string(s)
                   + " update=" + std::to_string(read(s)) + " draw=" + std::to_string(read(s + 4))
                   + " enter=" + std::to_string(read(s + 8))
                   + " x=" + std::to_string(read(s + 0x18)) + " y=" + std::to_string(read(s + 0x1C))
                   + " w=" + std::to_string(read(s + 0x20)) + " h=" + std::to_string(read(s + 0x24))
                   + " flags=" + std::to_string(read(s + 0x30)) + " items="
                   + std::to_string(read(s + 0x34)) + " up=" + std::to_string(read(s + 0x38))
                   + " down=" + std::to_string(read(s + 0x3C));
            api->log(mod, OPENRAC_LOG_INFO, line.c_str());
            line = "QA panel item=" + std::to_string(read(read(s + 0x34)))
                   + " target=" + std::to_string(read(read(s + 0x34) + 4))
                   + " anim=" + std::to_string(read(page + 4 * j));
            api->log(mod, OPENRAC_LOG_INFO, line.c_str());
        }
        const auto items = read(focus + 0x34);
        using Help = std::uint32_t (*)(int);
        auto help = reinterpret_cast<Help>(api->function("FUN_001fdd10"));
        for (unsigned i = 0; i < 16 && (read(items + 12 * i) & 0xffffu); ++i) {
            const auto id = read(items + 12 * i) & 0xffffu, ptr = help(id);
            line = "QA pause item=" + std::to_string(i) + " id=" + std::to_string(id)
                   + " action=" + std::to_string(read(items + 12 * i) >> 16)
                   + " param=" + std::to_string(read(items + 12 * i + 4))
                   + " text=" + reinterpret_cast<const char*>(api->memory + ptr);
            api->log(mod, OPENRAC_LOG_INFO, line.c_str());
        }
    }
    if (!std::getenv("SP_QA_PAUSE") && !opened && skill_page && ticks > 200 && read(mode) == 3
        && read(system + 4)) {
        write(system + 8, std::getenv("SP_QA_GOODIES") ? read(skill_page + 0x38) : skill_page);
        opened = true;
        api->log(
            mod, OPENRAC_LOG_INFO, "QA: requested existing skill points page through menu.next"
        );
    }
    if (opened && std::getenv("SP_QA_GOODIES") && !selected_goodies_entry
        && read(system + 4) == read(skill_page + 0x38)) {
        const auto goodies = read(skill_page + 0x38), focus = read(goodies + 0x40),
                   items = read(focus + 0x34);
        for (unsigned i = 0; i < 32 && (read(items + 12 * i) & 0xffffu); ++i) {
            if (read(items + 12 * i + 4) == skill_page) {
                write(focus + 0x40, i);
                selected_goodies_entry = true;
                const auto line =
                    "QA Goodies entry action=" + std::to_string(read(items + 12 * i) >> 16);
                api->log(mod, OPENRAC_LOG_INFO, line.c_str());
                break;
            }
        }
    }
    if (ticks % 120 == 0) {
        const auto stream = skill_page ? read(skill_page + 0x4C) : 0;
        const auto line =
            "QA tick=" + std::to_string(ticks) + " level=" + std::to_string(level) + " ui_ready="
            + std::to_string(skill_page && read(input + 4) == skillpoints::kRelay[level] ? 1 : 0)
            + " mode=" + std::to_string(read(mode)) + " current=" + std::to_string(read(system + 4))
            + " selection=" + std::to_string(read(input + 0x40)) + " w="
            + std::to_string(read(stream + 0x20)) + " h=" + std::to_string(read(stream + 0x24))
            + " raMode=" + std::to_string(read(0x0015F5C4))
            + " xp_level_events=" + std::to_string(xp_level_events);
        api->log(mod, OPENRAC_LOG_INFO, line.c_str());
    }
}
}  // namespace

extern "C" {
OPENRAC_MOD_DEFINE("skill-points-qa", "0.0.1")
}

extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* a, openrac_mod* m) {
    api = a;
    mod = m;
    return api->on_frame(mod, frame, nullptr);
}

extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    if (xp && xp_token) {
        xp->unsubscribe(xp_token);
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

#include "native_ui.hpp"
#include "skill_points_api.h"
#include "store.hpp"

namespace skillpoints {
class Runtime;
Runtime* active = nullptr;
using Loader = std::uint32_t (*)(std::int32_t);
Loader original_loader = nullptr;

class GameMemory final : public IMemory {
public:
    explicit GameMemory(const openrac_api* api) : m_api(api) {}

    bool read(std::uint32_t address, void* output, std::uint32_t bytes) const override {
        // RA operands already name the active NTSC-U console address, including
        // level-specific RAM. Do not relocate a live pointer or a RA operand a second time.
        if (address >= 0x02000000u || bytes > 0x02000000u - address) {
            return false;
        }
        std::memcpy(output, m_api->memory + address, bytes);
        return true;
    }

private:
    const openrac_api* m_api;
};

class Runtime {
public:
    const openrac_api* api;
    openrac_mod* mod;
    GameMemory memory;
    Ledger ledger;
    AchievementPolicy policy;
    Engine engine;
    NativeUi ui;
    std::unique_ptr<FileStore> store;

    struct Listener {
        std::uint32_t token = 0;
        sp_listener fn = nullptr;
        void* user = nullptr;
    };

    std::array<Listener, 16> listeners{};
    std::uint32_t next_token = 1;
    bool dispatching = false, dirty = false, stopping = false, ui_failed = false,
         read_error = false;
    unsigned frame = 0, notice_after = 0, pending_notices = 0;
    int last_level = -99;

    explicit Runtime(const openrac_api* a, openrac_mod* m)
        : api(a),
          mod(m),
          memory(a),
          engine(memory, ledger, policy),
          ui(a, ledger, engine) {}

    void log(int level, const std::string& text) const { api->log(mod, level, text.c_str()); }

    void emit(std::uint32_t type, std::uint32_t id) {
        const sp_event event{sizeof(sp_event), type, id};
        const auto copy = listeners;
        dispatching = true;
        for (const auto& l : copy) {
            if (l.fn) {
                bool subscribed = false;
                for (const auto& current : listeners) {
                    if (current.token == l.token) {
                        subscribed = true;
                    }
                }
                if (subscribed) {
                    try {
                        l.fn(&event, l.user);
                    } catch (...) {
                        for (auto& current : listeners) {
                            if (current.token == l.token) {
                                current = {};
                            }
                        }
                        log(OPENRAC_LOG_ERROR, "Achievement listener threw; disconnected");
                    }
                }
            }
        }
        dispatching = false;
    }

    void flush() {
        if (!dirty || !store) {
            return;
        }
        try {
            store->save(ledger);
            dirty = false;
        } catch (const std::exception& e) {
            log(OPENRAC_LOG_ERROR, e.what());
        }
    }

    void tick() {
        ++frame;
        policy.set_enabled(kKerwanTimeTrial, api->setting_bool(mod, "kerwan_time_trial") != 0);
        const int level = api->loaded_level();
        if (level < 0 || level >= 19) {
            if (last_level >= 0 && last_level < 19) {
                engine.reset_session();
            }
            last_level = level;
            ui.restore();
            flush();
            return;
        }
        if (level != last_level) {
            last_level = level;
            ui_failed = false;
        }
        if (!ui_failed) {
            try {
                ui.refresh();
            } catch (const std::exception& e) {
                ui_failed = true;
                log(OPENRAC_LOG_ERROR, e.what());
            }
        }
        const auto now =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                           std::chrono::system_clock::now().time_since_epoch()
            )
                                           .count());
        engine.tick(now, [&](std::uint32_t id) {
            dirty = true;
            const auto* d = find_definition(id);
            log(OPENRAC_LOG_INFO, "Punto stile ottenuto: " + std::to_string(id) + " - " + d->title);
            if (d->native_index < 0) {
                ++pending_notices;
            }
            emit(SP_EVENT_UNLOCKED, id);
        });
        if (!engine.healthy() && !read_error) {
            read_error = true;
            log(
                OPENRAC_LOG_ERROR,
                "An unlock used an unreadable operand and was rejected; other achievements continue"
            );
        }
        // Keep the thirty original achievements visible for an existing memory-card save.
        // This imports original skill points only, never historical RA account unlocks.
        const auto flags = api->relocate_data(0x0013D408);
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            const auto& d = kCatalogue[i];
            std::uint8_t value = 0;
            if (d.native_index >= 0
                && memory.read(flags + static_cast<unsigned>(d.native_index), &value, 1)
                && value == 1) {
                if (ledger.unlock(d.id, 0)) {
                    dirty = true;
                }
            }
        }
        if (pending_notices && frame >= notice_after && api->setting_bool(mod, "notifications")) {
            std::uint32_t mode = 0;
            if (memory.read(0x0015F5C4, &mode, 4) && mode == 0) {
                ui.notify();
                pending_notices = 0;
                notice_after = frame + 180;
            }
        }
        if (dirty) {
            flush();
        }
    }

    ~Runtime() {
        stopping = true;
        emit(SP_EVENT_STOPPING, 0);
        ui.restore();
        flush();
        if (active == this) {
            active = nullptr;
        }
    }
};

static void frame_callback(void*) noexcept {
    if (active) {
        try {
            active->tick();
        } catch (const std::exception& e) {
            active->log(OPENRAC_LOG_ERROR, e.what());
        }
    }
}

static std::uint32_t relay(std::int32_t value) noexcept {
    if (active) {
        const auto address = static_cast<std::uint32_t>(value);
        if (active->ui.owns(address)) {
            if (!active->ui.handles(address)) {
                return 1;  // an inactive owned panel never becomes a level load
            }
            try {
                return active->ui.invoke(address);
            } catch (const std::exception& e) {
                active->log(OPENRAC_LOG_ERROR, e.what());
                return 0;
            }
        }
        active->ui.restore();
        active->flush();
    }
    return original_loader ? original_loader(value) : 0;
}

static int totals(sp_totals* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    *out =
        {sizeof(*out),
         static_cast<std::uint32_t>(kCatalogueSize),
         static_cast<std::uint32_t>(active->ledger.count()),
         667,
         active->ledger.earned_points()};
    return SP_OK;
}

static int get(std::uint32_t id, sp_achievement* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    const auto* d = find_definition(id);
    if (!d) {
        return SP_NOT_FOUND;
    }
    const auto measured = active->engine.measured(id);
    *out =
        {sizeof(*out),
         d->id,
         d->points,
         d->native_index,
         d->title,
         d->description,
         d->author,
         active->ledger.unlocked(id) ? 1u : 0u,
         measured.first,
         measured.second,
         active->ledger.timestamp(id)};
    return SP_OK;
}

static int at(std::uint32_t i, sp_achievement* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (i >= kCatalogueSize) {
        return SP_NOT_FOUND;
    }
    return get(kCatalogue[i].id, out);
}

static int enabled(std::uint32_t id) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!find_definition(id)) {
        return SP_NOT_FOUND;
    }
    return active->policy.enabled(id) ? 1 : 0;
}

static int active_totals(sp_totals* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    *out =
        {sizeof(*out),
         active->policy.count(),
         active->policy.unlocked(active->ledger),
         active->policy.points(),
         active->policy.earned_points(active->ledger)};
    return SP_OK;
}

static int subscribe(sp_listener fn, void* user, std::uint32_t* token) {
    if (!active || active->stopping) {
        return SP_UNAVAILABLE;
    }
    if (active->dispatching) {
        return SP_BUSY;
    }
    if (!fn || !token) {
        return SP_INVALID;
    }
    if (active->next_token == std::numeric_limits<std::uint32_t>::max()) {
        return SP_BUSY;
    }
    for (auto& l : active->listeners) {
        if (!l.fn) {
            l = {active->next_token++, fn, user};
            *token = l.token;
            return SP_OK;
        }
    }
    return SP_BUSY;
}

static int unsubscribe(std::uint32_t token) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!token) {
        return SP_INVALID;
    }
    for (auto& l : active->listeners) {
        if (l.token == token) {
            l = {};
            return SP_OK;
        }
    }
    return SP_NOT_FOUND;
}

static const sp_api service{
    sizeof(sp_api),
    OPENRAC_SKILL_POINTS_ABI,
    totals,
    at,
    get,
    subscribe,
    unsubscribe,
    enabled,
    active_totals
};
}  // namespace skillpoints

namespace {
std::unique_ptr<skillpoints::Runtime> session;
}

extern "C" {
OPENRAC_MOD_DEFINE("skill-points-ra", "0.1.2")
}

extern "C" OPENRAC_MOD_EXPORT const sp_api* openrac_skill_points_query(std::uint32_t abi) {
    return abi == OPENRAC_SKILL_POINTS_ABI && skillpoints::active ? &skillpoints::service : nullptr;
}

extern "C" OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    session.reset();
    if (!api) {
        return 1;
    }
    try {
        if (api->api_major != 1 || !api->game || !api->version || std::strcmp(api->game, "rac1")
            || std::strcmp(api->version, "ntsc")) {
            throw std::runtime_error("This achievement set requires Ratchet & Clank NTSC-U");
        }
        if (!api->memory || !api->function || !api->function_at || !api->relocate_data
            || !api->loaded_level || !api->stack_push || !api->stack_pop) {
            throw std::runtime_error("Incomplete OpenRAC mod API");
        }
        auto next = std::make_unique<skillpoints::Runtime>(api, mod);
        next->policy.set_enabled(
            skillpoints::kKerwanTimeTrial, api->setting_bool(mod, "kerwan_time_trial") != 0
        );
        const auto* profile = api->setting_string(mod, "profile");
        next->store = std::make_unique<skillpoints::FileStore>(
            skillpoints::profile_path(profile ? profile : ""), next->ledger
        );
        const int result = api->hook(
            mod,
            "rac1.level.load_gameplay",
            "u32(i32)",
            reinterpret_cast<void*>(skillpoints::relay),
            reinterpret_cast<void**>(&skillpoints::original_loader)
        );
        if (result != OPENRAC_HOOK_OK) {
            throw std::runtime_error(
                "Cannot register level/UI callback u32(i32): hook result " + std::to_string(result)
            );
        }
        if (api->on_frame(mod, skillpoints::frame_callback, nullptr) != 0) {
            throw std::runtime_error("Cannot register achievement frame callback");
        }
        skillpoints::active = next.get();
        next->log(
            OPENRAC_LOG_INFO,
            "178 official RetroAchievements rules loaded; 30 original skill points + 148 new; "
            "offline profile ready; active="
                + std::to_string(next->policy.count())
        );
        session = std::move(next);
        return 0;
    } catch (const std::exception& e) {
        api->fail(mod, e.what());
        return 1;
    }
}

extern "C" OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    session.reset();
}

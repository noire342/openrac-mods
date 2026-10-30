// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.hpp"

#include <chrono>

namespace skillpoints {
Runtime* active = nullptr;

Runtime::Runtime(const openrac_api* a, openrac_mod* m)
    : api(a),
      mod(m),
      memory(a),
      engine(memory, ledger, policy),
      ui(a, ledger, engine) {}

void Runtime::log(int level, const std::string& text) const {
    api->log(mod, level, text.c_str());
}

void Runtime::emit(std::uint32_t type, std::uint32_t id) {
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

void Runtime::flush() {
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

void Runtime::tick() {
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
        log(OPENRAC_LOG_ERROR,
            "An unlock used an unreadable operand and was rejected; other achievements continue");
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

Runtime::~Runtime() {
    stopping = true;
    emit(SP_EVENT_STOPPING, 0);
    ui.restore();
    flush();
    if (active == this) {
        active = nullptr;
    }
}
}  // namespace skillpoints

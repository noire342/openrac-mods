// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <memory>
#include <string>

#include "game_memory.hpp"
#include "native_ui.hpp"
#include "skill_points_api.h"
#include "store.hpp"

namespace skillpoints {
class Runtime {
public:
    const openrac_api* api;
    openrac_mod* mod;
    GameMemory memory;
    Ledger ledger;
    AchievementPolicy policy;
    Engine engine;
    NativeUi ui;
    std::unique_ptr<IStore> store;

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

    explicit Runtime(const openrac_api*, openrac_mod*);
    void log(int level, const std::string& text) const;
    void emit(std::uint32_t type, std::uint32_t id);
    void flush();
    void tick();
    ~Runtime();
};

extern Runtime* active;
}  // namespace skillpoints

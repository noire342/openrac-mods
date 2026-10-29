// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstddef>

#include "check.hpp"
#include "events.hpp"

static_assert(
    offsetof(weapon_xp_event_v1, flags) == WEAPON_XP_EVENT_V1_BASE_SIZE,
    "ABI v1's original event prefix must remain unchanged"
);

using namespace weaponxp;

struct State {
    Events* events;
    std::uint32_t token = 0;
    int calls = 0;
};

void once(void* context, const weapon_xp_event_v1*) {
    auto& state = *static_cast<State*>(context);
    ++state.calls;
    CHECK(state.events->subscribe(once, &state, &state.token) == WEAPON_XP_BUSY);
    CHECK(state.events->unsubscribe(state.token) == WEAPON_XP_OK);
}

int main() {
    Events hub;
    State state{&hub};
    CHECK(hub.subscribe(nullptr, nullptr, &state.token) == WEAPON_XP_INVALID_ARGUMENT);
    CHECK(hub.subscribe(once, &state, &state.token) == WEAPON_XP_OK);
    weapon_xp_event_v1 event{};
    event.size = sizeof event;
    hub.emit(event);
    hub.emit(event);
    CHECK(state.calls == 1 && !hub.dispatching());
    CHECK(hub.unsubscribe(state.token) == WEAPON_XP_NOT_FOUND);
    auto throwing = +[](void*, const weapon_xp_event_v1*) {
        throw 1;
    };
    CHECK(hub.subscribe(throwing, nullptr, &state.token) == WEAPON_XP_OK);
    hub.emit(event);  // A broken observer is disconnected; no exception crosses the C ABI.
    CHECK(hub.unsubscribe(state.token) == WEAPON_XP_NOT_FOUND);
    std::uint32_t token = 0;
    for (int i = 0; i < 16; ++i) {
        CHECK(hub.subscribe(+[](void*, const weapon_xp_event_v1*) {}, nullptr, &token) == 0);
    }
    CHECK(hub.subscribe(once, &state, &token) == WEAPON_XP_FULL);
    std::puts("versioned event subscriptions and reentrancy: ok");
}

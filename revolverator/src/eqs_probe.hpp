// SPDX-License-Identifier: GPL-3.0-or-later
// Optional interop with glawks/expanded-quick-select v1.0.3+ (MIT API v1).
// Never loads another plugin or introduces a hard dependency on EQS.
#pragma once
#include <array>
#include <cstdint>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace revolverator::eqs_probe {
constexpr std::uint32_t no_page = UINT32_MAX;
constexpr std::uint32_t gameplay_open = 1u;
struct State {
    std::uint32_t size = sizeof(State);
    std::uint32_t page_count = 0;
    std::uint32_t gameplay_page = no_page;
    std::uint32_t editor_page = no_page;
    std::uint32_t flags = 0;
};
struct Slots {
    std::uint32_t size = sizeof(Slots);
    std::uint32_t count = 0;
    std::array<std::int32_t, 8> items{};
};
struct Api {
    std::uint32_t size;
    std::uint32_t version;
    int (*get_state)(State*);
    int (*get_slots)(std::uint32_t, Slots*);
};
static_assert(sizeof(State) == 20, "EQS state ABI mismatch");
static_assert(sizeof(Slots) == 40, "EQS slots ABI mismatch");
static_assert(sizeof(Api) == sizeof(void*) * 2 + 8, "EQS API ABI mismatch");

inline const Api* discover() noexcept {
#if defined(_WIN32)
    // GetModuleHandle never loads a DLL: non-EQS users keep the existing path.
    const HMODULE module = GetModuleHandleW(L"expanded_quick_select.dll");
    if (!module) return nullptr;
    using Query = const Api* (*)(std::uint32_t);
    const auto query = reinterpret_cast<Query>(
        GetProcAddress(module, "openrac_expanded_quick_select_query"));
    if (!query) return nullptr;
    const Api* api = query(1u);
    if (api && api->size >= sizeof(Api) && api->version == 1u && api->get_state && api->get_slots)
        return api;
#endif
    return nullptr;
}
inline bool get_state(const Api& api, State& state) noexcept {
    return api.get_state(&state) == 1 && state.size >= sizeof(State) &&
        state.page_count >= 1 && state.page_count <= 8;
}
inline bool installed() noexcept {
    State state{};
    const auto* api = discover();
    return api && get_state(*api, state);
}
inline bool gameplay_slots(const Api& api, Slots& slots) noexcept {
    State state{};
    return get_state(api, state) && (state.flags & gameplay_open) &&
        state.gameplay_page < state.page_count &&
        api.get_slots(state.gameplay_page, &slots) == 1 &&
        slots.size >= sizeof(Slots) && slots.count == slots.items.size();
}
}  // namespace revolverator::eqs_probe

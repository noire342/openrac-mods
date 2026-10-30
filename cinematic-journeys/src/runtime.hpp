// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "discs.hpp"
#include "openrac/openrac_mod.h"
#include "policy.hpp"

namespace cinematic {
class Clock {
public:
    virtual ~Clock() = default;
    virtual double seconds() const noexcept = 0;
};

class SteadyClock final : public Clock {
public:
    double seconds() const noexcept override;
};

// Only this adapter knows the SDK, relocated guest addresses and callback order.
class Runtime final {
public:
    Runtime(const openrac_api& api, openrac_mod* mod, const Clock& clock);
    void load();
    void pad(int port, std::uint8_t* report, int size);
    void process(std::uint32_t state, std::uint32_t report, int size);
    void set_original(void* original) noexcept;

private:
    const openrac_api& m_api;
    openrac_mod* m_mod;
    const Clock& m_clock;
    const Disc* m_disc = nullptr;
    FlightMinimum m_minimum;
    bool m_protect, m_restore_start = false;
    bool m_holding = false;
    bool m_trace = false;
    std::uint32_t m_trace_state = ~0u;
    int m_trace_ship = -100, m_trace_busy = -100, m_trace_phase = -100;
    using Process = void (*)(std::uint32_t, std::uint32_t, std::int32_t);
    Process m_original = nullptr;
    bool scene() const;
    bool ship_scene() const;
    void flight();
    template <class T>
    T read(std::uint32_t at) const;
    template <class T>
    void write(std::uint32_t at, T value);
};
}  // namespace cinematic

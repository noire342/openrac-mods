// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace cinematic {
double SteadyClock::seconds() const noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Runtime::Runtime(const openrac_api& api, openrac_mod* mod, const Clock& clock)
    : m_api(api),
      m_mod(mod),
      m_clock(clock),
      m_minimum(api.setting_float(mod, "minimum_flight_seconds")),
      m_protect(api.setting_bool(mod, "protect_scenes") != 0) {}

template <class T>
T Runtime::read(std::uint32_t at) const {
    T value{};
    std::memcpy(&value, m_api.memory + at, sizeof value);
    return value;
}

template <class T>
void Runtime::write(std::uint32_t at, T value) {
    std::memcpy(m_api.memory + at, &value, sizeof value);
}

void Runtime::load() {
    if (std::strcmp(m_api.game, "rac1")) {
        throw std::runtime_error("Cinematic Journeys requires rac1");
    }
    for (const auto& disc : discs) {
        if (!std::strcmp(m_api.version, disc.version)) {
            m_disc = &disc;
        }
    }
    if (!m_disc) {
        throw std::runtime_error("Cinematic Journeys supports PAL and NTSC-U");
    }
    if (!m_api.function(m_disc->flight_update)) {
        throw std::runtime_error(
            "Cinematic Journeys: this build lacks the expected original flight update"
        );
    }
#if defined(_MSC_VER)
    char* trace = nullptr;
    std::size_t trace_size = 0;
    if (_dupenv_s(&trace, &trace_size, "CINEMATIC_JOURNEYS_TRACE") == 0) {
        m_trace = trace && std::strcmp(trace, "1") == 0;
    }
    std::free(trace);
#else
    const char* trace = std::getenv("CINEMATIC_JOURNEYS_TRACE");
    m_trace = trace && std::strcmp(trace, "1") == 0;
#endif
    m_api.log(
        m_mod,
        OPENRAC_LOG_INFO,
        "Protecting scenes: L1+L2+R1+R2+Start skips deliberately; minimum flight uses original "
        "animated passes"
    );
}

bool Runtime::scene() const {
    const auto level = m_api.loaded_level();
    const int mode = level >= 0 && level < 19
                         ? read<std::int32_t>(m_api.relocate_level_data(0, m_disc->level_mode))
                         : -1;
    return protected_scene(
        level,
        mode,
        read<std::int32_t>(m_disc->ship + 0x20),
        read<std::int16_t>(m_disc->loader_busy) != 0
    );
}

bool Runtime::ship_scene() const {
    const int state = read<std::int32_t>(m_disc->ship + 0x20);
    const int level = m_api.loaded_level();
    const bool landing =
        level >= 0 && level < 19
        && read<std::int32_t>(m_api.relocate_level_data(0, m_disc->level_mode)) == 6
        && (state == 0 || state == 8);
    const bool travelling =
        read<std::int16_t>(m_disc->loader_busy) != 0 && (state == 3 || state == 4 || state == 7);
    return landing || travelling;
}

void Runtime::pad(int port, std::uint8_t* report, int size) {
    if (port != 0 || !report || size < 2) {
        return;
    }
    const auto raw = static_cast<std::uint16_t>(report[0] | (report[1] << 8));
    const auto filtered = filter_buttons(raw, m_protect, ship_scene());
    m_restore_start = (raw & start) == 0 && (filtered & start) != 0;
    write_buttons(report, static_cast<std::size_t>(size), filtered);
}

void Runtime::flight() {
    const bool active = read<std::int32_t>(m_disc->ship + 0x20) == 4
                        && read<std::int16_t>(m_disc->loader_busy) == 1
                        && read<std::int32_t>(m_api.relocate_data(m_disc->scene_done)) == 0;
    const bool hold = m_minimum.hold(active, m_clock.seconds());
    if (hold && !m_holding) {
        m_api.log(m_mod, OPENRAC_LOG_DEBUG, "Minimum flight started");
    }
    if (!hold && m_holding) {
        m_api.log(m_mod, OPENRAC_LOG_DEBUG, "Minimum flight released");
    }
    m_holding = hold;
    // In the original update, phase 0 selects another cruising pass; phase 1 selects the final
    // pass. Let the current animation end normally, but keep choosing cruise until the minimum.
    // Never touch completion, loader stages, resource ownership, frame/bank counters or fades.
    if (hold && read<std::int32_t>(m_disc->ship + 0x5c) == 1) {
        write<std::int32_t>(m_disc->ship + 0x5c, 0);
    }
}

void Runtime::set_original(void* original) noexcept {
    m_original = reinterpret_cast<Process>(original);
}

void Runtime::process(std::uint32_t state, std::uint32_t report, int size) {
    if (m_trace) {
        const int ship = read<std::int32_t>(m_disc->ship + 0x20);
        const int busy = read<std::int16_t>(m_disc->loader_busy);
        const int phase = read<std::int32_t>(m_disc->ship + 0x5c);
        if (state != m_trace_state || ship != m_trace_ship || busy != m_trace_busy
            || phase != m_trace_phase) {
            char line[200];
            std::snprintf(
                line,
                sizeof line,
                "trace pad=%x ship=%d busy=%d phase=%d level=%d clock=%.3f",
                state,
                ship,
                busy,
                phase,
                m_api.loaded_level(),
                m_clock.seconds()
            );
            m_api.log(m_mod, OPENRAC_LOG_INFO, line);
            m_trace_state = state;
            m_trace_ship = ship;
            m_trace_busy = busy;
            m_trace_phase = phase;
        }
    }
    if (state != m_disc->pad || size < 2) {
        m_original(state, report, size);
        return;
    }
    flight();
    auto* bytes = m_api.memory + report;
    const auto previous = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
    // Movie playback consumes on_pad directly, and cannot reach this hook. Reintroduce Start
    // only for the normal game's menus/pause path, after the port's automatic skip expansion.
    const bool restore = m_restore_start && !scene();
    m_restore_start = false;
    if (restore) {
        write_buttons(bytes, static_cast<std::size_t>(size), previous & ~start);
    }
    m_original(state, report, size);
    if (restore) {
        const auto after = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
        write_buttons(bytes, static_cast<std::size_t>(size), after | start);
    }
}
}  // namespace cinematic

// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_hud.hpp"
#include "native_tables.hpp"
#include "air_alarm.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>

namespace clank {
namespace {
using Element = void (*)(std::uint32_t);
std::atomic<NativeHud *> hud_context{};
Element original_wheel_init{};
Element original_air_update{};
int (*original_wheel_draw)(std::uint32_t){};
int (*original_air_draw)(std::uint32_t){};
void wheel_init_bridge(std::uint32_t record) {
    if (auto *hud = hud_context.load(std::memory_order_acquire))
        hud->wheel_init(record);
    else if (original_wheel_init)
        original_wheel_init(record);
}
void air_update_bridge(std::uint32_t record) {
    if (auto *hud = hud_context.load(std::memory_order_acquire))
        hud->air_update(record);
    else if (original_air_update)
        original_air_update(record);
}
int wheel_draw_bridge(std::uint32_t record) {
    if (auto *hud = hud_context.load(std::memory_order_acquire))
        return hud->wheel_draw(record);
    return original_wheel_draw ? original_wheel_draw(record) : 0;
}
int air_draw_bridge(std::uint32_t record) {
    if (auto *hud = hud_context.load(std::memory_order_acquire))
        return hud->air_draw(record);
    return original_air_draw ? original_air_draw(record) : 0;
}
} // namespace
NativeHud::NativeHud(const openrac_api &api, openrac_mod *mod, const GadgebotIcon &icon,
                     const AlarmSource &alarm)
    : m_api(api), m_mod(mod), m_assets(api, icon), m_alarm(api, alarm) {
    m_show = reinterpret_cast<Show>(api.function(profile::hud_show));
    m_remove = reinterpret_cast<Remove>(api.function(profile::hud_remove));
    m_scale_ticks = reinterpret_cast<int (*)(int)>(api.function(profile::scale_ticks));
    m_warning = reinterpret_cast<int (*)(int, int)>(api.function(profile::oxygen_warning));
    if (!m_show || !m_remove || !m_scale_ticks || !m_warning)
        throw std::runtime_error("Native Clank HUD services are unavailable");
    for (unsigned i = 0; i < m_functions.size(); ++i) {
        m_functions[i] = api.function(hud_callbacks[i].name);
        if (!m_functions[i])
            throw std::runtime_error("Native Clank HUD callbacks are unavailable");
    }
    original_wheel_init = reinterpret_cast<Element>(m_functions[0]);
    original_air_update = reinterpret_cast<Element>(m_functions[4]);
    original_wheel_draw = reinterpret_cast<int (*)(std::uint32_t)>(m_functions[2]);
    original_air_draw = reinterpret_cast<int (*)(std::uint32_t)>(m_functions[5]);
    hud_context.store(this, std::memory_order_release);
}
NativeHud::~NativeHud() {
    clear();
    hud_context.store(nullptr, std::memory_order_release);
}
void *NativeHud::callback(std::uint32_t address) const noexcept {
    if (m_level < 0 || m_level >= 19)
        return nullptr;
    for (unsigned i = 0; i < hud_callbacks.size(); ++i) {
        const auto &entry = hud_callbacks[i];
        if (address == entry.canonical ||
            (entry.addresses[m_level] && address == entry.addresses[m_level]))
            return i == 0   ? reinterpret_cast<void *>(wheel_init_bridge)
                   : i == 2 ? reinterpret_cast<void *>(wheel_draw_bridge)
                   : i == 4 ? reinterpret_cast<void *>(air_update_bridge)
                   : i == 5 ? reinterpret_cast<void *>(air_draw_bridge)
                            : m_functions[i];
    }
    return nullptr;
}
std::uint32_t NativeHud::epoch() const noexcept {
    openrac_play_status s{};
    s.size = sizeof(s);
    m_api.play_status(&s);
    return s.loads;
}
bool NativeHud::same_epoch(std::uint32_t value) const noexcept { return value == epoch(); }
void NativeHud::wheel_init(std::uint32_t record) {
    original_wheel_init(record);
    // Standalone Clank enters directly in the selectable phase. The original
    // deferred phase depends on a flag normally set by Clank's story handoff.
    const std::uint32_t selectable = 0x010000FFu;
    if (record >= 0x10000 && record <= 0x02000000 - 0x80)
        std::memcpy(m_api.memory + record + 0x78, &selectable, sizeof(selectable));
    m_wheel_record = record;
    m_wheel_id = read<int>(record + 0x64);
    m_wheel_epoch = epoch();
    m_api.log(m_mod, OPENRAC_LOG_INFO, "Original Gadgebot command wheel opened");
}
int NativeHud::wheel_draw(std::uint32_t record) {
    m_assets.before_draw();
    return original_wheel_draw(record);
}
int NativeHud::air_draw(std::uint32_t record) {
    if (read<int>(record) != EnergyArtwork::meter_icon || !same_epoch(m_air_epoch))
        return original_air_draw(record);
    m_air_record = record;
    const auto counter = read<int>(m_api.relocate_level_data(0, profile::rendered_frame_count));
    const auto alarm = air_alarm(read<int>(record + 0x74), counter, m_scale_ticks(1),
                                 read<float>(m_api.relocate_data(profile::frame_scale_sq)));
    const bool critical = read<int>(record + 0x74) <= 3333;
    const bool entered_critical = critical && !m_was_critical;
    m_was_critical = critical;
    m_air_frame = alarm.frame;
    if (m_air_frame != m_last_air_frame) {
        m_api.log(m_mod, OPENRAC_LOG_INFO,
                  m_air_frame ? "Clank energy meter: critical" : "Clank energy meter: normal");
        m_last_air_frame = m_air_frame;
    }
    m_drawing_air = true;
    m_warning_played = false;
    struct Drawing {
        bool &flag;
        ~Drawing() { flag = false; }
    } drawing{m_drawing_air};
    m_assets.before_draw();
    const int width = original_air_draw(record);
    if ((alarm.warning || (entered_critical && alarm.frame)) && counter != m_last_warning_tick &&
        !m_warning_played)
        m_warning(0xB, 0);
    if (m_warning_played)
        m_last_warning_tick = counter;
    return width;
}
bool NativeHud::pending_memory() const noexcept {
    return m_air_record && read<int>(m_air_record) == EnergyArtwork::meter_icon;
}
bool NativeHud::remap_air_icon(int &id, int &frame) const noexcept {
    if (!m_drawing_air || id != EnergyArtwork::meter_icon)
        return false;
    if (frame == 0 || frame == 1) {
        id = 0x753F;
        frame = m_air_frame;
        return true;
    }
    if (frame == 2) {
        id = EnergyArtwork::icon;
        frame = m_air_frame;
        return true;
    }
    return false;
}
void NativeHud::warning_played() noexcept {
    m_warning_played = true;
    m_api.log(m_mod, OPENRAC_LOG_INFO, "Clank energy warning: native sound requested");
}
int NativeHud::play_warning(int variant) {
    if (m_air_id < 0)
        return -1;
    const int voice = m_alarm.play(variant);
    if (voice >= 0)
        warning_played();
    else
        m_api.log(m_mod, OPENRAC_LOG_WARN, "Native energy alarm could not start a voice");
    return voice;
}
void NativeHud::air_update(std::uint32_t record) {
    if (read<int>(record) == EnergyArtwork::meter_icon && same_epoch(m_air_epoch)) {
        m_air_record = record;
        m_assets.charge(m_current_air);
    }
    original_air_update(record);
}
void NativeHud::air(const AirStatus &status) {
    if (!status.visible) {
        if (m_air_id >= 0 && status.remaining >= 1.0f) {
            m_current_air = 10000;
            m_assets.charge(10000);
        }
        hide_air();
        return;
    }
    const int value =
        static_cast<int>(std::lround(std::clamp(status.remaining, 0.0f, 1.0f) * 10000));
    m_assets.charge(value);
    m_current_air = value;
    if (m_air_id < 0) {
        m_air_epoch = epoch();
        m_air_id = m_show(4, EnergyArtwork::meter_icon, hud_callbacks[3].canonical,
                          hud_callbacks[4].canonical, hud_callbacks[5].canonical,
                          m_assets.charge_address(), 10000);
        m_api.log(m_mod, OPENRAC_LOG_INFO, "Clank distance limit: energy meter active");
    }
}
void NativeHud::hide_air() noexcept {
    if (m_air_id >= 0 && same_epoch(m_air_epoch)) {
        m_remove(m_air_id);
    }
    m_air_id = -1;
}
void NativeHud::clear() noexcept {
    hide_air();
    if (m_wheel_id >= 0 && same_epoch(m_wheel_epoch) &&
        read<int>(m_wheel_record + 0x64) == m_wheel_id)
        m_remove(m_wheel_id);
    m_wheel_id = -1;
    m_wheel_record = 0;
}
float NativeHud::game_seconds() const noexcept {
    const auto dt = read<float>(m_api.relocate_data(profile::game_time_step));
    return std::isfinite(dt) && dt > 0 ? dt : 0;
}
float NativeHud::native_air_loss() const noexcept {
    const int ticks = m_scale_ticks(0x3C);
    return ticks > 0 && ticks <= 10000 ? (10000 / (ticks * 15)) / 10000.0f : 0;
}
} // namespace clank

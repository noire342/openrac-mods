// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_upgrades.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace weaponxp {
namespace {
bool valid(std::uint32_t at, unsigned size) {
    return at >= 0x10000 && at <= 0x2000000 - size;
}

constexpr unsigned kNameSize = 128, kBufferSize = 0x100 + unsigned(kWeapons.size()) * kNameSize;
}  // namespace

template <class T>
T NativeUpgrades::read(std::uint32_t at) const {
    T value{};
    if (valid(at, sizeof value)) {
        std::memcpy(&value, m_api->memory + at, sizeof value);
    }
    return value;
}

template <class T>
void NativeUpgrades::write(std::uint32_t at, T value) {
    if (valid(at, sizeof value)) {
        std::memcpy(m_api->memory + at, &value, sizeof value);
    }
}

NativeUpgrades::NativeUpgrades(const openrac_api* api, const Disc& disc)
    : m_api(api),
      m_disc(disc) {
    const bool pal = std::strcmp(disc.version, "pal") == 0;
    m_allocate = reinterpret_cast<Allocate>(api->function(pal ? "func_001FFAB8" : "FUN_001ff288"));
    m_present =
        reinterpret_cast<Present>(api->function(pal ? "func_L01_002AFD00" : "FUN_L01_002aea70"));
    m_place =
        reinterpret_cast<Place>(api->function(pal ? "func_L00_00217718" : "FUN_L00_00216f90"));
    m_notify =
        reinterpret_cast<Notify>(api->function(pal ? "func_L00_00264DB8" : "FUN_L00_00263d40"));
    if (!m_allocate || !m_present || !m_place || !m_notify) {
        throw std::runtime_error("OpenRAC has no native acquisition presentation");
    }
}

NativeUpgrades::~NativeUpgrades() {
    reset();
}

std::uint32_t NativeUpgrades::global(std::uint32_t pal, std::uint32_t ntsc) const {
    return m_api->relocate_data(std::strcmp(m_disc.version, "pal") == 0 ? pal : ntsc);
}

std::uint32_t NativeUpgrades::vendor() const {
    return m_api
        ->relocate_level_data(1, std::strcmp(m_disc.version, "pal") == 0 ? 0x1CABC0 : 0x1CA940);
}

void NativeUpgrades::enqueue(int item, int before, int after) {
    if (weapon_index(item) < 0) {
        return;
    }
    for (int level = std::max(2, before + 1); level <= std::min(kMaxLevel, after); ++level) {
        m_queue.push_back({item, level});
    }
}

bool NativeUpgrades::supports_presentation(int item) const {
    if (weapon_index(item) < 0) return false;
    const bool pal = std::strcmp(m_disc.version, "pal") == 0;
    const auto scenes = m_api->relocate_level_data(1, pal ? 0x1CA720 : 0x1CA4A0);
    const auto entry = scenes + unsigned(item) * 4;
    return valid(entry, 4) && read<int>(entry) >= 0;
}

bool NativeUpgrades::request_presentation(int item) {
    if (pending() || !supports_presentation(item)) return false;
    m_queue.push_back({item, 0, true});
    return true;
}

void NativeUpgrades::rename(Binding& b, int level) {
    if (b.level == level
        || read<unsigned>(b.entry) != (b.level == 1 ? b.original : b.replacement)) {
        return;
    }
    const auto name = b.name + " v" + std::to_string(level);
    if (name.size() >= kNameSize) {
        return;
    }
    std::memcpy(m_api->memory + b.replacement, name.c_str(), name.size() + 1);
    write(b.entry, level == 1 ? b.original : b.replacement);
    b.level = level;
}

void NativeUpgrades::names(const Progression& state, bool enabled) {
    const auto bank = read<unsigned>(global(0x15F780, 0x15F6A0));
    const int count = read<int>(global(0x1997FC, 0x1996FC));
    if (!valid(bank, 16) || count <= 0 || count > 20000 || !valid(bank, count * 16)) {
        return;
    }
    if (m_bank && bank != m_bank && !m_active) {
        reset();
    }
    m_bank = bank;
    if (m_bindings.empty()) {
        if (!m_buffer) {
            m_buffer = m_allocate(kBufferSize);
        }
        if (!valid(m_buffer, kBufferSize)) {
            m_buffer = 0;
            return;
        }
        const auto defs = global(0x1864D0, 0x1863D0);
        for (const auto& weapon : kWeapons) {
            const int id = read<int>(defs + weapon.item * 0x4C);
            for (int i = 0; i < count; ++i) {
                const auto entry = bank + i * 16;
                if (read<int>(entry + 4) != id) {
                    continue;
                }
                const auto original = read<unsigned>(entry);
                if (!valid(original, kNameSize)) {
                    break;
                }
                const auto* text = reinterpret_cast<const char*>(m_api->memory + original);
                const auto* end = static_cast<const char*>(std::memchr(text, 0, kNameSize - 5));
                if (!end || end == text) {
                    break;
                }
                m_bindings.push_back(
                    {entry,
                     original,
                     m_buffer + 0x100 + unsigned(weapon_index(weapon.item)) * kNameSize,
                     weapon.item,
                     1,
                     std::string(text, end)}
                );
                break;
            }
        }
    }
    for (auto& b : m_bindings) {
        if (requested_presentation_pending() && m_queue.front().item == b.item) continue;
        const int target = m_active && !m_queue.empty() && m_queue.front().item == b.item
                               ? m_queue.front().level
                               : state.weapon_level(b.item);
        rename(b, enabled ? target : 1);
    }
}

void NativeUpgrades::tick(const Progression& state, bool safe, bool versioned_names) {
    names(state, versioned_names);
    const auto hero = m_api->relocate_data(m_disc.hero);
    const auto mode = m_api->relocate_level_data(0, m_disc.mode);
    if (m_active) {
        if (read<int>(mode) != 0) {
            m_completed = 0;
            hide_player();
            return;
        }
        // The mode can return to gameplay one tick before the scene actors retire.
        // Keep input and the real body out of that transition frame.
        if (++m_completed < 2) {
            hide_player();
            return;
        }
        // The native exit places Ratchet beside its trigger. Our trigger is a snapshot of him;
        // return to the exact combat location, facing and velocity instead.
        std::memcpy(m_api->memory + m_buffer + 0x80, m_position.data(), 16);
        std::memcpy(m_api->memory + m_buffer + 0x90, m_rotation.data(), 16);
        m_place(m_buffer + 0x80, m_buffer + 0x90, 0, 1);
        std::memcpy(m_api->memory + hero + 0xE0, m_velocity.data(), 16);
        restore_player();
        write(hero + 0x20B8, m_previous_item);
        std::memcpy(m_api->memory + vendor(), m_vendor.data(), m_vendor.size());
        if (!m_queue.front().presentation_only) notify(m_queue.front().item, m_queue.front().level);
        m_active = false;
        m_queue.pop_front();
        m_delay = 180;
        return;
    }
    if (!safe || !m_buffer || m_queue.empty()) {
        return;
    }
    const int hero_state = read<int>(hero + 0x2084);
    if (read<int>(hero + 0x22A8) <= 0 || (hero_state != 0 && hero_state != 1 && hero_state != 2)) {
        return;
    }
    if (m_delay > 0) {
        --m_delay;
        return;
    }
    // Releasing controls through the final native pad hook lets walking and a sustained
    // attack finish normally before the native acquisition scene takes ownership.
    if (!m_preparing) {
        m_preparing = true;
        m_quiet = 0;
        return;
    }
    const float vx = read<float>(hero + 0xE0), vy = read<float>(hero + 0xE4);
    if (read<unsigned char>(hero + 0x20A8) || !std::isfinite(vx) || !std::isfinite(vy)
        || vx * vx + vy * vy > .0001f) {
        m_quiet = 0;
        return;
    }
    if (++m_quiet < 2) {
        return;
    }
    const auto body = read<unsigned>(hero + 0x2080);
    if (!valid(body, 0x100)) {
        return;
    }
    const auto next = m_queue.front();
    const bool pal = std::strcmp(m_disc.version, "pal") == 0;
    const auto scenes = m_api->relocate_level_data(1, pal ? 0x1CA720 : 0x1CA4A0);
    if (read<int>(scenes + next.item * 4) < 0) {
        // RYNO is awarded by the shady salesman and has no retail vendor acquisition scene.
        // Preserve its native acquisition-message path rather than indexing an invalid archive.
        if (!next.presentation_only) notify(next.item, next.level);
        m_queue.pop_front();
        m_preparing = false;
        m_delay = 180;
        return;
    }
    std::memcpy(m_vendor.data(), m_api->memory + vendor(), m_vendor.size());
    std::memcpy(m_position.data(), m_api->memory + hero + 0x80, 16);
    std::memcpy(m_rotation.data(), m_api->memory + hero + 0x90, 16);
    std::memcpy(m_velocity.data(), m_api->memory + hero + 0xE0, 16);
    m_previous_item = read<int>(m_api->relocate_data(m_disc.item_in_hand));
    std::memcpy(m_api->memory + m_buffer, m_api->memory + body, 0x100);
    m_active = true;
    m_preparing = false;
    m_completed = 0;
    for (auto& b : m_bindings) {
        if (b.item == next.item && !next.presentation_only) {
            rename(b, next.level);
        }
    }
    const auto game_camera = m_api->relocate_level_data(0, pal ? 0x166D00 : 0x166C80);
    const std::array<float, 3> camera{
        read<float>(game_camera + 0x140),
        read<float>(game_camera + 0x144),
        read<float>(game_camera + 0x148)
    };
    m_present(m_buffer, next.item);
    align_scene(camera);
    hide_player();
}

void NativeUpgrades::filter_report(unsigned char* report, int size) const {
    if (!active() || !report || size < 6) {
        return;
    }
    // The native report has a different button-byte order from SDK on_pad reports.
    // Block every gameplay action in either order, including partial trigger pressure.
    report[0] = 0xFF;
    report[1] = 0xFF;
    std::fill(report + 2, report + 6, 128);
    if (size > 6) {
        std::fill(report + 6, report + std::min(size, 18), 0);
    }
}

void NativeUpgrades::hide_player() {
    const auto hero = m_api->relocate_data(m_disc.hero);
    write<unsigned char>(hero + 0x20AC, 1);
    write<unsigned char>(hero + 0x20A5, 1);
    const auto body = read<unsigned>(hero + 0x2080);
    if (!valid(body, 256)) {
        return;
    }
    const bool pal = std::strcmp(m_disc.version, "pal") == 0;
    const auto scene = m_api->relocate_level_data(1, pal ? 0x16CD60 : 0x16CCE0);
    const int count = read<short>(scene + 0x44);
    // Never hide an actor used by the acquisition scene itself.
    bool separate = false;
    for (int i = 0; i < std::min(count, 14); ++i) {
        const auto actor = read<unsigned>(scene + 0x178 + i * 4);
        if (actor == body) {
            return;
        }
        separate |= valid(actor, 256) && read<short>(actor + 0xA6) == 0;
    }
    if (!separate) {
        return;
    }
    if (!m_hidden_body) {
        m_hidden_body = body;
        m_body_flags = read<unsigned short>(body + 0x34);
    }
    write<unsigned short>(body + 0x34, read<unsigned short>(body + 0x34) | 1);
}

void NativeUpgrades::restore_player() {
    if (m_hidden_body
        && m_hidden_body == read<unsigned>(m_api->relocate_data(m_disc.hero) + 0x2080)) {
        const auto flags = read<unsigned short>(m_hidden_body + 0x34);
        write<unsigned short>(m_hidden_body + 0x34, (flags & ~1u) | (m_body_flags & 1u));
    }
    m_hidden_body = 0;
}

void NativeUpgrades::notify(int item, int level) {
    for (auto& binding : m_bindings) {
        if (binding.item != item) {
            continue;
        }
        rename(binding, level);
        m_notify(read<int>(binding.entry + 4), 180);
        break;
    }
}

void NativeUpgrades::align_scene(const std::array<float, 3>& camera) {
    const bool pal = std::strcmp(m_disc.version, "pal") == 0;
    const auto scene = m_api->relocate_level_data(1, pal ? 0x16CD60 : 0x16CCE0);
    const auto camera_keys = read<unsigned>(scene + 0x54);
    const int count = read<short>(scene + 0x44);
    if (!valid(camera_keys, 32) || count < 1 || count > 14) {
        return;
    }
    unsigned hero_keys = 0;
    for (int i = 0; i < count; ++i) {
        const auto actor = read<unsigned>(scene + 0x178 + i * 4);
        if (valid(actor, 0x100) && read<short>(actor + 0xA6) == 0) {
            hero_keys = read<unsigned>(actor + 0x78);
            break;
        }
    }
    if (!valid(hero_keys, 16)) {
        return;
    }
    float position[4];
    std::memcpy(position, m_position.data(), 16);
    const float hx = read<float>(hero_keys), hy = read<float>(hero_keys + 4),
                hz = read<float>(hero_keys + 8);
    const float dx = read<float>(camera_keys) - hx, dy = read<float>(camera_keys + 4) - hy;
    if (dx * dx + dy * dy < 0.01f) {
        return;
    }
    // Use the side from which the player's collision-aware gameplay camera already sees Ratchet.
    // Keep the retail camera path, actor animation and framing; rotate/translate their stage only.
    const float yaw =
        std::atan2(camera[1] - position[1], camera[0] - position[0]) - std::atan2(dy, dx);
    const float c = std::cos(yaw), s = std::sin(yaw);
    const float basis[16] = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, yaw, 0};
    std::memcpy(m_api->memory + vendor() + 0x80, basis, sizeof basis);
    const float origin[4] =
        {position[0] - (c * hx - s * hy), position[1] - (s * hx + c * hy), position[2] - hz, 0};
    std::memcpy(m_api->memory + vendor() + 0xC0, origin, sizeof origin);
}

void NativeUpgrades::reset() {
    restore_player();
    for (const auto& b : m_bindings) {
        if (read<unsigned>(b.entry) == b.replacement) {
            write(b.entry, b.original);
        }
    }
    m_bindings.clear();
    if (m_buffer && !m_active) {
        const auto cursor = global(0x19A4E8, 0x19A3E8) + 0x10;
        // Reclaim only if nothing else was allocated after us. Never rewind another owner's heap.
        if (read<unsigned>(cursor) == m_buffer + kBufferSize) {
            write(cursor, m_buffer);
        }
    }
    m_buffer = m_bank = 0;
    // Pending celebrations survive a planet change; an interrupted one is replayed safely.
    m_active = false;
    m_preparing = false;
    m_quiet = m_completed = 0;
    m_delay = 30;
}
}  // namespace weaponxp

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <deque>
#include <string>
#include <vector>

#include "discs.hpp"
#include "name_providers.hpp"
#include "openrac/openrac_mod.h"
#include "progression.hpp"

namespace weaponxp {
// Owns native localized-name bindings and the original acquisition scene lifecycle.
// It never calls Item_Give, debits bolts, grants ammo or changes inventory ownership.
class NativeUpgrades {
public:
    NativeUpgrades(const openrac_api* api, const Disc& disc, NameProviders* names = nullptr);
    ~NativeUpgrades();
    void enqueue(int item, int before, int after);
    bool supports_presentation(int item) const;
    bool request_presentation(int item);
    bool requested_presentation_pending() const {
        return !m_queue.empty() && m_queue.front().presentation_only;
    }
    void tick(const Progression& state, bool safe, bool versioned_names = true);
    void reset(bool restore_guest = true);
    void filter_report(unsigned char* report, int size) const;

    bool pending() const { return active() || !m_queue.empty(); }

    bool active() const { return m_active || m_preparing; }

private:
    const openrac_api* m_api;
    const Disc& m_disc;
    NameProviders* m_name_providers;
    using Allocate = std::uint32_t (*)(int);
    using Present = void (*)(std::uint32_t, int);
    using Place = void (*)(std::uint32_t, std::uint32_t, int, int);
    using Notify = void (*)(int, int);
    Allocate m_allocate;
    Present m_present;
    Place m_place;
    Notify m_notify;
    std::uint32_t m_buffer = 0;
    std::uint32_t m_bank = 0;
    int m_language = -1;

    struct Binding {
        std::uint32_t entry, original, replacement;
        int item;
        std::string name;
        int level = 0;
        int language = -1;
        std::uint64_t name_revision = 0;
        bool projected = false;
    };

    std::vector<Binding> m_bindings;

    struct Upgrade {
        int item, level;
        bool presentation_only = false;
    };

    std::deque<Upgrade> m_queue;
    bool m_active = false;
    bool m_preparing = false;
    unsigned m_quiet = 0, m_completed = 0;
    unsigned m_hidden_body = 0;
    unsigned short m_body_flags = 0;
    int m_previous_item = 0, m_delay = 0;
    std::array<unsigned char, 0x220> m_vendor{};
    std::array<unsigned char, 16> m_position{}, m_rotation{}, m_velocity{};
    template <class T>
    T read(std::uint32_t at) const;
    template <class T>
    void write(std::uint32_t at, T value);
    std::uint32_t global(std::uint32_t pal, std::uint32_t ntsc) const;
    std::uint32_t vendor() const;
    void names(const Progression& state, bool enabled);
    void rename(Binding& binding, int level, bool enabled);
    void align_scene(const std::array<float, 3>& camera);
    void notify(int item, int level, bool versioned_names);
    void hide_player();
    void restore_player();
};
}  // namespace weaponxp

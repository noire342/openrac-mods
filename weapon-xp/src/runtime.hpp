// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "combat.hpp"
#include "discs.hpp"
#include "events.hpp"
#include "hud_bridge.hpp"
#include "hit_filters.hpp"
#include "native_damage.hpp"
#include "native_hud.hpp"
#include "native_morph_model.hpp"
#include "native_rewards.hpp"
#include "native_upgrades.hpp"
#include "openrac/openrac_mod.h"
#include "store.hpp"
#include "ui.hpp"

namespace weaponxp {

// OpenRAC adapter: translates guest memory and hook calls into domain events and draw commands.
// All mutable state belongs to one session; the only singleton is the SDK's C callback bridge.
class Runtime {
public:
    Runtime(const openrac_api* api, openrac_mod* mod);
    ~Runtime();
    void load();
    static const weapon_xp_api_v1* query(std::uint32_t abi);

private:
    using GetHit = std::uint32_t (*)(std::uint32_t, std::int32_t, std::int32_t);
    using Death = void (*)(std::uint32_t, std::int32_t, std::int32_t);
    using LoadLevel = std::uint32_t (*)(std::int32_t);
    using Hud = std::int32_t (*)(std::uint32_t);
    static Runtime* s_instance;
    const openrac_api* m_api;
    openrac_mod* m_mod;
    const Disc* m_disc = nullptr;
    Progression m_state;
    Settings m_settings;
    Combat m_combat;
    Events m_events;
    HitFilters m_filters;
    std::uint32_t m_level_epoch=0;
    bool m_ready = false;
    std::uint64_t m_revision = 1;
    std::string m_rules_error;
    std::filesystem::path m_rules_path;
    bool m_trace = false;
    unsigned m_draw_traces = 0;
    unsigned m_combat_traces = 0;
    std::unique_ptr<Store> m_store;
    std::unique_ptr<HudBridge> m_hud_bridge;
    std::unique_ptr<NativeHud> m_painter;
    std::unique_ptr<NativeMorphModel> m_morph_model;
    void (*m_morph_update)(unsigned) = nullptr;
    void update_morph(unsigned moby);
    std::unique_ptr<NativeDamage> m_native_damage;
    std::unique_ptr<NativeUpgrades> m_upgrades;
    Ui m_ui;
    bool m_dirty = false, m_save_warning = false;
    std::uint64_t m_tick = 0, m_saved_tick = 0;
    int m_last_level = -1;
    std::uint32_t m_table = 0;
    std::array<std::uint16_t, kWeapons.size()> m_base_ammo{};
    std::array<std::uint16_t, kWeapons.size()> m_applied_ammo{};
    GetHit m_get_hit = nullptr;
    Death m_death = nullptr;
    LoadLevel m_load_level = nullptr;
    Hud m_hud = nullptr;
    Hud m_gauge_hud = nullptr;
    void (*m_pad_report)(unsigned, unsigned, int) = nullptr;
    void (*m_relay_original)(std::uint32_t) = nullptr;

    struct DamageEdit {
        std::uint32_t hit = 0, owner = 0;
        float raw = 0, scaled = 0;
    };

    std::array<DamageEdit, 64> m_edits{};

    struct ExternalKey {
        std::uint64_t session = 0, event = 0, target = 0;
        std::uint32_t epoch = 0;
    };

    struct ExternalPrepared {
        ExternalKey key;
        std::uint32_t moby = 0, hit = 0;
        int target_class = -1, item = -1, reward = 0;
        float damage = 0;
        bool secondary = false, confirmed = false;
        std::uint64_t at = 0;
        std::array<unsigned char, 64> before{}, after{};
    };

    struct ExternalReplicated {
        ExternalKey key;
        std::uint32_t moby = 0, hit = 0;
        std::uint64_t at = 0;
        std::array<unsigned char, 64> payload{};
    };

    std::uint64_t m_external_scope = 0, m_external_highest = 0;
    std::size_t m_external_cursor = 0;
    bool m_external_processing = false;
    std::array<ExternalPrepared, 4096> m_external_prepared{};
    std::array<ExternalReplicated, 64> m_external_replicated{};

    struct Source {
        std::uint32_t moby;
        std::int16_t oclass;
        int item;
    };

    std::array<Source, 32> m_sources{};
    std::size_t m_source_cursor = 0;

    bool valid(std::uint32_t address, std::uint32_t size);
    template <class T>
    T read(std::uint32_t address) {
        T result{};
        if (valid(address, sizeof(T))) std::memcpy(&result, m_api->memory + address, sizeof(T));
        return result;
    }
    template <class T>
    void write(std::uint32_t address, T value) {
        if (valid(address, sizeof(T))) std::memcpy(m_api->memory + address, &value, sizeof(T));
    }
    std::uint32_t resident(std::uint32_t address);
    std::uint32_t overlay(std::uint32_t address);
    std::uint32_t hero();
    bool playing();
    void log(int severity, const std::string& message);
    void save() noexcept;
    void restore_hits();
    void reset_level();
    void remember_source();
    int source_item(std::uint32_t hit);
    std::uint32_t get_hit(std::uint32_t moby, std::int32_t mask, std::int32_t keep);
    std::uint32_t process_hit(std::uint32_t moby, std::uint32_t hit,bool* external=nullptr);
    bool external_context(const weapon_xp_external_hit_v2*, bool require_ring);
    static bool same_key(const ExternalKey&, const weapon_xp_external_hit_v2&);
    int begin_external_scope(std::uint64_t);
    int prepare_local_hit(const weapon_xp_external_hit_v2*, weapon_xp_prepared_hit_v2*);
    int mark_replicated_hit(const weapon_xp_external_hit_v2*);
    int confirm_external_death(const weapon_xp_external_hit_v2*);
    void record_death(std::uint32_t moby, std::int32_t flags, std::int32_t index);
    std::uint32_t load_level(std::int32_t level);
    void pad_report(unsigned pad, unsigned report, int size);
    void update_capacity();
    void restore_capacity();
    void tick();
    std::int32_t draw_hud(std::uint32_t element);
    void relay(std::uint32_t element);
    void hook(const char* name, const char* signature, void* callback, void** original);
    void accept_award(const Award& award);
    int get_stats(int item, weapon_xp_stats_v1* out);
    int award_xp(int item, std::uint32_t amount);
    int get_configuration(weapon_xp_configuration_v3* out);
    int set_configuration(const weapon_xp_configuration_v3* in, std::uint64_t revision);
    int get_rules(int item, weapon_xp_rules_v3* out);
    int set_rules(const weapon_xp_rules_v3* entries, std::uint32_t count, std::uint64_t revision);
    int reload_rules(std::uint64_t revision);
    bool configuration_busy() const;
    int configuration_revision(std::uint64_t expected) const;
    void configuration_changed();
};

}  // namespace weaponxp

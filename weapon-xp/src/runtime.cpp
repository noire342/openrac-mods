// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "enemies.hpp"
#include "rules_file.hpp"

namespace weaponxp {
constexpr std::uint32_t kRam = 0x02000000;
constexpr std::uint32_t kMobyClass = 0xA6;
constexpr std::uint32_t kHeroMoby = 0x2080;
constexpr std::uint32_t kWeaponMoby = 0x1090;
Runtime* Runtime::s_instance = nullptr;

Runtime::Runtime(const openrac_api* api, openrac_mod* mod)
    : m_api(api),
      m_mod(mod),
      m_combat(m_state, m_settings) {
    s_instance = this;
}

Runtime::~Runtime() {
    if (m_ready) {
        weapon_xp_event_v1 event{};
        event.size = sizeof event;
        event.kind = WEAPON_XP_STOPPING;
        event.item_id = -1;
        m_events.emit(event);
        save();
        restore_hits();
        restore_capacity();
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void Runtime::load() {
#if defined(_WIN32)
    char* trace_value = nullptr;
    std::size_t trace_size = 0;
    if (_dupenv_s(&trace_value, &trace_size, "WEAPON_XP_TRACE") == 0) {
        m_trace = trace_value && trace_size > 1;
        std::free(trace_value);
    }
#else
    m_trace = std::getenv("WEAPON_XP_TRACE") != nullptr;
#endif
    const auto* api = m_api;
    auto* mod = m_mod;
    if (std::strcmp(api->game, "rac1") != 0) {
        throw std::runtime_error("weapon-xp requires rac1");
    }
    for (const auto& disc : kDiscs) {
        if (std::strcmp(api->version, disc.version) == 0) {
            m_disc = &disc;
        }
    }
    if (!m_disc) {
        throw std::runtime_error("weapon-xp requires PAL v2.00 or NTSC-U");
    }
    if (!api->memory) {
        throw std::runtime_error("OpenRAC supplied no game memory");
    }
    m_painter = std::make_unique<NativeHud>(api, *m_disc);
    m_morph_model = std::make_unique<NativeMorphModel>(api, *m_disc);
    m_native_damage = std::make_unique<NativeDamage>(api, *m_disc);
    m_upgrades = std::make_unique<NativeUpgrades>(api, *m_disc);
    m_hud = reinterpret_cast<Hud>(api->function(m_disc->weapon_draw));
    m_gauge_hud = reinterpret_cast<Hud>(api->function(m_disc->gauge_draw));
    void* relay_function = api->function(m_disc->relay);
    if (!m_hud || !m_gauge_hud || !relay_function || !api->function_at) {
        throw std::runtime_error("OpenRAC has no compatible weapon HUD callbacks");
    }
    m_hud_bridge = std::make_unique<HudBridge>(
        api,
        *m_disc,
        reinterpret_cast<void*>(m_hud),
        relay_function,
        reinterpret_cast<void*>(m_gauge_hud)
    );
    m_settings.xp_rate = api->setting_float(mod, "xp_rate");
    if (!std::isfinite(m_settings.xp_rate) || m_settings.xp_rate < 0.25 || m_settings.xp_rate > 4) {
        throw std::runtime_error("XP rate must be 0.25 to 4");
    }
    m_settings.scale_damage = api->setting_bool(mod, "scale_damage");
    m_settings.scale_capacity = api->setting_bool(mod, "scale_capacity");
    m_settings.hud = api->setting_bool(mod, "hud");
    m_settings.animations = api->setting_bool(mod, "level_up_animation");
    m_settings.versioned_names = api->setting_bool(mod, "versioned_names");
    m_settings.flip_morph_model = api->setting_bool(mod, "flip_morph_model");
    const auto kill_cap = api->setting_float(mod, "max_kill_xp");
    if (!std::isfinite(kill_cap) || kill_cap < 0 || kill_cap > 100000
        || std::floor(kill_cap) != kill_cap) {
        throw std::runtime_error("max_kill_xp must be an integer 0..100000");
    }
    m_settings.kill_xp_cap = static_cast<unsigned>(kill_cap);
    m_ui.enabled = m_settings.hud;
    m_settings.reduced_motion = m_ui.reduced_motion = api->setting_bool(mod, "reduced_motion");
    m_store = std::make_unique<Store>();
    m_store->open(profile_path(m_disc->version, api->setting_string(mod, "profile")), m_state);
    const std::string rules_name = api->setting_string(mod, "rules_file");
    if (rules_name.empty() || rules_name.size() > 64 || rules_name.find("..") != std::string::npos
        || rules_name.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-"
           ) != std::string::npos) {
        throw std::runtime_error("rules_file must be a simple filename, at most 64 characters");
    }
    m_rules_path = m_store->path().parent_path() / rules_name;
    if (!read_rules_file(m_rules_path, m_state.rules, m_rules_error, true)) {
        throw std::runtime_error(m_rules_error);
    }
    if (m_state.migrated) {
        m_dirty = true;
        save();
        log(OPENRAC_LOG_INFO,
            "XP profile migrated to format 4; existing XP retained; original backup "
            "preserved");
    }
    hook(
        "rac1.moby.bolt.update",
        "void(u32)",
        reinterpret_cast<void*>(+[](std::uint32_t e) { s_instance->relay(e); }),
        reinterpret_cast<void**>(&m_relay_original)
    );
    hook(
        "rac1.combat.get_hit",
        "u32(u32,i32,i32)",
        reinterpret_cast<void*>(+[](std::uint32_t m, std::int32_t mask, std::int32_t keep) {
            return s_instance->get_hit(m, mask, keep);
        }),
        reinterpret_cast<void**>(&m_get_hit)
    );
    hook(
        "rac1.moby.record_death",
        "void(u32,i32,i32)",
        reinterpret_cast<void*>(+[](std::uint32_t m, std::int32_t f, std::int32_t i) {
            return s_instance->record_death(m, f, i);
        }),
        reinterpret_cast<void**>(&m_death)
    );
    hook(
        "rac1.level.load_gameplay",
        "u32(i32)",
        reinterpret_cast<void*>(+[](std::int32_t l) { return s_instance->load_level(l); }),
        reinterpret_cast<void**>(&m_load_level)
    );
    hook(
        "rac1.moby.morphoray.update",
        "void(u32)",
        reinterpret_cast<void*>(+[](unsigned moby) { s_instance->update_morph(moby); }),
        reinterpret_cast<void**>(&m_morph_update)
    );
    if (api->on_frame(mod, +[](void*) { s_instance->tick(); }, nullptr) != 0) {
        throw std::runtime_error("OpenRAC refused weapon-xp callbacks");
    }
    hook(
        "rac1.pad.process_report",
        "void(u32,u32,i32)",
        reinterpret_cast<void*>(+[](unsigned pad, unsigned report, int size) {
            s_instance->pad_report(pad, report, size);
        }),
        reinterpret_cast<void**>(&m_pad_report)
    );
    log(OPENRAC_LOG_INFO,
        "weapon XP loaded in the original weapon HUD; profile " + m_store->path().u8string());
    m_ready = true;
}

bool Runtime::valid(std::uint32_t address, std::uint32_t size) {
    return address >= 0x10000 && address <= kRam && size <= kRam - address;
}

std::uint32_t Runtime::resident(std::uint32_t address) {
    return m_api->relocate_data(address);
}

std::uint32_t Runtime::overlay(std::uint32_t address) {
    return m_api->relocate_level_data(0, address);
}

std::uint32_t Runtime::hero() {
    return Runtime::resident(m_disc->hero);
}

bool Runtime::playing() {
    return m_api->loaded_level() >= 0 && read<std::int32_t>(overlay(m_disc->mode)) == 0
           && read<std::uint8_t>(hero() + 0x20A4) == 0;
}

void Runtime::log(int severity, const std::string& message) {
    m_api->log(m_mod, severity, message.c_str());
}

void Runtime::save() noexcept {
    if (!m_dirty || !m_store) {
        return;
    }
    try {
        m_store->save(m_state);
        m_dirty = false;
        m_save_warning = false;
        m_saved_tick = m_tick;
    } catch (const std::exception& error) {
        if (!m_save_warning) {
            log(OPENRAC_LOG_ERROR, error.what());
        }
        m_save_warning = true;
        m_saved_tick = m_tick;
    }
}

// Undo only our unchanged multiplier. A reaction that consumed/changed a hit keeps its changes.
// This prevents a pending hit that survives several ticks from being multiplied repeatedly.
void Runtime::restore_hits() {
    if (m_native_damage) {
        m_native_damage->restore();
    }
    for (auto& edit : m_edits) {
        if (edit.hit && edit.raw != edit.scaled
            && read<std::uint32_t>(edit.hit + 0x34) == edit.owner
            && read<float>(edit.hit + 0x2C) == edit.scaled) {
            write(edit.hit + 0x2C, edit.raw);
        }
        edit = {};
    }
}

void Runtime::reset_level() {
    if (m_morph_model) {
        m_morph_model->reset();
    }
    if (m_upgrades) {
        m_upgrades->reset();
    }
    if (m_hud_bridge) {
        m_hud_bridge->detach();
    }
    m_edits = {};
    m_combat.reset();
    m_external_replicated = {};
    m_sources = {};
    m_source_cursor = 0;
    m_table = 0;
    m_base_ammo = {};
    m_applied_ammo = {};
}

// Snapshot the equipped weapon's identity while it is equipped. Never assign a delayed hit
// to whatever weapon happens to be equipped when it lands.
void Runtime::remember_source() {
    const int item = read<std::int32_t>(resident(m_disc->item_in_hand));
    const auto moby = read<std::uint32_t>(hero() + kWeaponMoby);
    if (weapon_index(item) < 0 || !valid(moby, 0x100)) {
        return;
    }
    const auto oclass = read<std::int16_t>(moby + kMobyClass);
    for (auto& source : m_sources) {
        if (source.moby == moby) {
            source = {moby, oclass, item};
            return;
        }
    }
    m_sources[m_source_cursor++ % m_sources.size()] = {moby, oclass, item};
}

int Runtime::source_item(std::uint32_t hit) {
    const auto attacker = read<std::uint32_t>(hit + 0x20);
    if (valid(attacker, 0x100)) {
        const auto oclass = read<std::int16_t>(attacker + kMobyClass);
        const int known = weapon_from_class(oclass);
        if (known >= 0) {
            return known;
        }
        for (const auto& source : m_sources) {
            if (source.moby == attacker && source.oclass == oclass) {
                return source.item;
            }
        }

        // Projectile bodies preserve their origin in private vars, including after a switch.
        // Offsets are shared by PAL/NTSC: spawn_bomb, Blaster shot, Mine and RYNO missile.
        struct OwnerLink {
            int projectile;
            unsigned offset;
            int model, item;
            bool hero_owned;
        };

        static constexpr OwnerLink links[] =
            {{121, 0x50, 192, 10, false},
             {305, 0x20, 168, 15, true},
             {74, 0x30, 190, 17, false},
             {457, 0x20, 454, 23, false}};
        for (const auto& link : links) {
            if (oclass != link.projectile) {
                continue;
            }
            const auto vars = read<std::uint32_t>(attacker + 0x78);
            if (valid(vars, 0x54)) {
                const auto owner = read<std::uint32_t>(vars + link.offset);
                if (valid(owner, 0x100)
                    && (read<std::int16_t>(owner + kMobyClass) == link.model
                        || (link.hero_owned && owner == read<std::uint32_t>(hero() + kHeroMoby)))) {
                    return link.item;
                }
            }
        }
    }
    // A directional descriptor stamps its originating class at +0x2A. This survives a generic
    // projectile and a weapon switch. Plain PostHitMessage does not initialize these bytes.
    if (read<float>(hit + 0x1C) == 5627.9248046875f) {
        return weapon_from_class(read<std::uint16_t>(hit + 0x2A));
    }
    return -1;
}

std::uint32_t Runtime::get_hit(std::uint32_t moby, std::int32_t mask, std::int32_t keep) {
    const auto hit = m_get_hit(moby, mask, keep);
    return process_hit(moby, hit);
}

std::uint32_t Runtime::process_hit(std::uint32_t moby, std::uint32_t hit) {
    try {
        m_native_damage->restore();
        if (m_trace && hit && m_combat_traces++ < 64) {
            std::ostringstream trace;
            trace << "trace hit target=" << std::hex << moby << " class=" << std::dec
                  << read<std::int16_t>(moby + kMobyClass) << " hit=" << std::hex << hit
                  << " ring=" << overlay(m_disc->hit_ring)
                  << " attacker=" << read<unsigned>(hit + 0x20) << " item=" << std::dec
                  << source_item(hit) << " attacker-class="
                  << read<std::int16_t>(read<unsigned>(hit + 0x20) + kMobyClass)
                  << " stamp=" << read<std::uint16_t>(hit + 0x2A)
                  << " tag=" << read<float>(hit + 0x1C) << " damage=" << read<float>(hit + 0x2C)
                  << " playing=" << playing();
            log(OPENRAC_LOG_INFO, trace.str());
        }
        if (!hit || !playing() || !valid(moby, 0x100)) {
            return hit;
        }
        const auto ring = overlay(m_disc->hit_ring);
        if (hit < ring || (hit - ring) % 0x40 != 0 || (hit - ring) / 0x40 >= 64 || !valid(hit, 0x40)
            || read<std::uint32_t>(hit + 0x34) != moby) {
            return hit;
        }
        const auto& replicated = m_external_replicated[(hit - ring) / 0x40];
        if (replicated.key.session == m_external_scope && m_external_scope != 0
            && replicated.moby == moby && replicated.hit == hit
            && std::memcmp(replicated.payload.data(), m_api->memory + hit, 64) == 0) {
            m_combat.forget(moby);
            if (m_tick - replicated.at > 8) {
                if (read<unsigned char>(moby + 0xA4) == (hit - ring) / 64) {
                    write<unsigned char>(moby + 0xA4, 0xFF);
                }
                return 0;
            }
            return hit;
        }
        if (moby == read<unsigned>(hero() + kHeroMoby)
            || enemy_reward(read<short>(moby + kMobyClass)) == 0) {
            return hit;
        }
        auto& edit = m_edits[(hit - ring) / 0x40];
        const float damage = read<float>(hit + 0x2C);
        const int item = source_item(hit);
        if (m_settings.scale_damage && item == 15) {
            m_native_damage->prepare(m_state.rule(item).damage[m_state.weapon_level(item) - 1]);
        }
        if (edit.hit == hit && edit.owner == moby && edit.scaled == damage) {
            return hit;
        }
        if (!std::isfinite(damage) || damage <= 0 || damage > 1000000) {
            return hit;
        }
        if (weapon_index(item) < 0) {
            m_combat.forget(moby);
            return hit;
        }
        const float scaled = m_combat.hit(moby, item, damage, m_tick);
        edit = {hit, moby, damage, scaled};
        if (scaled != damage) {
            write(hit + 0x2C, scaled);
        }
        weapon_xp_event_v1 event{};
        event.size = sizeof event;
        event.kind = WEAPON_XP_HIT;
        event.item_id = item;
        event.new_level = m_state.weapon_level(item);
        event.xp = m_state.experience(item);
        event.target_moby = moby;
        event.damage = scaled;
        event.flags = 0;
        for (int axis = 0; axis < 3; ++axis) {
            event.target_position[axis] = read<float>(moby + 0x10 + 4 * axis);
        }
        m_events.emit(event);
    } catch (const std::exception& error) {
        log(OPENRAC_LOG_ERROR, error.what());
    }
    return hit;
}

void Runtime::record_death(std::uint32_t moby, std::int32_t flags, std::int32_t index) {
    if (playing() && valid(moby, 0x100)) {
        try {
            const int bolts = NativeRewards(m_api, *m_disc).bolts(moby);
            const auto award = m_combat.death(moby, bolts, m_tick);
            if (m_trace) {
                std::ostringstream trace;
                trace << "trace death class=" << read<std::int16_t>(moby + kMobyClass)
                      << " native-bolts=" << read<short>(moby + 0xB4)
                      << " budget=" << read<short>(moby + 0xB6) << " nominal-bolts=" << bolts
                      << " changed=" << award.changed << " item=" << award.item
                      << " xp=" << m_state.experience(award.item);
                log(OPENRAC_LOG_INFO, trace.str());
            }
            accept_award(award);
        } catch (const std::exception& error) {
            log(OPENRAC_LOG_ERROR, error.what());
        }
    }
    m_death(moby, flags, index);
}

std::uint32_t Runtime::load_level(std::int32_t level) {
    save();
    restore_hits();
    reset_level();
    const auto result = m_load_level(level);
    weapon_xp_event_v1 event{};
    event.size = sizeof event;
    event.kind = WEAPON_XP_LEVEL_LOADED;
    event.item_id = -1;
    m_events.emit(event);
    return result;
}

void Runtime::update_capacity() {
    if (!m_settings.scale_capacity) {
        return;
    }
    const auto table = overlay(m_disc->item_table);
    if (!valid(table, kItems * 0x18)) {
        return;
    }
    if (m_table != table) {
        m_table = table;
        for (std::size_t i = 0; i < kWeapons.size(); ++i) {
            m_base_ammo[i] = read<std::uint16_t>(table + kWeapons[i].item * 0x18 + 0xE);
        }
    }
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        const auto value =
            static_cast<std::uint16_t>(m_state.weapon_capacity(kWeapons[i].item, m_base_ammo[i]));
        write(table + kWeapons[i].item * 0x18 + 0xE, value);
        m_applied_ammo[i] = value;
    }
    const int held = read<std::int32_t>(resident(m_disc->item_in_hand));
    const int index = weapon_index(held);
    if (index >= 0) {
        m_hud_bridge->refresh_capacity(held, m_state.weapon_capacity(held, m_base_ammo[index]));
    }
}

void Runtime::restore_capacity() {
    if (!m_settings.scale_capacity || !m_table || m_api->loaded_level() != m_last_level
        || !valid(m_table, kItems * 0x18)) {
        return;
    }
    for (std::size_t i = 0; i < kWeapons.size(); ++i) {
        const auto at = m_table + kWeapons[i].item * 0x18 + 0xE;
        const auto expected = m_applied_ammo[i];
        if (read<std::uint16_t>(at) == expected) {
            write(at, m_base_ammo[i]);
        }
    }
    const int held = read<int>(resident(m_disc->item_in_hand));
    const int index = weapon_index(held);
    if (index >= 0) {
        m_hud_bridge->refresh_capacity(held, m_base_ammo[index]);
    }
}

void Runtime::tick() {
    try {
        ++m_tick;
        const int loaded = m_api->loaded_level();
        if (loaded != m_last_level) {
            save();
            reset_level();
            m_last_level = loaded;
        } else {
            restore_hits();
        }
        if (loaded < 0) {
            return;
        }
        if (m_trace && m_tick % 120 == 0) {
            const auto slot = resident(m_disc->hud_bank);
            const auto callback = read<std::uint32_t>(slot + 0x18);
            std::ostringstream trace;
            trace << "trace tick=" << m_tick << " level=" << loaded << " playing=" << playing()
                  << " mode=" << read<std::int32_t>(overlay(m_disc->mode))
                  << " hero-state=" << int(read<std::uint8_t>(hero() + 0x20A4))
                  << " hud=" << m_ui.enabled
                  << " held=" << read<int>(resident(m_disc->item_in_hand)) << std::hex
                  << " bank=" << slot << " pending=" << read<unsigned>(slot + 0x68)
                  << " icon=" << read<unsigned>(slot + 0x20) << " cb=" << callback
                  << " actual=" << m_api->function_at(callback)
                  << " expected=" << reinterpret_cast<void*>(m_hud)
                  << " timer=" << read<int>(overlay(m_disc->hud_parameters));
            if (read<int>(resident(m_disc->item_in_hand)) == 9) {
                const auto gauge_slot = slot + 4 * 0x90;
                trace << " gauge-cb=" << read<unsigned>(gauge_slot + 0x18)
                      << " gauge-owned=" << m_hud_bridge->owns(gauge_slot)
                      << " gauge-pending=" << read<unsigned>(gauge_slot + 0x68);
            }
            log(OPENRAC_LOG_INFO, trace.str());
        }
        if (m_dirty && m_tick - m_saved_tick >= 300) {
            save();
        }
        m_upgrades->tick(m_state, playing(), m_settings.versioned_names);
        if (!playing() || m_upgrades->active()) {
            return;
        }
        if (m_ui.enabled) {
            m_hud_bridge->attach(read<int>(resident(m_disc->item_in_hand)));
        }
        remember_source();
        update_capacity();
        const float dt = read<float>(resident(m_disc->tick_seconds));
        const float seconds = std::isfinite(dt) && dt > 0 ? std::min(dt, 0.1f) : 1.0f / 60.0f;
        m_ui.tick(seconds, read<std::int32_t>(resident(m_disc->item_in_hand)), m_state);
        m_combat.expire(m_tick);
    } catch (const std::exception& error) {
        log(OPENRAC_LOG_ERROR, error.what());
    }
}

void Runtime::pad_report(unsigned pad, unsigned report, int size) {
    if (m_upgrades->active() && report && size >= 6 && size <= 32
        && std::uint64_t(report) + unsigned(size) <= 0x100000000ull) {
        std::array<unsigned char, 32> saved{};
        std::memcpy(saved.data(), m_api->memory + report, size);
        m_upgrades->filter_report(m_api->memory + report, size);
        m_pad_report(pad, report, size);
        std::memcpy(m_api->memory + report, saved.data(), size);
        return;
    }
    m_pad_report(pad, report, size);
}

std::int32_t Runtime::draw_hud(std::uint32_t element) {
    const bool gauge = read<int>(element + 0x20) == 0x753F;
    const auto first_packet = gauge ? m_painter->packet_cursor() : 0;
    const auto result = (gauge ? m_gauge_hud : m_hud)(element);
    if (!playing() || !valid(element, 0x90)) {
        return result;
    }
    try {
        if (gauge) {
            const auto bounds = m_painter->gauge_bounds(element);
            if (m_trace && m_draw_traces++ < 3) {
                std::ostringstream s;
                s << "trace gauge x=" << bounds.x << " y=" << bounds.y << " w=" << bounds.width
                  << " h=" << bounds.height << std::hex
                  << " palette=" << read<unsigned>(overlay(m_disc->hud_parameters + 16))
                  << " general="
                  << read<unsigned>(
                         resident(std::strcmp(m_disc->version, "pal") == 0 ? 0x161000 : 0x160F00)
                     )
                  << " hud-stream="
                  << read<unsigned>(
                         overlay(std::strcmp(m_disc->version, "pal") == 0 ? 0x161240 : 0x1611C0)
                     )
                  << " general-at="
                  << resident(std::strcmp(m_disc->version, "pal") == 0 ? 0x161000 : 0x160F00)
                  << " hud-at="
                  << overlay(std::strcmp(m_disc->version, "pal") == 0 ? 0x161240 : 0x1611C0);
                log(OPENRAC_LOG_INFO, s.str());
            }
            if (bounds.width >= 42 && bounds.height >= 40) {
                m_painter->reflow_gauge(first_packet, bounds);
                const auto color = read<std::uint32_t>(overlay(m_disc->hud_parameters + 16));
                m_painter->draw_gauge(m_ui.draw_vertical(
                    9, m_state, bounds.x, bounds.y, bounds.width, bounds.height, color
                ));
            }
            return result;
        }
        const int item = read<std::int32_t>(element + 0x20) - 60000;
        const int timer = read<std::int32_t>(overlay(m_disc->hud_parameters));
        if (timer <= 0) {
            return result;
        }
        const float visibility =
            std::clamp(read<std::uint8_t>(element + 0x70) / static_cast<float>(timer), 0.0f, 1.0f);
        const int max_ammo = read<std::int32_t>(element + 8);
        const int width =
            read<std::int32_t>(overlay(m_disc->hud_parameters + (max_ammo < 100 ? 8 : 12)));
        const int x = read<std::int32_t>(element + 0x50);
        const int y = read<std::int32_t>(resident(m_disc->video_mode)) ? 10 : 18;
        const int offset = read<std::int32_t>(overlay(m_disc->hud_parameters + 32));
        const auto color = read<std::uint32_t>(overlay(m_disc->hud_parameters + 16));
        const WeaponTile tile{x, y, x + offset + width, static_cast<int>(128 * visibility), color};
        const bool trace_draw = m_trace && m_draw_traces++ < 3;
        if (trace_draw) {
            std::ostringstream trace;
            trace << "trace draw item=" << item << " x=" << x << " y=" << y << " width=" << width
                  << " offset=" << offset << " alpha=" << tile.alpha << " color=" << std::hex
                  << color;
            log(OPENRAC_LOG_INFO, trace.str());
        }
        const auto draws = m_ui.draw(item, m_state, tile);
        for (const auto& draw : draws) {
            m_painter->draw(draw);
        }
    } catch (const std::exception& error) {
        log(OPENRAC_LOG_ERROR, error.what());
    }
    return result;
}

void Runtime::update_morph(unsigned moby) {
    m_morph_model->reset();
    m_morph_update(moby);
    m_morph_model->after(moby, m_settings.flip_morph_model && playing() && !m_upgrades->active());
}

void Runtime::relay(std::uint32_t element) {
    if (m_hud_bridge && m_hud_bridge->owns(element)) {
        draw_hud(element);
    } else {
        m_relay_original(element);
    }
}

void Runtime::hook(const char* name, const char* signature, void* callback, void** original) {
    const int result = m_api->hook(m_mod, name, signature, callback, original);
    if (result != OPENRAC_HOOK_OK) {
        const char* reason = result == OPENRAC_HOOK_SIGNATURE ? "signature mismatch"
                             : result == OPENRAC_HOOK_UNKNOWN ? "hook unavailable"
                                                              : "hook registration refused";
        throw std::runtime_error(
            std::string(name) + ": " + reason + "; requested " + signature + "; result "
            + std::to_string(result) + "; disc " + m_disc->version
        );
    }
}

}  // namespace weaponxp

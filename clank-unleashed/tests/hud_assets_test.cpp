// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud_assets.hpp"
#include "native_hud.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
using namespace clank;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)
std::vector<std::uint8_t> ram(0x2000000);
template <class T> void put(unsigned at, T value) {
    std::memcpy(ram.data() + at, &value, sizeof(value));
}
template <class T> T get(unsigned at) {
    T value{};
    std::memcpy(&value, ram.data() + at, sizeof(value));
    return value;
}
std::array<std::uint32_t, 5> voice_args{};
unsigned voice_calls{}, show_calls{}, removals{}, draw_calls{}, energy_draws{}, water_draws{};
unsigned bank_loads{}, bank_unloads{}, killed_voices{};
bool services_alive = true;
std::uint32_t charge_pointer{};
NativeHud *drawing_hud{};
int allocate_voice(std::uint32_t def, int flags, std::uint32_t actor, int position, int volume) {
    CHECK(services_alive);
    voice_args = {def, static_cast<unsigned>(flags), actor, static_cast<unsigned>(position),
                  static_cast<unsigned>(volume)};
    ++voice_calls;
    put(profile::voice_pool + 8 * 0x70 + 8, def);
    return 7;
}
std::uint32_t load_bank(int sector, int offset) {
    CHECK(services_alive);
    CHECK(sector == 10 && offset == 20);
    ++bank_loads;
    return 4;
}
void unload_bank(int bank) {
    CHECK(services_alive);
    CHECK(bank == 4);
    ++bank_unloads;
}
void kill_voice(int slot) {
    CHECK(services_alive);
    CHECK(slot == 7);
    ++killed_voices;
}
int warning(int id, int flags) {
    CHECK(id == 11);
    return drawing_hud->play_warning(flags);
}
int show_meter(int slot, int texture, std::uint32_t, std::uint32_t, std::uint32_t,
               std::uint32_t pointer, int max) {
    CHECK(services_alive);
    CHECK(slot == 4 && texture == EnergyArtwork::meter_icon && max == 10000 && pointer >= 0x10000);
    ++show_calls;
    charge_pointer = pointer;
    return 42;
}
void remove_meter(int id) {
    CHECK(services_alive);
    CHECK(id == 42);
    ++removals;
}
int scale_ticks(int n) { return n; }
void unused_native() {}
int original_draw(std::uint32_t record) {
    ++draw_calls;
    int id = get<int>(record), frame = 2;
    if (drawing_hud->remap_air_icon(id, frame)) {
        ++energy_draws;
        CHECK(id == EnergyArtwork::icon && frame >= 0 && frame < 2);
        int casing = EnergyArtwork::meter_icon, casing_frame = 0;
        CHECK(drawing_hud->remap_air_icon(casing, casing_frame) && casing == 0x753F);
    } else {
        ++water_draws;
        CHECK(id == 0x753F && frame == 2);
    }
    return 42;
}
int main() {
    openrac_api api{};
    api.memory = ram.data();
    api.relocate_data = [](std::uint32_t at) { return at == 0x0019A4E8 ? 0x10000u : at; };
    api.relocate_level_data = [](int, std::uint32_t at) { return at; };
    put(0x10018, 0x20000u);
    const std::array<unsigned, 4> original{0x21000, 0x22000, 0x23000, 0x24000};
    for (unsigned i = 0; i < 4; ++i)
        put(0x1001C + i * 4, original[i]);
    put(0x20024, 2u);
    put(0x20044, 3u);
    put(0x21000, std::uint16_t(0x753F));
    put(0x21002, std::uint16_t(3));
    put(0x21004, std::uint16_t(1));
    put(0x21008, std::uint16_t(0xFFFF));
    for (unsigned i = 0; i < 4; ++i)
        put(0x22000 + i * 4, 0xAB000000u + i);
    std::fill_n(ram.data() + 0x23000, 24, 0x55);
    std::fill_n(ram.data() + 0x24000, 16, 0x66);
    GadgebotIcon icon;
    icon.palette.assign(1024, 0x42);
    for (unsigned i = 0; i < 8; ++i)
        icon.images[i].assign(1024, static_cast<std::uint8_t>(i + 1));
    HudAssets assets(api, icon);
    for (int repeat = 0; repeat < 10; ++repeat) {
        auto end = assets.reserve(0x100000);
        CHECK(end > 0x100000 && end < 0x110000 && assets.reserve(0x100000) == end);
        auto defs = get<unsigned>(0x1001C), refs = get<unsigned>(0x10020),
             imgs = get<unsigned>(0x10024), pals = get<unsigned>(0x10028);
        CHECK(get<std::uint16_t>(defs) == 0x753F && get<std::uint16_t>(defs + 8) == 0x7553);
        CHECK(get<std::uint16_t>(defs + 16) == EnergyArtwork::icon &&
              get<std::uint16_t>(defs + 18) == 2);
        CHECK(get<std::uint16_t>(defs + 24) == EnergyArtwork::meter_icon &&
              get<std::uint16_t>(defs + 26) == 3);
        CHECK(get<std::uint16_t>(defs + 32) == 0xFFFF);
        auto meter_first = get<std::uint16_t>(defs + 28);
        CHECK(get<unsigned>(refs + meter_first * 4) == get<unsigned>(0x22004));
        CHECK(get<unsigned>(refs + (meter_first + 1) * 4) == get<unsigned>(0x22008));
        CHECK(get<unsigned>(refs + (meter_first + 2) * 4) == get<unsigned>(refs + 12 * 4));
        CHECK(std::equal(ram.data() + imgs, ram.data() + imgs + 24, ram.data() + 0x23000));
        CHECK(std::equal(ram.data() + pals, ram.data() + pals + 16, ram.data() + 0x24000));
        CHECK(get<int>(assets.charge_address()) == 10000);
        assets.charge(1234);
        CHECK(get<int>(assets.charge_address()) == 1234);
        for (unsigned i = 0; i < 10; ++i) {
            CHECK(get<std::uint8_t>(imgs + (3 + i) * 8 + 6) == (i < 8 ? 5 : 6));
            if (i < 8)
                CHECK(get<std::uint8_t>(get<unsigned>(imgs + (3 + i) * 8)) == i + 1);
            put(imgs + (3 + i) * 8 + 4, std::uint16_t(999));
        }
        put(pals + 20, std::uint16_t(999));
        put(pals + 28, std::uint16_t(999));
        assets.before_draw();
        CHECK(get<std::uint16_t>(pals + 20) == 0 && get<std::uint16_t>(pals + 28) == 0);
        for (unsigned i = 0; i < 10; ++i)
            CHECK(get<std::uint16_t>(imgs + (3 + i) * 8 + 4) == 0);
        assets.remove();
        CHECK(!assets.charge_address());
        for (unsigned i = 0; i < 4; ++i)
            CHECK(get<unsigned>(0x1001C + i * 4) == original[i]);
    }
    CHECK(assets.reserve(0x1FFB000) <= 0x2000000);
    assets.remove();
    put(0x20044, 0xFFFFFFFFu);
    bool rejected = false;
    try {
        assets.reserve(0x100000);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    CHECK(rejected && get<unsigned>(0x1001C) == original[0]);
    put(0x20044, 3u);
    // A planet with its own wheel keeps that definition and still gets the meter.
    put(0x21008, std::uint16_t(0x7553));
    put(0x2100A, std::uint16_t(8));
    put(0x2100C, std::uint16_t(4));
    put(0x21010, std::uint16_t(0xFFFF));
    assets.reserve(0x100000);
    CHECK(get<std::uint16_t>(get<unsigned>(0x1001C) + 8) == 0x7553);
    assets.remove();
    put(0x21010, EnergyArtwork::meter_icon);
    rejected = false;
    try {
        assets.reserve(0x100000);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    CHECK(rejected && get<unsigned>(0x1001C) == original[0]);
    put(0x21008, std::uint16_t(0xFFFF));
    const auto art = make_energy_artwork();
    CHECK(std::count_if(art.images[0].begin(), art.images[0].end(),
                        [](auto v) { return v > 120; }) > 30);
    for (unsigned i = 0; i < art.images[0].size(); ++i)
        CHECK(art.images[1][i] == (art.images[0][i] ? art.images[0][i] + 128 : 0));
    api.function = [](const char *name) -> void * {
        const std::string key(name);
        if (key == profile::hud_show)
            return reinterpret_cast<void *>(show_meter);
        if (key == profile::hud_remove)
            return reinterpret_cast<void *>(remove_meter);
        if (key == profile::scale_ticks)
            return reinterpret_cast<void *>(scale_ticks);
        if (key == "func_L00_00236F38")
            return reinterpret_cast<void *>(original_draw);
        if (key == profile::allocate_voice)
            return reinterpret_cast<void *>(allocate_voice);
        if (key == profile::alarm_bank_load)
            return reinterpret_cast<void *>(load_bank);
        if (key == profile::alarm_bank_unload)
            return reinterpret_cast<void *>(unload_bank);
        if (key == profile::alarm_voice_kill)
            return reinterpret_cast<void *>(kill_voice);
        if (key == profile::oxygen_warning)
            return reinterpret_cast<void *>(warning);
        return reinterpret_cast<void *>(unused_native);
    };
    api.log = [](openrac_mod *, int, const char *) {};
    api.play_status = [](openrac_play_status *s) {
        s->loads = 3;
        return 0;
    };
    put(profile::hero + profile::air_offset, 7654);
    put(profile::hero + 0xA88, 0x300000u);
    put(profile::hero + 0x2080, 0x310000u);
    put(0x300024, 0x320000u);
    put(0x320028, 0x330000u);
    put(0x32000D, std::uint8_t(12));
    put(0x3000A6, std::uint16_t(0));
    put(0x3100A6, std::uint16_t(87));
    AlarmSource source;
    source.sector = 10;
    source.offset = 20;
    source.sound = 31;
    NativeHud hud(api, nullptr, icon, source);
    drawing_hud = &hud;
    hud.reserve_assets(0x100000);
    hud.air({true, false, 0.5f});
    CHECK(show_calls == 1 && charge_pointer >= 0x100000 && get<int>(charge_pointer) == 5000);
    CHECK(get<int>(profile::hero + profile::air_offset) == 7654);
    hud.air({true, false, 0.4f});
    CHECK(show_calls == 1 && get<int>(charge_pointer) == 4000);
    CHECK(hud.play_warning(0) == 7 && voice_calls == 1);
    CHECK(voice_args[0] == charge_pointer + 16 && voice_args[1] == 0x11 && voice_args[2] == 0 &&
          voice_args[4] == 0x400);
    CHECK(get<std::int16_t>(voice_args[0] + 0x1A) == 31 &&
          get<unsigned>(voice_args[0] + 0x1C) == 4);
    CHECK(get<unsigned>(0x300024) == 0x320000);
    constexpr unsigned record = 0x400000;
    put(record, static_cast<int>(EnergyArtwork::meter_icon));
    put(record + 0x64, 42);
    put(record + 0x74, 5000);
    CHECK(hud.air_draw(record) == 42 && energy_draws == 1 && water_draws == 0 &&
          hud.pending_memory());
    put(record + 0x74, 2000);
    put(profile::frame_scale_sq, 1.0f);
    put(profile::rendered_frame_count, 61); // Already mid-flash when entering critical.
    CHECK(hud.air_draw(record) == 42 && voice_calls == 2 && bank_loads == 1);
    CHECK(hud.air_draw(record) == 42 && voice_calls == 2);
    hud.air({});
    CHECK(removals == 1 && get<int>(charge_pointer) == 10000);
    // Real native removal changes the serial immediately, before slide-out ends.
    put(record + 0x64, 99);
    put(record + 0x20, 0xFFFF);
    CHECK(hud.air_draw(record) == 42 && energy_draws == 4 && water_draws == 0);
    CHECK(hud.play_warning(0) == -1 && voice_calls == 2);
    put(record, 0x753F);
    CHECK(hud.air_draw(record) == 42 && water_draws == 1 && !hud.pending_memory());
    CHECK(get<int>(profile::hero + profile::air_offset) == 7654);
    hud.before_level_change();
    CHECK(!hud.pending_memory() && bank_unloads == 1 && killed_voices == 1);
    const auto old_removals = removals;
    {
        NativeHud closing(api, nullptr, icon, source);
        closing.reserve_assets(0x100000);
        closing.air({true, false, 0.3f});
        CHECK(closing.play_warning(0) == 7);
        closing.abandon();
        services_alive = false;
    }
    CHECK(removals == old_removals && bank_unloads == 1 && killed_voices == 1);
}

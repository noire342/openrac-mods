// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud_assets.hpp"
#include <algorithm>
#include <stdexcept>

namespace clank {
std::uint32_t HudAssets::reserve(std::uint32_t cursor) {
    const auto arena = m_api.relocate_data(0x0019A4E8);
    if (m_arena == arena && m_installed[0] && read<std::uint32_t>(arena + 0x1C) == m_installed[0])
        return std::max(cursor, m_end);
    m_arena = 0;
    m_original = m_installed = {};
    const auto header = read<std::uint32_t>(arena + 0x18);
    for (unsigned i = 0; i < 4; ++i)
        m_original[i] = read<std::uint32_t>(arena + 0x1C + i * 4);
    const auto defs = m_original[0];
    unsigned rows = 0, frames = 0;
    bool has_gadgebot = false;
    std::uint32_t oxygen_row = 0;
    while (rows < 2048) {
        const auto at = defs + rows * 8;
        if (!range(at, 8))
            throw std::runtime_error("Invalid native HUD definitions");
        const auto id = read<std::uint16_t>(at);
        if (id == EnergyArtwork::icon || id == EnergyArtwork::meter_icon)
            throw std::runtime_error("The private Clank energy icon is already in use");
        if (id == 0x753F && read<std::uint16_t>(at + 2) >= 3)
            oxygen_row = at;
        has_gadgebot |= id == 0x7553;
        if (id == 0xFFFF)
            break;
        frames =
            std::max(frames, unsigned(read<std::uint16_t>(at + 2)) + read<std::uint16_t>(at + 4));
        ++rows;
    }
    const auto palettes = read<std::uint32_t>(header + 0x24);
    const auto images = read<std::uint32_t>(header + 0x44);
    const unsigned extra_icons = has_gadgebot ? 2 : 3;
    const unsigned extra_palettes = has_gadgebot ? 1 : 2;
    const unsigned extra_frames = EnergyArtwork::frames + (has_gadgebot ? 0 : 8);
    if (rows >= 2048 - extra_icons || !rows || !oxygen_row || frames > 16381 - extra_frames ||
        palettes > 4096 - extra_icons || images > 16384 - extra_frames ||
        !range(m_original[1], frames * 4) || !range(m_original[3], palettes * 8) ||
        !range(m_original[2], images * 8) || m_icon.palette.size() != 1024 ||
        !std::all_of(m_icon.images.begin(), m_icon.images.end(),
                     [](const auto &v) { return v.size() == 1024; }))
        throw std::runtime_error("Unsupported native HUD lookup tables or Clank icons");
    std::vector<std::uint8_t> data;
    const auto start = (cursor + 63u) & ~63u;
    auto allocate = [&](std::uint32_t count) {
        data.resize((data.size() + 15u) & ~15u);
        const auto offset = static_cast<std::uint32_t>(data.size());
        data.resize(data.size() + count, 0);
        return offset;
    };
    auto put = [&](std::uint32_t at, auto value) {
        std::memcpy(data.data() + at, &value, sizeof(value));
    };
    auto copy = [&](std::uint32_t at, std::uint32_t source, std::uint32_t count) {
        std::memcpy(data.data() + at, m_api.memory + source, count);
    };
    const auto def_at = allocate((rows + extra_icons + 1) * 8);
    const auto refs_at = allocate((frames + extra_frames + 3) * 4);
    const auto pal_at = allocate((palettes + extra_palettes) * 8);
    const auto img_at = allocate((images + extra_frames) * 8);
    copy(def_at, defs, rows * 8);
    copy(refs_at, m_original[1], frames * 4);
    copy(pal_at, m_original[3], palettes * 8);
    copy(img_at, m_original[2], images * 8);
    unsigned added_icons = 0, added_frames = 0;
    const auto append = [&](std::uint16_t id, const auto &palette, const auto &icon_images,
                            std::uint8_t log2_side) {
        const auto row = def_at + (rows + added_icons) * 8;
        put(row, id);
        put(row + 2, static_cast<std::uint16_t>(icon_images.size()));
        put(row + 4, static_cast<std::uint16_t>(frames + added_frames));
        auto pixels = allocate(1024);
        std::memcpy(data.data() + pixels, palette.data(), 1024);
        put(pal_at + (palettes + added_icons) * 8, start + pixels);
        for (const auto &image : icon_images) {
            pixels = allocate(static_cast<std::uint32_t>(image.size()));
            std::memcpy(data.data() + pixels, image.data(), image.size());
            put(refs_at + (frames + added_frames) * 4,
                static_cast<std::uint16_t>(palettes + added_icons));
            put(refs_at + (frames + added_frames) * 4 + 2,
                static_cast<std::uint16_t>(images + added_frames));
            put(img_at + (images + added_frames) * 8, start + pixels);
            data[img_at + (images + added_frames) * 8 + 6] = log2_side;
            data[img_at + (images + added_frames) * 8 + 7] = log2_side;
            ++added_frames;
        }
        ++added_icons;
    };
    if (!has_gadgebot)
        append(0x7553, m_icon.palette, m_icon.images, 5);
    append(EnergyArtwork::icon, m_energy.palette, m_energy.images, EnergyArtwork::log2_side);
    // Alias the original casing. The third frame is already electrical even
    // without a hook, including the native slide-out after the serial changes.
    const auto meter = def_at + (rows + added_icons) * 8;
    put(meter, EnergyArtwork::meter_icon);
    put(meter + 2, std::uint16_t(3));
    put(meter + 4, static_cast<std::uint16_t>(frames + added_frames));
    put(meter + 6, read<std::uint16_t>(oxygen_row + 6));
    const auto original_first = read<std::uint16_t>(oxygen_row + 4);
    copy(refs_at + (frames + added_frames) * 4, m_original[1] + original_first * 4, 8);
    const auto electric_first = frames + added_frames - EnergyArtwork::frames;
    std::memcpy(data.data() + refs_at + (frames + added_frames + 2) * 4,
                data.data() + refs_at + electric_first * 4, 4);
    put(def_at + (rows + added_icons + 1) * 8, std::uint16_t(0xFFFF));
    const auto charge_at = allocate(48); // charge followed by the owned native sound definition
    put(charge_at, 10000);
    if (start < cursor || !range(start, static_cast<std::uint32_t>(data.size()) + 63))
        throw std::runtime_error("No safe game memory for Clank HUD icons");
    std::memcpy(m_api.memory + start, data.data(), data.size());
    m_arena = arena;
    m_level = m_api.loaded_level ? m_api.loaded_level() : -1;
    m_start = start;
    m_end = (start + static_cast<std::uint32_t>(data.size()) + 63u) & ~63u;
    m_installed = {start + def_at, start + refs_at, start + img_at, start + pal_at};
    // Arena order: definitions, references, image pages, palette pages.
    // m_original has this same order; the checked counts above follow that order.
    m_new_palette = start + pal_at + palettes * 8;
    m_new_images = start + img_at + images * 8;
    m_charge = start + charge_at;
    m_palette_count = added_icons;
    m_image_count = added_frames;
    for (unsigned i = 0; i < 4; ++i)
        write(arena + 0x1C + i * 4, m_installed[i]);
    return m_end;
}
void HudAssets::before_draw() noexcept {
    if (!m_arena || read<std::uint32_t>(m_arena + 0x1C) != m_installed[0])
        return;
    // These extra pages are outside the original bank's index ranges. Let the
    // native texture allocator upload them afresh at this frame's free cursor.
    for (unsigned i = 0; i < m_palette_count; ++i)
        write(m_new_palette + i * 8 + 4, std::uint16_t(0));
    for (unsigned i = 0; i < m_image_count; ++i)
        write(m_new_images + i * 8 + 4, std::uint16_t(0));
}
void HudAssets::remove() noexcept {
    if (m_arena && (!m_api.loaded_level || m_api.loaded_level() == m_level) &&
        read<std::uint32_t>(m_arena + 0x1C) == m_installed[0])
        for (unsigned i = 0; i < 4; ++i)
            write(m_arena + 0x1C + i * 4, m_original[i]);
    m_arena = m_start = m_end = m_new_palette = m_new_images = 0;
    m_charge = 0;
    m_palette_count = m_image_count = 0;
    m_original = m_installed = {};
}
} // namespace clank

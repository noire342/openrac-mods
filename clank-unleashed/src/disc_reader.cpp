// SPDX-License-Identifier: GPL-3.0-or-later
// Disc layout: OpenRAC d015ea1c, port/assets/disc/{toc,level,disc}.{h,cpp}.
// Copyright (c) 2026 OpenRAC contributors. Adapted from ReRAC's ISC format
// documentation, Copyright (c) 2026 ReRAC contributors. No disc bytes shipped.
#include "resources.hpp"
#include "wad.hpp"
#include "game_profile.hpp"
#include "asset_hash.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
namespace clank {
namespace {
std::uint32_t word(const std::vector<std::uint8_t> &data, std::size_t at) {
    return ByteView(data).u32_at(at);
}
class Disc final {
  public:
    explicit Disc(const std::filesystem::path &path) : file(path, std::ios::binary) {
        if (!file)
            throw std::runtime_error("Cannot open the player's R&C1 disc image");
        file.seekg(0, std::ios::end);
        size = static_cast<std::uint64_t>(file.tellg());
        if (size != profile::disc_bytes)
            throw std::runtime_error(
                "This preview requires the supported PAL SCES_509.16 v2.00 disc image");
    }
    std::vector<std::uint8_t> read(std::uint64_t at, std::uint64_t bytes) {
        if (bytes > 64 * 1024 * 1024 || at > size || bytes > size - at)
            throw std::runtime_error("Invalid R&C1 disc range");
        std::vector<std::uint8_t> out(static_cast<std::size_t>(bytes));
        file.seekg(static_cast<std::streamoff>(at));
        file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(bytes));
        if (!file)
            throw std::runtime_error("Incomplete R&C1 disc read");
        return out;
    }

  private:
    std::ifstream file;
    std::uint64_t size{};
};
std::vector<std::uint8_t> slice(const std::vector<std::uint8_t> &data, std::size_t at,
                                std::size_t count) {
    if (at > data.size() || count > data.size() - at)
        throw std::runtime_error("Invalid class resource slice");
    return {data.begin() + at, data.begin() + at + count};
}
} // namespace
std::vector<std::uint8_t> read_clank_class(const std::filesystem::path &iso) {
    Disc disc(iso);
    const auto toc = disc.read(1500u * 2048u, 0x2960);
    if (word(toc, 0) != 1)
        throw std::runtime_error("Unsupported R&C1 disc table");
    const auto header = disc.read(std::uint64_t(word(toc, 0x28C8 + 6 * 8)) * 2048, 0x2434);
    if (word(header, 4) != 0x2434)
        throw std::runtime_error("Unsupported Blarg level header");
    const auto data =
        disc.read(std::uint64_t(word(header, 8)) * 2048, std::uint64_t(word(header, 12)) * 2048);
    const auto index = slice(data, word(data, 0x10), word(data, 0x14));
    auto packed = slice(data, word(data, 0x50), word(data, 0x54));
    const auto core = wad_decompress(ByteView(packed));
    std::set<std::uint32_t> boundaries{static_cast<std::uint32_t>(core.size())};
    for (unsigned at : {8u, 12u, 16u, 20u, 80u, 84u, 88u})
        if (auto v = word(index, at); v)
            boundaries.insert(v);
    std::uint32_t clank = 0;
    for (unsigned table : {0x18u, 0x20u, 0x28u}) {
        const auto count = word(index, table), start = word(index, table + 4);
        const unsigned stride = table == 0x28 ? 48 : 32;
        if (count > 2048)
            throw std::runtime_error("Invalid class count");
        for (unsigned i = 0; i < count; ++i) {
            const auto at = start + i * stride, offset = word(index, at);
            if (offset)
                boundaries.insert(offset);
            if (table == 0x18 && word(index, at + 4) == 87)
                clank = offset;
        }
    }
    if (auto table = word(index, 0x78); table)
        for (unsigned i = 0; i < 256; ++i) {
            const auto at = word(index, table + i * 4);
            if (at && at < core.size())
                boundaries.insert(at);
        }
    const auto gadgets = word(index, 0x80), table = word(index, 0x84);
    if (gadgets > 64)
        throw std::runtime_error("Invalid gadget count");
    for (unsigned i = 0; i < gadgets; ++i)
        if (auto at = word(index, table + i * 16); at)
            boundaries.insert(at);
    const auto next = boundaries.upper_bound(clank);
    if (!clank || next == boundaries.end() || *next <= clank || *next - clank > 1024 * 1024)
        throw std::runtime_error("Playable Clank resource missing from the player's disc");
    auto blob = slice(core, clank, *next - clank);
    if (blob.size() < 128 || blob[0xC] != 19)
        throw std::runtime_error("Unexpected playable Clank animation resource");
    // Native relocation follows pointers inside this resource. Admit only the
    // exact PAL layout verified with the pinned executable, before any writes.
    if (!sha256_matches(blob.data(), blob.size(), profile::payload_sha256))
        throw std::runtime_error("Unsupported playable Clank resource; this preview requires the "
                                 "verified PAL disc revision");
    return blob;
}
AlarmSource read_alarm_source(const std::filesystem::path &iso) {
    Disc disc(iso);
    const auto toc = disc.read(1500u * 2048u, 0x2960);
    const auto header = disc.read(std::uint64_t(word(toc, 0x28C8 + 8)) * 2048, 0x2434);
    const auto base = std::uint64_t(word(header, 8)) * 2048;
    const auto root = disc.read(base, 0x58);
    const auto bank_at = base + word(root, 8);
    const auto bank = disc.read(bank_at, word(root, 12));
    if (!sha256_matches(bank.data(), bank.size(), profile::alarm_bank_sha256))
        throw std::runtime_error("Unsupported original oxygen alarm bank");
    const auto index = disc.read(base + word(root, 0x10), word(root, 0x14));
    const auto packed = disc.read(base + word(root, 0x50), word(root, 0x54));
    const auto core = wad_decompress(ByteView(packed));
    const auto count = word(index, 0x18), classes = word(index, 0x1C), remap = word(index, 0x70);
    if (count > 2048)
        throw std::runtime_error("Invalid alarm source class table");
    for (unsigned i = 0; i < count; ++i) {
        const auto row = classes + i * 32;
        if (word(index, row + 4) != 0)
            continue;
        const auto blob = word(index, row);
        const auto defs = blob + word(core, blob + 0x28);
        const auto definition = slice(core, defs + 11 * 32, 32);
        const auto map = word(index, remap + 8 + i * 4);
        const auto offset = map & 0xFFFFu, entries = map >> 16;
        if (entries <= 11 || offset >= 0x8000u ||
            !sha256_matches(definition.data(), definition.size(), profile::alarm_definition_sha256))
            throw std::runtime_error("Unsupported original oxygen alarm definition");
        AlarmSource out;
        out.sector = static_cast<std::uint32_t>(bank_at / 2048);
        out.offset = static_cast<std::uint32_t>(bank_at % 2048);
        out.sound = static_cast<std::int16_t>(word(index, remap + offset + 11 * 4) & 0xFFFFu);
        if (out.sound != 31)
            throw std::runtime_error("Unexpected native oxygen alarm sound mapping");
        std::copy(definition.begin(), definition.end(), out.definition.begin());
        return out;
    }
    throw std::runtime_error("Original oxygen alarm is absent from the supported disc");
}
GadgebotIcon read_gadgebot_icon(const std::filesystem::path &iso) {
    Disc disc(iso);
    const auto toc = disc.read(1500u * 2048u, 0x2960);
    const auto header = disc.read(std::uint64_t(word(toc, 0x28C8 + 6 * 8)) * 2048, 0x2434);
    const auto base = std::uint64_t(word(header, 8)) * 2048;
    const auto root = disc.read(base, 0x58);
    const auto meta = disc.read(base + word(root, 0x20), word(root, 0x24));
    const auto packed = disc.read(base + word(root, 0x30), word(root, 0x34));
    const auto pixels = wad_decompress(ByteView(packed));
    const ByteView view(meta);
    const auto defs = word(meta, 4), refs = word(meta, 8);
    const auto palette_pages = word(meta, 12), image_pages = word(meta, 16);
    std::uint32_t entry = 0;
    for (unsigned i = 0; i < 2048; ++i) {
        const auto at = defs + i * 8;
        const auto id = view.u32_at(at) & 0xFFFFu;
        if (id == 0x7553) {
            entry = at;
            break;
        }
        if (id == 0xFFFF)
            break;
    }
    if (!entry || (view.u32_at(entry) >> 16) != 8)
        throw std::runtime_error("Original Gadgebot HUD icon is absent from the player's disc");
    const auto first = view.u32_at(entry + 4) & 0xFFFFu;
    GadgebotIcon icon;
    std::uint32_t palette_offset = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const auto pair = view.u32_at(refs + (first + i) * 4);
        const auto pal = pair & 0xFFFFu, img = pair >> 16;
        if (pal >= word(meta, 0x18) || pal < word(meta, 0x14) || img >= word(meta, 0x38) ||
            img < word(meta, 0x34))
            throw std::runtime_error("Unsupported Gadgebot icon bank");
        const auto pal_offset = word(meta, palette_pages + pal * 8) & 0x7FFFFFFFu;
        if (i && pal_offset != palette_offset)
            throw std::runtime_error("Unexpected Gadgebot icon palettes");
        palette_offset = pal_offset;
        const auto image = image_pages + img * 8;
        if ((word(meta, image + 4) >> 16) != 0x0505)
            throw std::runtime_error("Unexpected Gadgebot icon dimensions");
        icon.images[i] = slice(pixels, word(meta, image) & 0x7FFFFFFFu, 1024);
    }
    icon.palette = slice(pixels, palette_offset, 1024);
    auto bundle = icon.palette;
    for (const auto &image : icon.images)
        bundle.insert(bundle.end(), image.begin(), image.end());
    if (!sha256_matches(bundle.data(), bundle.size(), profile::gadgebot_icon_sha256))
        throw std::runtime_error("Unsupported original Gadgebot HUD icon resource");
    return icon;
}
} // namespace clank

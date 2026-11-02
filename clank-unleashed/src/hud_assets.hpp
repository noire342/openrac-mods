// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "energy_artwork.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <openrac/openrac_mod.h>
#include <vector>

namespace clank {
struct GadgebotIcon {
    std::vector<std::uint8_t> palette;
    std::array<std::vector<std::uint8_t>, 8> images;
};
GadgebotIcon read_gadgebot_icon(const std::filesystem::path &iso);

// Extends the four HUD lookup tables with original mod energy artwork and any
// missing Gadgebot icon. Every existing entry remains intact.
class HudAssets final {
  public:
    HudAssets(const openrac_api &api, const GadgebotIcon &icon) : m_api(api), m_icon(icon) {}
    ~HudAssets() { remove(); }
    std::uint32_t reserve(std::uint32_t cursor);
    void remove() noexcept;
    void abandon() noexcept { m_arena = m_charge = 0; }
    void before_draw() noexcept;
    std::uint32_t charge_address() const noexcept { return m_charge; }
    void charge(int value) noexcept { write(m_charge, value); }

  private:
    template <class T> T read(std::uint32_t at) const {
        T value{};
        if (!range(at, sizeof(T)))
            return value;
        std::memcpy(&value, m_api.memory + at, sizeof(T));
        return value;
    }
    template <class T> void write(std::uint32_t at, T value) {
        if (range(at, sizeof(T)))
            std::memcpy(m_api.memory + at, &value, sizeof(T));
    }
    static bool range(std::uint32_t at, std::uint32_t size) noexcept {
        return at >= 0x10000 && size <= 0x2000000 && at <= 0x2000000 - size;
    }
    const openrac_api &m_api;
    const GadgebotIcon &m_icon;
    const EnergyArtwork m_energy = make_energy_artwork();
    std::uint32_t m_arena{}, m_start{}, m_end{}, m_new_palette{}, m_new_images{};
    std::uint32_t m_charge{};
    unsigned m_palette_count{}, m_image_count{};
    std::array<std::uint32_t, 4> m_original{}, m_installed{};
    int m_level = -1;
};
} // namespace clank

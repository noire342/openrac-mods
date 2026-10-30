// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include <cstdint>

#include "discs.hpp"
#include "openrac/openrac_mod.h"

namespace weaponxp {

// Adapter for the stock SDK: the existing weapon HUD bank's void(u32) draw callback is
// routed through an already-hookable void(u32) shared callback. Only registered guest function
// addresses are stored in game data. No native pointers, code patches or new HUD banks.
class HudBridge {
public:
    HudBridge(
        const openrac_api* api,
        const Disc& disc,
        void* weapon_draw,
        void* relay,
        void* gauge_draw = nullptr
    );
    ~HudBridge();
    void attach(int item = -1);
    void detach();
    bool owns(std::uint32_t element) const;
    void refresh_capacity(int item, int capacity);

private:
    const openrac_api* m_api;
    const Disc& m_disc;
    void* m_draw;
    void* m_relay;
    void* m_gauge;
    std::uint32_t m_slot = 0, m_original = 0, m_address = 0;
    int m_level = -1;
    std::uint32_t read(std::uint32_t address) const;
    void write(std::uint32_t address, std::uint32_t value);
};

}  // namespace weaponxp

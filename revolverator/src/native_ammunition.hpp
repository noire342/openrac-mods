// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "memory.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace revolverator {

// Owns only Revolverator's inventory metadata and its ammunition pickup views.
// The game retains responsibility for granting, saving, selling and collecting
// ammunition. No inventory counters are rewritten when a level is entered.
class NativeAmmunition final : public Memory {
public:
    static constexpr int item_id = 18;
    static constexpr int capacity = 10;
    static constexpr int pickup = 2;
    static constexpr int unit_price = 25;
    static constexpr unsigned native_pickup_class = 225;
    static constexpr unsigned visual_class = 32000;

    explicit NativeAmmunition(const openrac_api* api,GuestArena* arena=nullptr,bool hud_fallback=false);
    ~NativeAmmunition();
    NativeAmmunition(const NativeAmmunition&) = delete;
    NativeAmmunition& operator=(const NativeAmmunition&) = delete;

    // Call after level load and before the first native Item_Give, then once per
    // frame. reset() belongs before native level unload, after Presentation.reset.
    void frame();
    void reset();
    bool begin_attack();
    int count() const;
    int maximum() const;

    // Wrap rac1.moby.ammo_pickup.update. Only pickups whose native pvars name
    // inventory slot 18 receive the companion model's render-only class ID.
    void before_update(unsigned moby);
    void after_update(unsigned moby);

private:
    using TakeAmmo = int(*)(int,int);
    TakeAmmo take_ammo_;
    int level_ = -1;
    bool metadata_ready_ = false;
    int plural_id_ = 0, singular_id_ = 0;

    struct Patch {
        unsigned address = 0;
        std::uint16_t original = 0, replacement = 0;
    };
    std::vector<Patch> patches_;

    struct Pickup {
        unsigned address = 0, pvars = 0, stamp = 0;
        std::uint16_t original_class = 0;
    };
    std::vector<Pickup> pickups_;

    struct TextBank {
        unsigned original = 0, copy = 0;
        int count = 0;
        std::vector<std::array<std::uint32_t,4>> snapshot;
    } text_bank_;

    bool in_level() const;
    bool patch16(unsigned address, std::uint16_t replacement);
    bool extend_text_bank();
    bool patch_metadata();
    bool pickup_valid(const Pickup& lease) const;
    Pickup* find_pickup(unsigned moby);
    void restore_text_bank();
};

}

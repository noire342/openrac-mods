// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include "arsenal_menu_model.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace revolverator {
struct WeaponMenuCell {
    std::uint16_t icon{};
    std::int16_t frame{},kind{},id{},stream_entry{};
    friend bool operator==(const WeaponMenuCell& a,const WeaponMenuCell& b){
        return a.icon==b.icon&&a.frame==b.frame&&a.kind==b.kind&&a.id==b.id&&a.stream_entry==b.stream_entry;
    }
};
static_assert(sizeof(WeaponMenuCell)==10,"native menu cells retain their console layout");

// Extends the two existing weapon grids and keeps the existing eight quick
// slots. Logical keys never reach the native item arrays or saved native IDs.
class NativeWeaponMenu final:public Memory {
public:
    using Call=int(*)(unsigned);
    using WeaponKey=arsenal::WeaponKey;
    static constexpr unsigned WeaponGrid=0x1cee58,QuickWeaponGrid=0x1cf298;
    static constexpr unsigned QuickGadgetGrid=0x1cf300,QuickRing=0x1cf368;
    static constexpr unsigned WeaponCells=0x1cf120,MenuSystem=0x1d5bf0;
    static constexpr unsigned Columns=5,VisibleRows=4,VisibleCells=Columns*VisibleRows;
    NativeWeaponMenu(const openrac_api* api,GuestArena& arena,ArsenalMenuModel& model);
    ~NativeWeaponMenu();
    void frame();
    void reset();
    bool ready()const;
    // Static widget labels are often only referenced by data pointers, so
    // resolve them relative to the validated, code-referenced cell table.
    unsigned screen_address(unsigned canonical)const;
    bool owns_grid(unsigned screen)const;
    WeaponKey selected_key()const;
    WeaponKey equipped_key()const{return model_.registry().equipped();}
    // The native renderer still owns panels, spacing, focus and highlighting.
    // A smaller metric can be applied only while drawing a weapon grid.
    void set_icon_scale(float value);
    int grid_update(unsigned screen,Call original);
    int grid_draw(unsigned screen,Call original);
    int slots_update(unsigned screen,Call original);
    int quick_load(unsigned screen,Call original);
    int quick_store(unsigned screen,Call original);
    int quick_draw(unsigned screen,Call original);
    int selected_call(unsigned screen,Call original);
    int equipped_call(unsigned screen,Call original);
    // Diagnostics and layout invariants, without guest pointers in a public API.
    unsigned catalog_size()const{return unsigned(catalog_.size());}
    unsigned first_row(unsigned screen)const;
private:
    ArsenalMenuModel& model_;
    struct Entry {WeaponMenuCell cell;WeaponKey key;};
    struct Grid {
        unsigned address{},original_cells{},cells{};
        int original_rows{},original_cols{},rows{},start_row{};
        bool active=false;
    };
    std::array<Grid,2> grids_{};
    std::vector<Entry> catalog_;
    std::array<WeaponMenuCell,15> native_{};
    int level_=-1;
    float icon_scale_=.8f;
    std::size_t descriptor_count_=0;
    using Sprite=void(*)(int,int,int,int,int,int);
    using FindFrame=int(*)(int,int);
    using Paging=void(*)(int);
    using FinishPaging=void(*)();
    using Trig=float(*)(float);
    using Sound=int(*)(int,int,unsigned);
    Sprite sprite_;FindFrame find_;Paging paging_;FinishPaging finish_;
    Trig cosine_,sine_,normalize_;Sound sound_;
    bool supported()const;
    bool owned(WeaponKey key)const;
    Grid* grid(unsigned screen);
    const Grid* grid(unsigned screen)const;
    unsigned focused()const;
    unsigned pending_focus()const;
    unsigned pressed_at()const{return global(0x13c940)+0x1c4;}
    unsigned visible_rows()const;
    unsigned maximum_start_row()const;
    WeaponKey key_at(const Grid& grid,int cell)const;
    void fill(Grid& grid);
    void write_native_slots(unsigned screen);
    void follow_grid_transition(unsigned from,unsigned buttons);
};
}

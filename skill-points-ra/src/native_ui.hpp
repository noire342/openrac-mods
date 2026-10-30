// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine.hpp"
#include "menu.hpp"
#include "pause_access.hpp"

#include <openrac/openrac_mod.h>

namespace skillpoints {
class NativeUi {
public:
    NativeUi(const openrac_api*, const Ledger&, const Engine&);

    ~NativeUi() { restore(); }

    void refresh();
    void restore();
    bool handles(std::uint32_t address) const;
    bool owns(std::uint32_t address) const;
    std::uint32_t invoke(std::uint32_t address);
    void notify();

private:
    enum class Role {
        List,
        Description,
        Title,
        Heading,
        Status
    };

    struct Patch {
        std::uint32_t address, original, replacement, screen;
        Role role;
    };

    void patch(std::uint32_t screen, std::uint32_t offset, const char* expected, Role);
    void text(
        std::uint32_t screen,
        const std::string&,
        bool small,
        bool center,
        int top = 4,
        int bottom = -1,
        std::uint32_t color = 0x80FFA888u,
        int inset = 8
    );
    std::string display_title(const Definition&) const;
    std::string fit_title(const std::string&, int width) const;
    void draw_list(std::uint32_t screen);
    std::uint32_t word(std::uint32_t address) const;
    void word(std::uint32_t address, std::uint32_t value);
    bool in_page() const;
    const openrac_api* m_api;
    const Ledger& m_ledger;
    const Engine& m_engine;
    PauseAccess m_pause;
    Menu m_menu;
    std::vector<Patch> m_patches;
    std::uint32_t m_page = 0, m_input = 0, m_relay = 0;
    int m_level = -99;
    std::uint32_t m_last_input_tick = 0xffffffffu;
};
}  // namespace skillpoints

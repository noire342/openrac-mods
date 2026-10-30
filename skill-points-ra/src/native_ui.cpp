// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_ui.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "native_map.hpp"

namespace skillpoints {
NativeUi::NativeUi(const openrac_api* api, const Ledger& ledger, const Engine& engine)
    : m_api(api),
      m_ledger(ledger),
      m_engine(engine),
      m_pause(api) {}

std::uint32_t NativeUi::word(std::uint32_t a) const {
    std::uint32_t v = 0;
    if (a >= 0x10000 && a <= 0x02000000 - 4) {
        std::memcpy(&v, m_api->memory + a, 4);
    }
    return v;
}

void NativeUi::word(std::uint32_t a, std::uint32_t v) {
    if (a >= 0x10000 && a <= 0x02000000 - 4) {
        std::memcpy(m_api->memory + a, &v, 4);
    }
}

bool NativeUi::in_page() const {
    return m_page && word(m_api->relocate_data(0x001D5BF0) + 4) == m_page;
}

void NativeUi::patch(std::uint32_t screen, std::uint32_t offset, const char* expected, Role role) {
    const auto a = screen + offset, current = word(a);
    if (!current || !m_api->function(expected)
        || m_api->function_at(current) != m_api->function(expected)) {
        throw std::runtime_error(
            std::string("Unsupported skill points callback: ") + expected
            + " screen=" + std::to_string(screen) + " code=" + std::to_string(current)
        );
    }
    m_patches.push_back({a, current, m_relay, screen, role});
    word(a, m_relay);
}

void NativeUi::restore() {
    m_pause.restore();
    // Ownership: never overwrite a later mod's callback or a fresh level's structures.
    if (m_level == m_api->loaded_level()) {
        for (auto it = m_patches.rbegin(); it != m_patches.rend(); ++it) {
            if (word(it->address) == it->replacement) {
                word(it->address, it->original);
            }
        }
    }
    m_patches.clear();
    m_page = 0;
    m_input = 0;
    m_level = -99;
    m_last_input_tick = 0xffffffffu;
}

void NativeUi::refresh() {
    m_menu.filter(m_engine.policy());
    const auto level = m_api->loaded_level();
    if (level < 0 || level >= 19) {
        restore();
        return;
    }
    if (level == m_level && !m_patches.empty()) {
        m_pause.refresh();
        return;
    }
    restore();
    m_level = level;
    m_relay = kRelay[level];
    if (m_api->function_at(m_relay) != m_api->function("FUN_L00_00241940")) {
        throw std::runtime_error("Incompatible native skill points relay");
    }
    // Menu objects are separate globals. A nearest-address relocation can land
    // in another menu object; discover the original object by its verified code
    // callbacks, then follow the game's own page/screen pointers.
    for (std::uint32_t a = 0x00160000; a < 0x00260000; a += 4) {
        if (word(a) == kPointInput[level] && word(a + 4) == kPointList[level]
            && (word(a + 0x30) & ~0x8000u) == 0x200u) {
            if (m_input) {
                throw std::runtime_error("Ambiguous skill points menu object");
            }
            m_input = a;
        }
    }
    if (!m_input) {
        m_level = -99;
        return;
    }  // the level's data may still be loading
    for (std::uint32_t a = 0x00160000; a < 0x00260000; a += 4) {
        if (word(a + 0x40) == m_input && word(a + 0x3C) == 0x24u && word(a + 0x48) == m_input) {
            if (m_page) {
                throw std::runtime_error("Ambiguous skill points page");
            }
            m_page = a;
        }
    }
    if (!m_page) {
        throw std::runtime_error("Original skill points page not found");
    }
    try {
        m_pause.attach(word(m_page + 0x38), level);
        patch(m_input, 4, "FUN_0021d4a8", Role::List);
        patch(word(m_page + 0x4C), 4, "FUN_00220348", Role::Description);
        patch(word(m_page + 0x54), 4, "FUN_0021a328", Role::Title);
        patch(word(m_page + 0x44), 4, "FUN_0021a328", Role::Heading);
        patch(word(m_page + 0x58), 4, "FUN_002220f0", Role::Status);
        m_pause.refresh();
    } catch (...) {
        restore();
        throw;
    }
}

bool NativeUi::handles(std::uint32_t screen) const {
    if (!in_page()) {
        return false;
    }
    return owns(screen);
}

bool NativeUi::owns(std::uint32_t screen) const {
    for (const auto& p : m_patches) {
        if (p.screen == screen && word(p.address) == p.replacement) {
            return true;
        }
    }
    return false;
}

void NativeUi::text(
    std::uint32_t screen,
    const std::string& str,
    bool small,
    bool center,
    int top,
    int bottom,
    std::uint32_t color,
    int inset
) {
    // Use the game's own font, clipping, wrapping and currently projected menu panel.
    const int w = static_cast<int>(word(screen + 0x20)), h = static_cast<int>(word(screen + 0x24));
    if (w < 16 || w > 2048 || h < 10 || h > 2048 || str.size() > 1500) {
        return;
    }

    struct Frame {
        const openrac_api* api;
        openrac_gaddr saved = 0, at = 0;

        Frame(const openrac_api* a, std::uint32_t n) : api(a) { at = api->stack_push(n, &saved); }

        ~Frame() {
            if (at) {
                api->stack_pop(saved);
            }
        }
    } frame(m_api, static_cast<std::uint32_t>(str.size() + 1 + 32));

    if (!frame.at) {
        return;
    }
    std::int16_t region[12] =
        {static_cast<std::int16_t>(top),
         static_cast<std::int16_t>(bottom < 0 ? h - 4 : bottom),
         4,
         static_cast<std::int16_t>(w - 4),
         static_cast<std::int16_t>(center ? w / 2 : inset),
         static_cast<std::int16_t>(top + 2),
         0,
         0,
         static_cast<std::int16_t>(small ? 12 : 14),
         static_cast<std::int16_t>(center ? 1 : 0),
         0,
         0};
    std::memcpy(m_api->memory + frame.at, region, sizeof region);
    std::memcpy(m_api->memory + frame.at + 32, str.c_str(), str.size() + 1);
    using Page = void (*)(int);
    using Flush = void (*)();
    using Font = void (*)(std::uint32_t, std::uint32_t, std::uint32_t, int);
    const auto begin = reinterpret_cast<Page>(m_api->function("FUN_001f4280"));
    const auto end = reinterpret_cast<Flush>(m_api->function("FUN_001f4398"));
    const auto font =
        reinterpret_cast<Font>(m_api->function(small ? "FUN_001f75f0" : "FUN_001f7580"));
    if (begin && end && font) {
        begin(0);
        font(frame.at, color, frame.at + 32, -1);
        end();
    }
}

std::uint32_t NativeUi::invoke(std::uint32_t screen) {
    const auto found = std::find_if(m_patches.begin(), m_patches.end(), [&](const Patch& p) {
        return p.screen == screen;
    });
    if (found == m_patches.end()) {
        return 0;
    }
    const auto role = found->role;
    using Original = std::int32_t (*)(std::uint32_t);
    if (role == Role::List) {
        const auto tick = word(m_api->relocate_data(0x001D5BF0) + 0x110);
        const auto pad = m_api->relocate_data(0x0013C940);
        const auto buttons = word(pad + 0x1C4);
        if (tick != m_last_input_tick) {
            m_last_input_tick = tick;
            const auto old_native = static_cast<std::uint32_t>(m_menu.native_selection());
            bool moved = false;
            if (buttons & 0x1000u) {
                m_menu.step(-1);
                moved = true;
            } else if (buttons & 0x4000u) {
                m_menu.step(1);
                moved = true;
            }
            if (moved && word(screen + 0x40) == old_native) {
                using Sound = int (*)(int, int, std::uint32_t);
                if (auto sound = reinterpret_cast<Sound>(m_api->function("FUN_0022da68"))) {
                    sound(1, 0x11, word(screen + 0x14));
                }
            }
        }
        word(screen + 0x40, static_cast<std::uint32_t>(m_menu.native_selection()));
        draw_list(screen);
        return 2;
    }
    if (role == Role::Heading) {
        text(
            screen,
            "Punti stile " + std::to_string(m_engine.policy().unlocked(m_ledger)) + " / "
                + std::to_string(m_menu.order().size()),
            false,
            true
        );
        return 2;
    }
    if (!m_menu.extended()) {
        const auto original = reinterpret_cast<Original>(m_api->function_at(found->original));
        return original ? static_cast<std::uint32_t>(original(screen)) : 2;
    }
    const auto& d = m_menu.current();
    const auto measured = m_engine.measured(d.id);
    std::string status = m_ledger.unlocked(d.id) ? "Ottenuto" : "Da ottenere";
    if (!m_ledger.unlocked(d.id) && measured.second) {
        status += "  " + std::to_string(measured.first) + " / " + std::to_string(measured.second);
    }
    if (role == Role::Title) {
        const int height = static_cast<int>(word(screen + 0x24));
        text(screen, std::string(d.title), true, true, std::max(6, height / 3 - 16));
        return 2;
    }
    if (role == Role::Status) {
        text(
            screen,
            status + "\1" + std::to_string(d.points) + " punti  -  " + m_menu.position(),
            true,
            true
        );
        return 2;
    }
    // The original large image panel may use a 512x256 capture target. Keep
    // descriptive text near its top rather than placing a footer beyond that target.
    text(
        screen,
        std::string(d.description) + "\1\1" + std::to_string(d.points) + " punti",
        true,
        false,
        10,
        240
    );
    return 2;
}

std::string NativeUi::display_title(const Definition& d) const {
    if (d.native_index < 0) {
        return d.title;
    }
    const auto items = word(m_input + 0x34);
    const auto id = word(items + 12 * static_cast<unsigned>(d.native_index)) & 0xffffu;
    using Help = std::uint32_t (*)(int);
    auto fn = reinterpret_cast<Help>(m_api->function("FUN_001fdd10"));
    if (!fn) {
        return d.title;
    }
    const auto ptr = fn(static_cast<int>(id));
    if (ptr < 0x10000 || ptr > 0x02000000 - 256) {
        return d.title;
    }
    std::string result;
    for (unsigned i = 0; i < 255 && m_api->memory[ptr + i]; ++i) {
        result += static_cast<char>(m_api->memory[ptr + i]);
    }
    return result.empty() ? d.title : result;
}

std::string NativeUi::fit_title(const std::string& title, int width) const {
    using Measure = int (*)(std::uint32_t, int);
    auto fn = reinterpret_cast<Measure>(m_api->function("FUN_001f6250"));
    if (!fn) {
        return title.substr(0, static_cast<std::size_t>(std::max(1, width / 8)));
    }
    openrac_gaddr saved = 0;
    const auto at = m_api->stack_push(512, &saved);
    if (!at) {
        return {};
    }

    struct Pop {
        const openrac_api* a;
        openrac_gaddr s;

        ~Pop() { a->stack_pop(s); }
    } pop{m_api, saved};

    auto measure = [&](const std::string& s) {
        std::memcpy(m_api->memory + at, s.c_str(), s.size() + 1);
        return fn(at, -1);
    };
    auto result = title.substr(0, 255);
    if (measure(result) <= width) {
        return result;
    }
    while (!result.empty() && measure(result + "...") > width) {
        result.pop_back();
    }
    return result + "...";
}

void NativeUi::draw_list(std::uint32_t screen) {
    const int width = static_cast<int>(word(screen + 0x20)),
              height = static_cast<int>(word(screen + 0x24));
    if (width < 48 || height < 24) {
        return;
    }
    int tw = 7, th = 7;
    while ((1 << tw) < width) {
        ++tw;
    }
    while ((1 << th) < height) {
        ++th;
    }
    while (tw + th >= 18) {
        --th;
    }
    const int usable = std::min(height, 1 << th) - 8;
    const int count = std::max(1, usable / 20);
    const int selected = static_cast<int>(m_menu.index());
    const int total = static_cast<int>(m_menu.order().size());
    const int first = std::clamp(selected - count / 2, 0, std::max(0, total - count));
    using Marker = void (*)(int, int, int);
    using Begin = void (*)(int);
    using End = void (*)();
    const auto marker = reinterpret_cast<Marker>(m_api->function("FUN_0021f8e8"));
    const auto begin = reinterpret_cast<Begin>(m_api->function("FUN_001f4280"));
    const auto end = reinterpret_cast<End>(m_api->function("FUN_001f4398"));
    for (int row = 0; row < count && first + row < total; ++row) {
        const auto& d = *m_menu.order()[static_cast<std::size_t>(first + row)];
        const auto color = first + row == selected ? 0x8020FFFFu : 0x80FFA888u;
        text(
            screen,
            fit_title(display_title(d), width - 42),
            false,
            false,
            4 + row * 20,
            24 + row * 20,
            color,
            32
        );
        if (marker && begin && end) {
            begin(0);
            marker(15, 15 + row * 20, m_ledger.unlocked(d.id) ? 1 : 0);
            end();
        }
    }
}

void NativeUi::notify() {
    // Exactly the notification and sound the original skill point award uses.
    using Queue = void (*)(int, int);
    using Sound = int (*)(int, int, int);
    if (auto q = reinterpret_cast<Queue>(m_api->function("FUN_L00_00263d40"))) {
        q(0x53d6, -1);
    }
    if (auto s = reinterpret_cast<Sound>(m_api->function("FUN_0022db10"))) {
        s(1, 0, 0);
    }
}
}  // namespace skillpoints

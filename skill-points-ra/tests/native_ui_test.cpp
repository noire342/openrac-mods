#include <cstring>
#include <iostream>
#include <stdexcept>

#include "native_map.hpp"
#include "native_ui.hpp"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

namespace {
struct Memory : skillpoints::IMemory {
    std::vector<std::uint8_t> data = std::vector<std::uint8_t>(0x02000000);

    bool read(std::uint32_t a, void* out, std::uint32_t n) const override {
        if (a >= data.size() || n > data.size() - a) {
            return false;
        }
        std::memcpy(out, data.data() + a, n);
        return true;
    }
} ram;

int level = 1, draws = 0;

std::uint32_t get(std::uint32_t a) {
    std::uint32_t v;
    std::memcpy(&v, ram.data.data() + a, 4);
    return v;
}

void set(std::uint32_t a, std::uint32_t v) {
    std::memcpy(ram.data.data() + a, &v, 4);
}

int cyclic(std::uint32_t) {
    return 7;
}

int stream(std::uint32_t) {
    ++draws;
    return 8;
}

int config(std::uint32_t) {
    return 2;
}

int list(std::uint32_t) {
    return 2;
}

std::uint32_t loader(std::int32_t) {
    return 0;
}

void* function(const char* name) {
    if (!std::strcmp(name, "FUN_L00_00241940")) {
        return reinterpret_cast<void*>(loader);
    }
    if (!std::strcmp(name, "FUN_0021abf8")) {
        return reinterpret_cast<void*>(cyclic);
    }
    if (!std::strcmp(name, "FUN_0021d4a8")) {
        return reinterpret_cast<void*>(list);
    }
    if (!std::strcmp(name, "FUN_0021b1c8")) {
        return reinterpret_cast<void*>(list);
    }
    if (!std::strcmp(name, "FUN_002220f0")) {
        return reinterpret_cast<void*>(config);
    }
    if (!std::strcmp(name, "FUN_00220348")) {
        return reinterpret_cast<void*>(stream);
    }
    if (!std::strcmp(name, "FUN_0021a328")) {
        return reinterpret_cast<void*>(config);
    }
    return nullptr;
}

void* function_at(std::uint32_t a) {
    if (a == skillpoints::kRelay[level]) {
        return reinterpret_cast<void*>(loader);
    }
    if (a == skillpoints::kPointInput[level]) {
        return reinterpret_cast<void*>(cyclic);
    }
    if (a == skillpoints::kPointList[level]) {
        return reinterpret_cast<void*>(list);
    }
    if (a == 0x10000001) {
        return reinterpret_cast<void*>(stream);
    }
    if (a == 0x10000002) {
        return reinterpret_cast<void*>(config);
    }
    if (a == 0x10000003) {
        return reinterpret_cast<void*>(list);
    }
    return nullptr;
}

openrac_gaddr relocate(openrac_gaddr a) {
    return a;
}

int loaded() {
    return level;
}

openrac_gaddr push(std::uint32_t, openrac_gaddr* saved) {
    *saved = 0;
    return 0x01F00000;
}

void pop(openrac_gaddr) {}
}  // namespace

int main() {
    try {
        using namespace skillpoints;
        Ledger ledger;
        Engine engine(ram, ledger);
        openrac_api api{};
        api.memory = ram.data.data();
        api.function = function;
        api.function_at = function_at;
        api.relocate_data = relocate;
        api.loaded_level = loaded;
        api.stack_push = push;
        api.stack_pop = pop;
        NativeUi ui(&api, ledger, engine);
        const std::uint32_t input = 0x170000, description = 0x171000, title = 0x172000,
                            heading = 0x173000, page = 0x174000, goodies = 0x175000,
                            focus = 0x176000, items = 0x177000;
        const std::uint32_t pause = 0x180000;
        for (level = 0; level < 19; ++level) {
            set(input, kPointInput[level]);
            set(input + 4, kPointList[level]);
            set(input + 0x30, 0x200);
            set(description + 4, 0x10000001);
            set(title + 4, 0x10000002);
            set(heading + 4, 0x10000002);
            set(page + 0x40, input);
            set(page + 0x3C, 0x24);
            set(page + 0x44, heading);
            set(page + 0x48, input);
            set(page + 0x4C, description);
            set(page + 0x54, title);
            set(page + 0x58, 0x178000);
            set(0x178004, 0x10000002);
            set(page + 0x38, goodies);
            set(goodies + 0x40, focus);
            set(goodies + 0x38, pause);
            set(pause + 0x3C, 2);
            for (unsigned p = 0; p < 7; ++p) {
                const auto s = 0x181000 + 0x100 * p, entries = 0x183000 + 0x100 * p;
                set(pause + 0x44 + 4 * p, s);
                set(s, kPointInput[level]);
                set(s + 4, 0x10000003);
                set(s + 0x34, entries);
                set(entries, 0x30000u + (p < 6 ? 0x4EE2u + p : 0x4FC2u));
                set(entries + 4, p == 6 ? goodies : 0);
                set(entries + 12, 0);
            }
            set(0x181038, 0x181500);
            set(0x18153C, 0x181000);
            set(focus + 0x34, items);
            set(items, 0x00034FC4);
            set(items + 4, page);
            set(0x1D5BF0 + 4, page);
            set(0x13D408, 0x12345678);
            ui.refresh();
            // Initial tables can already point to the seventh panel; a subsequent
            // stock initializer still writes the hidden six-panel navigation.
            if (level % 2) {
                ui.restore();
                set(0x181038, 0x181600);
                set(0x18153C, 0x181600);
                ui.refresh();
            }
            set(0x1D5BF0 + 4, pause);
            set(0x1D5BF0 + 0xD8, 0);
            // The stock pause initializer resets these on every opening.
            set(0x181038, 0x181500);
            set(0x18153C, 0x181000);
            ui.refresh();
            CHECK(get(0x1D5BF0 + 0xD8) == 1);
            CHECK(get(0x181038) == 0x181600);
            CHECK(get(0x18153C) == 0x181600);
            set(0x1D5BF0 + 4, page);
            ui.refresh();
            CHECK(get(0x1D5BF0 + 0xD8) == 0);
            CHECK(ui.handles(input));
            CHECK(get(items) == 0x00034FC4);
            CHECK(get(input + 4) == kRelay[level]);
            CHECK(get(input) == kPointInput[level]);
            CHECK(ui.invoke(description) == 8);
            CHECK(draws == level + 1);
            set(0x13C940 + 0x1C4, 0x4000);
            for (unsigned i = 0; i < 30; ++i) {
                set(0x1D5BF0 + 0x110, get(0x1D5BF0 + 0x110) + 1);
                CHECK(ui.invoke(input) == 2);
            }
            CHECK(get(input + 0x40) < 30);
            CHECK(ui.invoke(description) == 2);  // extended entry stays in the same panel
            for (unsigned i = 0; i < 148; ++i) {
                set(0x1D5BF0 + 0x110, get(0x1D5BF0 + 0x110) + 1);
                CHECK(ui.invoke(input) == 2);  // wrap before the next overlay test
            }
            set(0x13C940 + 0x1C4, 0x10);
            CHECK(reinterpret_cast<int (*)(std::uint32_t)>(function_at(get(input)))(input) == 7);
            set(0x1D5BF0 + 4, 0);
            CHECK(!ui.handles(input));
            CHECK(ui.owns(input));
            set(0x1D5BF0 + 4, page);
            CHECK(get(0x13D408) == 0x12345678);  // no writes to original completion flags
            set(input + 4, 0x12345678);
            ui.restore();
            CHECK(get(0x181038) == 0x181500);
            CHECK(get(0x18153C) == 0x181000);
            CHECK(get(input + 4) == 0x12345678);  // another mod owns it now
            CHECK(get(description + 4) == 0x10000001);
            CHECK(get(items) == 0x00034FC4);
        }
        std::cout << "19 overlays: verified callback routing, bounded selection, native navigation "
                     "and ownership restoration passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

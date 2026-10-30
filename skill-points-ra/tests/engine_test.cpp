#include <cstring>
#include <iostream>
#include <stdexcept>

#include "engine.hpp"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

struct Memory : skillpoints::IMemory {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x02000000);

    bool read(std::uint32_t a, void* out, std::uint32_t n) const override {
        if (a >= bytes.size() || n > bytes.size() - a) {
            return false;
        }
        std::memcpy(out, bytes.data() + a, n);
        return true;
    }

    template <class T>
    void set(std::uint32_t a, T v) {
        std::memcpy(bytes.data() + a, &v, sizeof v);
    }
};

int main() {
    try {
        using namespace skillpoints;
        CHECK(kCatalogueSize == 178);
        unsigned points = 0, native = 0;
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            points += kCatalogue[i].points;
            if (kCatalogue[i].native_index >= 0) {
                ++native;
            }
        }
        CHECK(points == 667 && native == 30);
        Memory memory;
        Ledger ledger;
        Engine engine(memory, ledger);  // compile all 178 official definitions
        memory.set<std::uint8_t>(0x0015ED84, 1);
        memory.set<std::uint32_t>(0x0015F5C4, 6);
        memory.set<std::uint32_t>(0x00141584, 0xffffffffu);
        std::vector<std::uint32_t> events;
        auto collect = [&](auto id) {
            events.push_back(id);
        };
        engine.tick(100, collect);
        CHECK(!ledger.unlocked(254397));
        memory.set<std::uint8_t>(0x0013DD41, 1);
        engine.tick(101, collect);
        CHECK(engine.healthy());
        CHECK(ledger.unlocked(254397));
        CHECK(ledger.timestamp(254397) == 101);
        for (int i = 0; i < 10; ++i) {
            engine.tick(102 + i, collect);
        }
        CHECK(std::count(events.begin(), events.end(), 254397) == 1);
        engine.reset_session();
        memory.set<std::uint8_t>(0x0013DD41, 0);
        engine.tick(120, collect);
        memory.set<std::uint8_t>(0x0013DD41, 1);
        engine.tick(121, collect);
        CHECK(std::count(events.begin(), events.end(), 254397) == 1);
        Memory loaded;
        loaded.set<std::uint8_t>(0x0015ED84, 1);
        loaded.set<std::uint32_t>(0x0015F5C4, 6);
        loaded.set<std::uint32_t>(0x00141584, 0xffffffffu);
        loaded.set<std::uint8_t>(0x0013DD41, 1);
        Ledger fresh;
        Engine baseline(loaded, fresh);
        baseline.tick(200, [](auto) {});
        baseline.tick(201, [](auto) {});
        CHECK(!fresh.unlocked(254397));
        AchievementPolicy policy;
        policy.set_enabled(kKerwanTimeTrial, false);
        CHECK(policy.count() == 177 && policy.points() == 657);
        Ledger history;
        history.unlock(kKerwanTimeTrial, 123);
        CHECK(policy.unlocked(history) == 0 && policy.earned_points(history) == 0);
        policy.set_enabled(kKerwanTimeTrial, true);
        CHECK(history.timestamp(kKerwanTimeTrial) == 123);
        CHECK(policy.unlocked(history) == 1 && policy.earned_points(history) == 10);
        // Use an actual purchase transition to prove a disabled evaluator cannot award,
        // and that re-enabling starts from a fresh baseline, without stale frame state.
        Memory toggled;
        toggled.set<std::uint8_t>(0x15ED84, 1);
        toggled.set<std::uint32_t>(0x15F5C4, 6);
        toggled.set<std::uint32_t>(0x141584, 0xffffffffu);
        Ledger toggle_ledger;
        policy.set_enabled(254397, false);
        Engine toggle_engine(toggled, toggle_ledger, policy);
        toggle_engine.tick(300, [](auto) {});
        toggled.set<std::uint8_t>(0x13DD41, 1);
        toggle_engine.tick(301, [](auto) {});
        CHECK(!toggle_ledger.unlocked(254397));
        policy.set_enabled(254397, true);
        toggle_engine.tick(302, [](auto) {});
        CHECK(!toggle_ledger.unlocked(254397));
        toggled.set<std::uint8_t>(0x13DD41, 0);
        toggle_engine.tick(303, [](auto) {});
        toggled.set<std::uint8_t>(0x13DD41, 1);
        toggle_engine.tick(304, [](auto) {});
        CHECK(toggle_ledger.unlocked(254397));
        std::cout << "178 rules compiled; transition, duplicate, session reset and loaded-save "
                     "boundaries passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

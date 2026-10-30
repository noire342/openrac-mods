#include <iostream>
#include <set>
#include <stdexcept>

#include "menu.hpp"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

int main() {
    try {
        using namespace skillpoints;
        Menu menu;
        std::set<std::uint32_t> ids;
        for (unsigned i = 0; i < 178; ++i) {
            menu.select(i);
            ids.insert(menu.current().id);
            CHECK(menu.native_selection() >= 0 && menu.native_selection() < 30);
            if (i < 30) {
                CHECK(menu.current().native_index == static_cast<int>(i));
                CHECK(!menu.extended());
            } else {
                CHECK(menu.extended());
            }
        }
        CHECK(ids.size() == 178);
        menu.select(0);
        menu.step(-1);
        CHECK(menu.index() == 177);
        menu.step(1);
        CHECK(menu.index() == 0);
        menu.step(178);
        CHECK(menu.index() == 0);
        CHECK(menu.position() == "1 / 178");
        AchievementPolicy policy;
        for (unsigned i = 0; i < 178; ++i) {
            if (menu.order()[i]->id == kKerwanTimeTrial) {
                menu.select(i);
            }
        }
        policy.set_enabled(kKerwanTimeTrial, false);
        menu.filter(policy);
        CHECK(menu.order().size() == 177 && menu.current().id != kKerwanTimeTrial);
        for (const auto* d : menu.order()) {
            CHECK(d->id != kKerwanTimeTrial);
        }
        menu.select(0);
        menu.step(-1);
        CHECK(menu.index() == 176);
        const auto preserved = menu.current().id;
        policy.set_enabled(kKerwanTimeTrial, true);
        menu.filter(policy);
        CHECK(menu.order().size() == 178 && menu.current().id == preserved);
        std::cout
            << "178 unique entries, original ordering, bounded native selection and wrap passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

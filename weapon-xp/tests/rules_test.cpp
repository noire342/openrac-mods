// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <fstream>
#include <limits>

#include "check.hpp"
#include "combat.hpp"
#include "rules_file.hpp"

using namespace weaponxp;

int main() {
    Rules parsed{};
    std::string error;
    CHECK(parse_rules(rules_template(), parsed, error));
    CHECK(parsed[weapon_index(15)].thresholds == kThresholds);
    const std::string custom = "format=1\n[weapon.15]\nthresholds=0,100,200,300,400,500,600,700,"
                               "800,100000\nxp_multiplier=3\n";
    CHECK(parse_rules(custom, parsed, error));
    CHECK(parsed[weapon_index(15)].xp_multiplier == 3 && parsed[0].thresholds == kThresholds);
    const auto previous = parsed[weapon_index(15)].thresholds;
    for (const auto& invalid :
         {"format=2",
          "format=1\n[weapon.12]\nxp_multiplier=1",
          "format=1\n[weapon.15]\nthresholds=0,1,2",
          "format=1\n[weapon.15]\nxp_multiplier=17",
          "format=1\n[weapon.15]\nxp_multiplier=-1",
          "format=1\n[weapon.15]\nxp_multiplier=2\nxp_multiplier=3",
          "format=1\n[weapon.15]\n[weapon.15]",
          "format=1\nunknown=1",
          "format=1\n[weapon.15]\nthresholds=0,1,2,3,4,5,6,7,8,1000000001",
          "format=1\n[weapon.15]\nthresholds=0,1,1,3,4,5,6,7,8,9"}) {
        CHECK(!parse_rules(invalid, parsed, error) && !error.empty());
        CHECK(parsed[weapon_index(15)].thresholds == previous);
    }
    auto rule = parsed[0];
    rule.damage[9] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validate(rule, error));
    rule = parsed[0];
    rule.ammo[0] = 2;
    CHECK(!validate(rule, error));
    Progression state;
    state.rules = parsed;
    Settings settings;
    Combat combat(state, settings);
    combat.hit(10, 15, 1, 0);
    CHECK(combat.death(10, 10, 1).changed);
    CHECK(state.experience(15) == 30);
    state.award(15, 120);
    CHECK(state.weapon_level(15) == 2);
    state.rules[weapon_index(15)].xp_multiplier = 0;
    combat.hit(11, 15, 1, 2);
    CHECK(!combat.death(11, 100, 3).changed && state.experience(15) == 150);
    state.rules[weapon_index(15)].xp_multiplier = 3;
    state.award(15, 100000);
    CHECK(state.experience(15) == 100000);
    state.rules[weapon_index(15)].thresholds = kThresholds;
    CHECK(state.award(15, 100) == 10 && state.experience(15) == 100000);
    Progression restored;
    CHECK(decode(encode(state), restored) && restored.experience(15) == 100000);
    settings.kill_xp_cap = 7;
    CHECK(reward_quarters(500, settings, parsed[weapon_index(15)]) == 84);
    const auto root =
        std::filesystem::current_path()
        / ("rules-test-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    const auto file = root / "rules.ini";
    CHECK(read_rules_file(file, parsed, error, true));
    std::ofstream(file, std::ios::binary) << "\xEF\xBB\xBF" << custom;
    CHECK(read_rules_file(file, parsed, error, false));
    std::ofstream(file, std::ios::binary) << std::string(32769, 'x');
    CHECK(!read_rules_file(file, parsed, error, false));
    CHECK(root.parent_path() == std::filesystem::current_path());
    std::filesystem::remove_all(root);
    std::puts(
        "transactional rules, validation, per-weapon XP, custom curves and bounded files: ok"
    );
}

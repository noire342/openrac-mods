// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <fstream>

#include "check.hpp"
#include "store.hpp"

using namespace weaponxp;

int main() {
    const auto root =
        std::filesystem::current_path()
        / ("weapon-xp-store-test-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = root / "profile.wxp";
    Progression state;
    {
        Store store;
        store.open(path, state);
        state.award(0x0F, 200);
        store.save(state);
        bool rejected = false;
        try {
            Store competing;
            Progression other;
            competing.open(path, other);
        } catch (...) {
            rejected = true;
        }
        CHECK(rejected);
        state.award(0x0A, 450);
        store.save(state);  // Replaces an existing file on Windows as well as Unix.
    }
    {
        Store store;
        Progression loaded;
        store.open(path, loaded);
        CHECK(loaded.experience(0x0F) == 200 && loaded.experience(0x0A) == 450);
    }
    {
        std::ofstream(path, std::ios::trunc) << "future/corrupt format";
        bool rejected = false;
        try {
            Store store;
            Progression loaded;
            store.open(path, loaded);
        } catch (...) {
            rejected = true;
        }
        CHECK(rejected && std::filesystem::file_size(path) == 21);
    }
    for (const auto* bad : {"../save", "", "x/y", "x\\y"}) {
        bool rejected = false;
        try {
            profile_path("pal", bad);
        } catch (...) {
            rejected = true;
        }
        CHECK(rejected);
    }
    const std::string legacy = "OPENRAC_WEAPON_XP 1\n9 0\n10 0\n11 0\n13 0\n15 1500\n16 0\n17 "
                               "0\n18 0\n19 0\n20 0\n23 0\n24 0\nCHECKSUM 72421153\n";
    const auto migration = root / "legacy.wxp";
    {
        std::ofstream output(migration, std::ios::binary);
        output << legacy;
    }
    {
        Store store;
        Progression loaded;
        store.open(migration, loaded);
        CHECK(loaded.migrated && loaded.weapon_level(15) == 5);
        const auto backup = std::filesystem::path(migration).concat(".v1.bak");
        std::ifstream input(backup, std::ios::binary);
        CHECK(std::string((std::istreambuf_iterator<char>(input)), {}) == legacy);
        store.save(loaded);
    }
    {
        Store store;
        Progression loaded;
        store.open(migration, loaded);
        CHECK(!loaded.migrated && loaded.experience(15) == kThresholds[4]);
    }
    const std::string v2 = "OPENRAC_WEAPON_XP 2\n9 0 0\n10 5399 3\n11 0 0\n13 0 0\n15 6000 0\n16 0 "
                           "0\n17 0 0\n18 0 0\n19 0 0\n20 0 0\n23 0 0\n24 0 0\nCHECKSUM 3e8750f9\n";
    const auto v2path = root / "previous.wxp";
    std::ofstream(v2path, std::ios::binary) << v2;
    {
        Store store;
        Progression loaded;
        store.open(v2path, loaded);
        CHECK(loaded.migration_source == 2 && loaded.weapon_level(15) == 5);
        CHECK(loaded.experience(15) == 6000 && loaded.experience(10) == 5399);
        CHECK(loaded.quarters[weapon_index(10)] == 3);
        std::ifstream backup(std::filesystem::path(v2path).concat(".v2.bak"), std::ios::binary);
        CHECK(std::string((std::istreambuf_iterator<char>(backup)), {}) == v2);
        loaded.award(15, 4000);
        CHECK(loaded.weapon_level(15) == 6);
        store.save(loaded);
    }
    {
        Store store;
        Progression loaded;
        store.open(v2path, loaded);
        CHECK(!loaded.migrated && loaded.weapon_level(15) == 6);
        CHECK(loaded.experience(10) == 5399 && loaded.quarters[weapon_index(10)] == 3);
    }
    const std::string v3 = "OPENRAC_WEAPON_XP 3\n9 0 0\n10 120 3\n11 0 0\n13 0 0\n15 46000 0\n16 0 "
                           "0\n17 0 0\n18 0 0\n19 0 0\n20 0 0\n23 0 0\n24 0 0\nCHECKSUM 50f5dced\n";
    const auto v3path = root / "release-050.wxp";
    std::ofstream(v3path, std::ios::binary) << v3;
    {
        Store store;
        Progression loaded;
        store.open(v3path, loaded);
        CHECK(
            loaded.migration_source == 3 && loaded.experience(15) == 46000
            && loaded.weapon_level(15) == 10
        );
        CHECK(loaded.experience(10) == 120 && loaded.quarters[weapon_index(10)] == 3);
        std::ifstream backup(std::filesystem::path(v3path).concat(".v3.bak"), std::ios::binary);
        CHECK(std::string((std::istreambuf_iterator<char>(backup)), {}) == v3);
        store.save(loaded);
    }
    CHECK(root.parent_path() == std::filesystem::current_path());
    std::filesystem::remove_all(root);
    std::puts("atomic profile persistence and locking: ok");
}

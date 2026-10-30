#include <iostream>
#include <stdexcept>

#include "store.hpp"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (0)

int main() {
    try {
        using namespace skillpoints;
        Ledger a;
        CHECK(a.unlock(254397, 100));
        CHECK(!a.unlock(254397, 101));
        auto encoded = a.serialize();
        auto b = Ledger::parse(encoded);
        CHECK(b.unlocked(254397));
        CHECK(b.timestamp(254397) == 100);
        CHECK(b.earned_points() == 1);
        auto broken = encoded;
        broken[30] = 'X';
        bool rejected = false;
        try {
            Ledger::parse(broken);
        } catch (...) {
            rejected = true;
        }
        CHECK(rejected);
        const auto dir = std::filesystem::temp_directory_path() / "openrac-skill-points-tests";
        std::filesystem::create_directories(dir);
        const auto file =
            dir
            / ("test-"
               + std::to_string(
                   std::chrono::high_resolution_clock::now().time_since_epoch().count()
               )
               + ".spra");
        {
            Ledger empty;
            FileStore store(file, empty);
            store.save(a);
            bool locked = false;
            try {
                Ledger other;
                FileStore second(file, other);
            } catch (...) {
                locked = true;
            }
            CHECK(locked);
        }
        {
            Ledger restored;
            FileStore store(file, restored);
            CHECK(restored.unlocked(254397));
        }
        std::filesystem::remove(file);
        auto lock = file;
        lock += ".lock";
        std::filesystem::remove(lock);
        CHECK(profile_path("campagna-1").filename() == "ntsc-campagna-1.spra");
        bool bad = false;
        try {
            profile_path("../escape");
        } catch (...) {
            bad = true;
        }
        CHECK(bad);
        std::cout
            << "Checksum, roundtrip, atomic store, process lock and profile isolation passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <set>
#include <stdexcept>

#include "ledger.hpp"

namespace skillpoints {
inline constexpr std::uint32_t kKerwanTimeTrial = 264462;

// Selection is session configuration; permanent unlocks always remain in the ledger.
class AchievementPolicy {
public:
    bool enabled(std::uint32_t id) const { return !m_disabled.count(id); }

    void set_enabled(std::uint32_t id, bool value) {
        if (!find_definition(id)) {
            throw std::out_of_range("Unknown achievement setting");
        }
        const auto changed =
            value ? m_disabled.erase(id) : (m_disabled.insert(id).second ? 1u : 0u);
        if (changed) {
            ++m_revision;
        }
    }

    std::uint64_t revision() const { return m_revision; }

    unsigned count() const { return static_cast<unsigned>(kCatalogueSize - m_disabled.size()); }

    unsigned points() const {
        unsigned total = 0;
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            if (enabled(kCatalogue[i].id)) {
                total += kCatalogue[i].points;
            }
        }
        return total;
    }

    unsigned unlocked(const Ledger& ledger) const {
        unsigned total = 0;
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            if (enabled(kCatalogue[i].id) && ledger.unlocked(kCatalogue[i].id)) {
                ++total;
            }
        }
        return total;
    }

    unsigned earned_points(const Ledger& ledger) const {
        unsigned total = 0;
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            if (enabled(kCatalogue[i].id) && ledger.unlocked(kCatalogue[i].id)) {
                total += kCatalogue[i].points;
            }
        }
        return total;
    }

private:
    std::set<std::uint32_t> m_disabled;
    std::uint64_t m_revision = 0;
};

inline const AchievementPolicy& all_achievements() {
    static const AchievementPolicy all;
    return all;
}
}  // namespace skillpoints

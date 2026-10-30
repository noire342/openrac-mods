// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "catalogue.hpp"

namespace skillpoints {
class Ledger {
public:
    bool unlock(std::uint32_t id, std::uint64_t timestamp);

    bool unlocked(std::uint32_t id) const { return m_entries.count(id) != 0; }

    std::uint64_t timestamp(std::uint32_t id) const;

    std::size_t count() const { return m_entries.size(); }

    unsigned earned_points() const;
    std::string serialize() const;
    static Ledger parse(const std::string&);

private:
    std::map<std::uint32_t, std::uint64_t> m_entries;
};
}  // namespace skillpoints

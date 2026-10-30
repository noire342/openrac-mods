// SPDX-License-Identifier: GPL-3.0-or-later
#include "ledger.hpp"

#include <sstream>
#include <stdexcept>

namespace skillpoints {
static std::uint32_t checksum(const std::string& s) {
    std::uint32_t value = 2166136261u;
    for (unsigned char c : s) {
        value = (value ^ c) * 16777619u;
    }
    return value;
}

bool Ledger::unlock(std::uint32_t id, std::uint64_t timestamp) {
    if (!find_definition(id)) {
        throw std::invalid_argument("Unknown achievement ID");
    }
    return m_entries.emplace(id, timestamp).second;
}

std::uint64_t Ledger::timestamp(std::uint32_t id) const {
    const auto it = m_entries.find(id);
    return it == m_entries.end() ? 0 : it->second;
}

unsigned Ledger::earned_points() const {
    unsigned total = 0;
    for (const auto& entry : m_entries) {
        total += find_definition(entry.first)->points;
    }
    return total;
}

std::string Ledger::serialize() const {
    std::ostringstream s;
    s << "OPENRAC_SKILL_POINTS 1 3070\n";
    for (const auto& entry : m_entries) {
        s << entry.first << ' ' << entry.second << '\n';
    }
    const auto body = s.str();
    s << "CHECKSUM " << checksum(body) << '\n';
    return s.str();
}

Ledger Ledger::parse(const std::string& data) {
    auto invalid = [] {
        throw std::runtime_error("Invalid skill points profile; file preserved");
    };
    if (data.size() > 32768 || data.rfind("OPENRAC_SKILL_POINTS 1 3070\n", 0) != 0) {
        invalid();
    }
    const auto pos = data.rfind("CHECKSUM ");
    if (pos == std::string::npos) {
        invalid();
    }
    std::istringstream tail(data.substr(pos + 9));
    std::uint64_t expected = 0;
    std::string extra;
    if (!(tail >> expected) || (tail >> extra) || expected != checksum(data.substr(0, pos))) {
        invalid();
    }
    const std::size_t header_size = std::string("OPENRAC_SKILL_POINTS 1 3070\n").size();
    std::istringstream rows(data.substr(header_size, pos - header_size));
    Ledger ledger;
    std::uint32_t id;
    std::uint64_t at;
    std::string line;
    while (std::getline(rows, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        if (!(row >> id >> at) || (row >> extra) || !find_definition(id) || at > 4102444800ull
            || !ledger.m_entries.emplace(id, at).second) {
            invalid();
        }
    }
    return ledger;
}
}  // namespace skillpoints

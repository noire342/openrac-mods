// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

#include "achievement_policy.hpp"
#include "catalogue.hpp"

namespace skillpoints {
class Menu {
public:
    Menu();
    void filter(const AchievementPolicy&);
    void step(int delta);
    void select(std::size_t index);

    std::size_t index() const { return m_index; }

    const Definition& current() const { return *m_order.at(m_index); }

    const std::vector<const Definition*>& order() const { return m_order; }

    int native_selection() const {
        return current().native_index >= 0 ? current().native_index : 0;
    }

    bool extended() const { return current().native_index < 0; }

    std::string position() const;

private:
    std::vector<const Definition*> m_order;
    std::size_t m_index = 0;
    const AchievementPolicy* m_policy = nullptr;
    std::uint64_t m_revision = 0;
};
}  // namespace skillpoints

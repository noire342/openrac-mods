// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu.hpp"

#include <algorithm>
#include <stdexcept>

namespace skillpoints {
Menu::Menu() {
    for (int native = 0; native < 30; ++native) {
        for (std::size_t i = 0; i < kCatalogueSize; ++i) {
            if (kCatalogue[i].native_index == native) {
                m_order.push_back(&kCatalogue[i]);
            }
        }
    }
    for (std::size_t i = 0; i < kCatalogueSize; ++i) {
        if (kCatalogue[i].native_index < 0) {
            m_order.push_back(&kCatalogue[i]);
        }
    }
    if (m_order.size() != 178) {
        throw std::runtime_error("Incomplete skill points catalogue");
    }
}

void Menu::filter(const AchievementPolicy& policy) {
    if (m_policy == &policy && m_revision == policy.revision()) {
        return;
    }
    const auto selected = current().id;
    Menu all;
    std::vector<const Definition*> next;
    for (const auto* d : all.order()) {
        if (policy.enabled(d->id)) {
            next.push_back(d);
        }
    }
    if (next.empty()) {
        throw std::runtime_error("No active skill points");
    }
    m_order = std::move(next);
    m_index = std::min(m_index, m_order.size() - 1);
    for (std::size_t i = 0; i < m_order.size(); ++i) {
        if (m_order[i]->id == selected) {
            m_index = i;
        }
    }
    m_policy = &policy;
    m_revision = policy.revision();
}

void Menu::step(int delta) {
    const auto n = static_cast<int>(m_order.size());
    m_index = static_cast<std::size_t>((static_cast<int>(m_index) + delta % n + n) % n);
}

void Menu::select(std::size_t i) {
    if (i >= m_order.size()) {
        throw std::out_of_range("skill point selection");
    }
    m_index = i;
}

std::string Menu::position() const {
    return std::to_string(m_index + 1) + " / " + std::to_string(m_order.size());
}
}  // namespace skillpoints

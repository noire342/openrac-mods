// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "rc_error.h"

namespace skillpoints {
thread_local Engine* Engine::s_current = nullptr;

Engine::Engine(const IMemory& memory, Ledger& ledger, const AchievementPolicy& policy)
    : m_memory(memory),
      m_ledger(ledger),
      m_policy(policy) {
    for (std::size_t i = 0; i < kCatalogueSize; ++i) {
        const auto& d = kCatalogue[i];
        Rule rule{d.id, std::unique_ptr<rc_runtime_t, Destroy>(rc_runtime_alloc())};
        if (!rule.runtime) {
            throw std::bad_alloc();
        }
        const int result =
            rc_runtime_activate_achievement(rule.runtime.get(), d.id, d.rule, nullptr, 0);
        if (result != RC_OK) {
            throw std::runtime_error(
                "Invalid RA rule " + std::to_string(d.id) + ": " + rc_error_str(result)
            );
        }
        m_rules.push_back(std::move(rule));
    }
}

Engine::~Engine() = default;

std::uint32_t Engine::read_memory(
    std::uint32_t a, std::uint8_t* out, std::uint32_t n, void* context
) {
    auto& self = *static_cast<Engine*>(context);
    if (self.m_memory.read(a, out, n)) {
        return n;
    }
    // Never commit unlocks from a frame with an unreadable operand.
    self.m_invalid_read = true;
    std::memset(out, 0, n);
    return 0;
}

void Engine::event(const rc_runtime_event_t* e) {
    if (s_current && e->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED) {
        s_current->m_triggered.push_back(e->id);
    }
}

void Engine::tick(std::uint64_t at, const std::function<void(std::uint32_t)>& notify) {
    m_rejected_event = false;
    std::vector<std::uint32_t> pending;

    struct Scope {
        Engine* old;

        explicit Scope(Engine* e) : old(s_current) { s_current = e; }

        ~Scope() { s_current = old; }
    } scope(this);

    for (auto& rule : m_rules) {
        const bool enabled = m_policy.enabled(rule.id);
        if (rule.enabled != enabled) {
            rc_runtime_reset(rule.runtime.get());
            rule.enabled = enabled;
        }
        if (!enabled || m_ledger.unlocked(rule.id)) {
            continue;
        }
        m_triggered.clear();
        m_invalid_read = false;
        rc_runtime_do_frame(rule.runtime.get(), event, read_memory, this, nullptr);
        if (m_invalid_read) {
            // Some level-specific indirect operands are intentionally absent on
            // other planets. Their guarded/paused rule must not suppress unrelated
            // achievements. Never commit an unlock using an unreadable operand.
            if (!m_triggered.empty()) {
                m_rejected_event = true;
                rc_runtime_reset(rule.runtime.get());
            }
            continue;
        }
        pending.insert(pending.end(), m_triggered.begin(), m_triggered.end());
    }
    // Listener callbacks run after every rule has sampled the same game frame.
    for (auto id : pending) {
        if (m_ledger.unlock(id, at)) {
            notify(id);
        }
    }
}

void Engine::reset_session() {
    m_invalid_read = false;
    m_rejected_event = false;
    for (auto& r : m_rules) {
        rc_runtime_reset(r.runtime.get());
    }
}

std::pair<unsigned, unsigned> Engine::measured(std::uint32_t id) const {
    if (!m_policy.enabled(id)) {
        return {0, 0};
    }
    unsigned value = 0, target = 0;
    for (const auto& r : m_rules) {
        if (r.id == id) {
            rc_runtime_get_achievement_measured(r.runtime.get(), id, &value, &target);
            break;
        }
    }
    return {value, target};
}
}  // namespace skillpoints

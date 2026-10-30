// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "achievement_policy.hpp"
#include "ledger.hpp"
#include "rc_runtime.h"

namespace skillpoints {
class IMemory {
public:
    virtual ~IMemory() = default;
    virtual bool read(std::uint32_t address, void* output, std::uint32_t bytes) const = 0;
};

class Engine {
public:
    Engine(const IMemory&, Ledger&, const AchievementPolicy& = all_achievements());
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // All definitions must compile before any is active. Never evaluates an incomplete set.
    void tick(std::uint64_t timestamp, const std::function<void(std::uint32_t)>& unlocked);
    void reset_session();
    std::pair<unsigned, unsigned> measured(std::uint32_t id) const;

    bool healthy() const { return !m_rejected_event; }

    const AchievementPolicy& policy() const { return m_policy; }

private:
    static std::uint32_t read_memory(std::uint32_t, std::uint8_t*, std::uint32_t, void*);
    static void event(const rc_runtime_event_t*);
    const IMemory& m_memory;
    Ledger& m_ledger;
    const AchievementPolicy& m_policy;

    struct Destroy {
        void operator()(rc_runtime_t* p) const { rc_runtime_destroy(p); }
    };

    struct Rule {
        std::uint32_t id;
        std::unique_ptr<rc_runtime_t, Destroy> runtime;
        bool enabled = true;
    };

    std::vector<Rule> m_rules;
    bool m_invalid_read = false;
    bool m_rejected_event = false;
    std::vector<std::uint32_t> m_triggered;
    static thread_local Engine* s_current;
};
}  // namespace skillpoints

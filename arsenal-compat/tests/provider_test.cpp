// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "provider.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <thread>

using namespace arsenal_compat;
namespace {
unsigned checks{};
#define CHECK(condition) do { ++checks; if (!(condition)) { std::cerr << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (false)
void* pointer(std::size_t index) { return reinterpret_cast<void*>(std::uintptr_t(0x10000 + index * 0x100)); }
struct FakeResolver final : Resolver {
    std::string digest{SupportedExecutable};
    int lookups{};
    int missing{-1}, unsafe{-1}, alias{-1};
    bool hash_ok{true};
    bool executable_digest(std::string& out) override { out = digest; return hash_ok; }
    void* resolve(const char* symbol) override {
        ++lookups;
        for (std::size_t i = 0; i < hook_specs().size(); ++i)
            if (std::strcmp(hook_specs()[i].symbol, symbol) == 0)
                return static_cast<int>(i) == missing ? nullptr : pointer(static_cast<int>(i) == alias ? 0 : i);
        return nullptr;
    }
    bool main_executable_target(void* p) override { return unsafe < 0 || p != pointer(static_cast<std::size_t>(unsafe)); }
};
struct FakeBackend final : Backend {
    struct Record { bool active{}, queued{}; };
    std::map<void*, Record> owned;
    int initialized{}, creates{}, queue_calls{}, applies{}, disables{}, removes{};
    int init_failure{}, create_failure{-1}, queue_failure{-1};
    int apply_failures{};
    void* disable_failure{};
    void* remove_failure{};
    std::function<void()> reenter;
    bool foreign_active{true};
    int initialize() override { ++initialized; return init_failure; }
    int create(void* target, void*, void** original) override {
        const int index = creates++;
        if (reenter) { auto callback = std::move(reenter); callback(); }
        if (index == create_failure) return 91;
        if (!owned.emplace(target, Record{}).second) return 92;
        *original = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(target) + 16);
        return 0;
    }
    int queue_enable(void* target) override {
        if (queue_calls++ == queue_failure) return 93;
        owned.at(target).queued = true;
        return 0;
    }
    int queue_disable(void* target) override { owned.at(target).queued = false; return 0; }
    int apply() override {
        ++applies;
        unsigned i{};
        for (auto& entry : owned) {
            entry.second.active = entry.second.queued;
            if (apply_failures && ++i == 7) { --apply_failures; return 94; }
        }
        return 0;
    }
    int disable(void* target) override {
        ++disables;
        if (target == disable_failure) return 95;
        owned.at(target).active = false; owned.at(target).queued = false;
        return 0;
    }
    int remove(void* target) override {
        ++removes;
        if (target == remove_failure) return 96;
        CHECK(!owned.at(target).active);
        return owned.erase(target) == 1 ? 0 : 97;
    }
    unsigned active() const {
        unsigned count{}; for (const auto& record : owned) count += record.second.active ? 1u : 0u; return count;
    }
};
struct Fixture {
    FakeResolver resolver;
    FakeBackend backend;
    Provider provider{resolver, backend};
    void prepare() { CHECK(provider.preflight() == 0); CHECK(provider.claim("revolverator") == 0); }
    int bind(std::size_t i) {
        const auto& spec = hook_specs()[i];
        void* original{};
        const int result = provider.bind(spec.id, spec.signature, pointer(40), &original);
        if (!result) CHECK(original == reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(pointer(i)) + 16));
        else CHECK(original == nullptr);
        return result;
    }
    void bind_all() { for (std::size_t i = 0; i < hook_specs().size(); ++i) CHECK(bind(i) == 0); }
    arsenal_hooks_diagnostics_v1 diagnostics() {
        arsenal_hooks_diagnostics_v1 out{}; out.size = sizeof(out);
        CHECK(provider.diagnostics(&out) == 0); return out;
    }
};
void catalogue() {
    std::set<std::string> ids, symbols;
    CHECK(hook_specs().size() == 30);
    for (const auto& spec : hook_specs()) {
        CHECK(ids.insert(spec.id).second); CHECK(symbols.insert(spec.symbol).second);
        CHECK(Provider::owns_id(spec.id)); CHECK(std::strlen(spec.signature) > 0);
    }
    CHECK(!Provider::owns_id(nullptr)); CHECK(!Provider::owns_id("rac1.official.other"));
}
void preflight_failures() {
    { Fixture f; f.resolver.digest[0] = '0'; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD); CHECK(f.resolver.lookups == 0); CHECK(f.backend.initialized == 0); }
    { Fixture f; f.resolver.hash_ok = false; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD); CHECK(!f.provider.ready()); }
    for (int i = 0; i < 30; ++i) {
        Fixture missing; missing.resolver.missing = i;
        CHECK(missing.provider.preflight() == ARSENAL_HOOKS_UNRESOLVED);
        CHECK(missing.backend.initialized == 0); CHECK(missing.diagnostics().resolved == static_cast<unsigned>(i));
        Fixture unsafe; unsafe.resolver.unsafe = i;
        CHECK(unsafe.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); CHECK(unsafe.backend.initialized == 0);
    }
    { Fixture f; f.resolver.alias = 10; CHECK(f.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); }
    { Fixture f; f.backend.init_failure = 21; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BACKEND); CHECK(f.diagnostics().backend_status == 21); }
    { Fixture f; f.resolver.missing = 4; CHECK(f.provider.preflight() != 0); CHECK(f.provider.rollback() == 0); f.resolver.missing = -1; CHECK(f.provider.preflight() == 0); CHECK(f.diagnostics().resolved == 30); }
}
void validation_and_commit() {
    Fixture f;
    CHECK(f.bind(0) == ARSENAL_HOOKS_NOT_CLAIMED);
    CHECK(f.provider.claim("test") == ARSENAL_HOOKS_NOT_READY);
    CHECK(f.provider.preflight() == 0); CHECK(f.resolver.lookups == 30);
    CHECK(f.provider.preflight() == ARSENAL_HOOKS_NOT_READY);
    CHECK(f.provider.claim("") == ARSENAL_HOOKS_ARGUMENT);
    CHECK(f.provider.claim(std::string(64, 'a').c_str()) == ARSENAL_HOOKS_ARGUMENT);
    CHECK(f.provider.claim("revolverator") == 0);
    CHECK(f.provider.claim("another-mod") == ARSENAL_HOOKS_CONSUMER);
    CHECK(f.provider.claim("revolverator") == ARSENAL_HOOKS_CONSUMER);
    void* original = pointer(50);
    auto before = f.diagnostics();
    CHECK(f.provider.bind("rac1.official.other", "void()", pointer(40), &original) == ARSENAL_HOOKS_UNKNOWN);
    CHECK(original == nullptr); auto after = f.diagnostics(); CHECK(before.last_status == after.last_status); CHECK(before.bound == after.bound);
    const auto& spec = hook_specs()[0];
    CHECK(f.provider.bind(spec.id, "void()", pointer(40), &original) == ARSENAL_HOOKS_SIGNATURE);
    CHECK(f.provider.bind(spec.id, spec.signature, nullptr, &original) == ARSENAL_HOOKS_ARGUMENT);
    CHECK(f.provider.bind(spec.id, spec.signature, pointer(0), &original) == ARSENAL_HOOKS_ARGUMENT);
    CHECK(f.provider.commit() == ARSENAL_HOOKS_INCOMPLETE); CHECK(f.backend.active() == 0);
    CHECK(f.bind(0) == 0); CHECK(f.bind(0) == ARSENAL_HOOKS_DUPLICATE);
    CHECK(f.backend.creates == 1); CHECK(f.backend.active() == 0);
    for (std::size_t i = 1; i < 30; ++i) CHECK(f.bind(i) == 0);
    CHECK(f.backend.applies == 0); CHECK(f.provider.commit() == 0); CHECK(f.backend.applies == 1); CHECK(f.backend.active() == 30);
    CHECK(f.diagnostics().active == 30); CHECK(f.diagnostics().stage == ARSENAL_HOOKS_ACTIVE);
    CHECK(f.provider.commit() == ARSENAL_HOOKS_COMMITTED);
    CHECK(f.bind(4) == ARSENAL_HOOKS_COMMITTED);
    CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    CHECK(f.diagnostics().stage == ARSENAL_HOOKS_READY); CHECK(f.diagnostics().consumer[0] == 0);
    CHECK(f.provider.claim("next-session") == 0); CHECK(f.bind(0) == 0); CHECK(f.provider.rollback() == 0);
}
void transactional_failures() {
    for (int fail = 0; fail < 30; ++fail) {
        Fixture f; f.prepare(); f.backend.create_failure = fail;
        for (int i = 0; i < fail; ++i) CHECK(f.bind(static_cast<std::size_t>(i)) == 0);
        CHECK(f.bind(static_cast<std::size_t>(fail)) == ARSENAL_HOOKS_BACKEND);
        CHECK(f.backend.active() == 0); CHECK(f.provider.commit() == ARSENAL_HOOKS_NOT_READY);
        CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    }
    for (int fail = 0; fail < 30; ++fail) {
        Fixture f; f.prepare(); f.bind_all(); f.backend.queue_failure = fail;
        CHECK(f.provider.commit() == ARSENAL_HOOKS_BACKEND);
        CHECK(f.backend.owned.empty()); CHECK(f.backend.active() == 0); CHECK(f.backend.foreign_active);
        CHECK(f.provider.rollback() == 0);
    }
    { Fixture f; f.prepare(); f.bind_all(); f.backend.apply_failures = 1;
      CHECK(f.provider.commit() == ARSENAL_HOOKS_BACKEND); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active); }
    { Fixture f; f.prepare(); f.bind_all(); CHECK(f.provider.commit() == 0);
      f.backend.disable_failure = pointer(4); CHECK(f.provider.rollback() == ARSENAL_HOOKS_ROLLBACK);
      CHECK(f.backend.owned.size() == 1); CHECK(f.backend.owned.count(pointer(4)) == 1);
      CHECK(f.provider.claim("another") == ARSENAL_HOOKS_NOT_READY);
      f.backend.disable_failure = nullptr; CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); }
    { Fixture f; f.prepare(); CHECK(f.bind(0) == 0); f.backend.remove_failure = pointer(0);
      CHECK(f.provider.rollback() == ARSENAL_HOOKS_ROLLBACK); CHECK(f.backend.owned.size() == 1);
      f.backend.remove_failure = nullptr; CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); }
}
void reentry_and_thread() {
    Fixture f; f.prepare();
    f.backend.reenter = [&] { CHECK(f.provider.rollback() == ARSENAL_HOOKS_BUSY); CHECK(f.provider.commit() == ARSENAL_HOOKS_BUSY); };
    CHECK(f.bind(0) == 0); CHECK(f.backend.owned.size() == 1);
    std::thread thread([&] { CHECK(f.provider.claim("foreign-thread") == ARSENAL_HOOKS_THREAD); CHECK(f.provider.rollback() == ARSENAL_HOOKS_THREAD); }); thread.join();
    CHECK(f.backend.owned.size() == 1); CHECK(f.provider.rollback() == 0);
    CHECK(f.provider.diagnostics(nullptr) == ARSENAL_HOOKS_ARGUMENT);
    arsenal_hooks_diagnostics_v1 small{}; small.size = 4; CHECK(f.provider.diagnostics(&small) == ARSENAL_HOOKS_ARGUMENT);
}
}
int main() {
    catalogue(); preflight_failures(); validation_and_commit(); transactional_failures(); reentry_and_thread();
    std::cout << checks << " arsenal provider checks passed\n";
}

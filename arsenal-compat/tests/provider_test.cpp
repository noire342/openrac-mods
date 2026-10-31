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
#include <vector>

using namespace arsenal_compat;
namespace {
static_assert(OPENRAC_ARSENAL_REQUIRED_HOOKS == 30u, "The ABI v1 required prefix must stay compatible");
constexpr std::size_t OptionalHook = OPENRAC_ARSENAL_REQUIRED_HOOKS;
Region testing_region = Region::Ntsc;
unsigned checks{};
#define CHECK(condition) do { ++checks; if (!(condition)) { std::cerr << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (false)
void* pointer(std::size_t index) { return reinterpret_cast<void*>(std::uintptr_t(0x10000 + index * 0x100)); }
struct FakeResolver final : Resolver {
    std::string digest{SupportedExecutable};
    std::string disc{"ntsc"};
    bool null_version{};
    std::vector<std::string> requested;
    int lookups{};
    int missing{-1}, unsafe{-1}, alias{-1};
    bool hash_ok{true};
    const char* version() const noexcept override { return null_version ? nullptr : disc.c_str(); }
    bool executable_digest(std::string& out) override { out = digest; return hash_ok; }
    void* resolve(const char* symbol) override {
        ++lookups;
        requested.emplace_back(symbol);
        const auto& specs = hook_specs(disc == "pal" ? Region::Pal : Region::Ntsc);
        for (std::size_t i = 0; i < specs.size(); ++i)
            if (std::strcmp(specs[i].symbol, symbol) == 0)
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
    explicit Fixture(Region region = testing_region) {
        if (region == Region::Pal) { resolver.disc = "pal"; resolver.digest = SupportedPalExecutable; }
    }
    void prepare() { CHECK(provider.preflight() == 0); CHECK(provider.claim("revolverator") == 0); }
    int bind(std::size_t i) {
        const auto& spec = hook_specs(resolver.disc == "pal" ? Region::Pal : Region::Ntsc)[i];
        void* original{};
        const int result = provider.bind(spec.id, spec.signature, pointer(40), &original);
        if (!result) CHECK(original == reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(pointer(i)) + 16));
        else CHECK(original == nullptr);
        return result;
    }
    void bind_required() { for (std::size_t i = 0; i < OPENRAC_ARSENAL_REQUIRED_HOOKS; ++i) CHECK(bind(i) == 0); }
    void bind_all() { bind_required(); for (std::size_t i = OptionalHook; i < hook_specs().size(); ++i) CHECK(bind(i) == 0); }
    arsenal_hooks_diagnostics_v1 diagnostics() {
        arsenal_hooks_diagnostics_v1 out{}; out.size = sizeof(out);
        CHECK(provider.diagnostics(&out) == 0); return out;
    }
};
void catalogue() {
    std::set<std::string> ids, symbols;
    CHECK(hook_specs().size() == 31);
    CHECK(std::strcmp(hook_specs()[OptionalHook].id, "rac1.vendor.icon_strip.draw") == 0);
    CHECK(std::strcmp(hook_specs()[OptionalHook].signature, "void()") == 0);
    CHECK(std::strcmp(hook_specs()[OptionalHook].symbol, "FUN_002386e8") == 0);
    for (const auto& spec : hook_specs()) {
        CHECK(ids.insert(spec.id).second); CHECK(symbols.insert(spec.symbol).second);
        CHECK(Provider::owns_id(spec.id)); CHECK(std::strlen(spec.signature) > 0);
    }
    CHECK(!Provider::owns_id(nullptr)); CHECK(!Provider::owns_id("rac1.official.other"));
    CHECK(&hook_specs() == &hook_specs(Region::Ntsc));
    static constexpr std::array<const char*, HookCatalogSize> expected_pal{{
        "func_0021C840", "func_0021E950", "func_0021D7A0", "func_00221B58",
        "func_0021D9C8", "func_0021DA60", "func_0021B298", "func_00220160",
        "func_0021F238", "func_0021EFA0", "func_0021F610", "func_0021F118",
        "func_002255F8", "func_0021EE00", "func_00225E00", "func_L00_00238478",
        "func_L00_00238530", "func_L00_00238C30", "func_L00_0029CDE0", "func_L00_0029C9E8",
        "func_L00_00258250", "func_L00_002C6608", "func_00227C78", "func_0020BFC8",
        "func_00209CE8", "func_0020BD70", "func_00209DC0", "func_00227DB0",
        "func_0020BA00", "func_00209E68", "func_00239A00"
    }};
    std::set<std::string> pal_symbols;
    for (std::size_t i = 0; i < HookCatalogSize; ++i) {
        const auto& pal = hook_specs(Region::Pal)[i];
        CHECK(std::strcmp(pal.symbol, expected_pal[i]) == 0); CHECK(pal_symbols.insert(pal.symbol).second);
        CHECK(std::strcmp(pal.id, hook_specs()[i].id) == 0);
        CHECK(std::strcmp(pal.signature, hook_specs()[i].signature) == 0);
        CHECK(Provider::owns_id(pal.id));
    }
}
void region_validation() {
    const std::array<const char*, 3> hashes{{SupportedExecutables[0], SupportedExecutables[1], SupportedPalExecutable}};
    for (const auto region : {Region::Ntsc, Region::Pal}) {
        for (const auto* hash : hashes) {
            Fixture f(region); f.resolver.digest = hash;
            const bool valid = region == Region::Pal ? std::strcmp(hash, SupportedPalExecutable) == 0 :
                std::strcmp(hash, SupportedPalExecutable) != 0;
            CHECK(f.provider.preflight() == (valid ? ARSENAL_HOOKS_OK : ARSENAL_HOOKS_BUILD));
            CHECK(f.provider.ready() == valid);
            CHECK(f.resolver.lookups == (valid ? 31 : 0)); CHECK(f.backend.initialized == (valid ? 1 : 0));
            if (valid) {
                for (std::size_t i = 0; i < HookCatalogSize; ++i)
                    CHECK(f.resolver.requested[i] == hook_specs(region)[i].symbol);
                CHECK(std::strcmp(f.diagnostics().executable_sha256, hash) == 0);
            }
        }
        Fixture unknown(region); unknown.resolver.digest.assign(64, '0');
        CHECK(unknown.provider.preflight() == ARSENAL_HOOKS_BUILD);
        CHECK(unknown.resolver.lookups == 0); CHECK(unknown.backend.initialized == 0);
    }
    for (const char* version : {"", "PAL", "pal_v1", "jp", "ntsc-j"}) {
        for (const auto* hash : hashes) {
            Fixture f; f.resolver.disc = version; f.resolver.digest = hash;
            CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD);
            CHECK(!f.provider.ready()); CHECK(f.resolver.lookups == 0); CHECK(f.backend.initialized == 0);
        }
    }
    { Fixture f; f.resolver.null_version = true; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD);
      CHECK(f.resolver.lookups == 0); CHECK(f.backend.initialized == 0); }
    // A resolver compiled without a version override keeps its legacy NTSC contract.
    struct LegacyResolver final : Resolver {
        FakeResolver delegate;
        bool executable_digest(std::string& out) override { return delegate.executable_digest(out); }
        void* resolve(const char* symbol) override { return delegate.resolve(symbol); }
        bool main_executable_target(void* address) override { return delegate.main_executable_target(address); }
    } legacy;
    FakeBackend backend; Provider provider(legacy, backend);
    CHECK(std::strcmp(legacy.version(), "ntsc") == 0); CHECK(provider.preflight() == ARSENAL_HOOKS_OK);
}
void preflight_failures() {
    const std::vector<const char*> digests = testing_region == Region::Pal ?
        std::vector<const char*>{SupportedPalExecutable} :
        std::vector<const char*>{SupportedExecutables.begin(), SupportedExecutables.end()};
    for (const auto* digest : digests) {
        Fixture f; f.resolver.digest = digest;
        CHECK(f.provider.preflight() == ARSENAL_HOOKS_OK);
        CHECK(f.resolver.lookups == 31);
        CHECK(f.provider.ready()); CHECK(f.diagnostics().required == 30); CHECK(f.diagnostics().resolved == 30);
        CHECK(std::strcmp(f.diagnostics().executable_sha256, digest) == 0);
    }
    { Fixture f; f.resolver.digest[0] = '0'; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD); CHECK(f.resolver.lookups == 0); CHECK(f.backend.initialized == 0); }
    { Fixture f; f.resolver.hash_ok = false; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BUILD); CHECK(!f.provider.ready()); }
    for (int i = 0; i < static_cast<int>(OPENRAC_ARSENAL_REQUIRED_HOOKS); ++i) {
        Fixture missing; missing.resolver.missing = i;
        CHECK(missing.provider.preflight() == ARSENAL_HOOKS_UNRESOLVED);
        CHECK(missing.backend.initialized == 0); CHECK(missing.diagnostics().resolved == static_cast<unsigned>(i));
        Fixture unsafe; unsafe.resolver.unsafe = i;
        CHECK(unsafe.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); CHECK(unsafe.backend.initialized == 0);
    }
    { Fixture f; f.resolver.unsafe = static_cast<int>(OptionalHook);
      CHECK(f.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); CHECK(!f.provider.ready()); CHECK(f.backend.initialized == 0); }
    { Fixture f; f.resolver.alias = static_cast<int>(OptionalHook);
      CHECK(f.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); CHECK(!f.provider.ready()); CHECK(f.backend.initialized == 0); }
    { Fixture f; f.resolver.alias = 10; CHECK(f.provider.preflight() == ARSENAL_HOOKS_UNSAFE_TARGET); }
    { Fixture f; f.backend.init_failure = 21; CHECK(f.provider.preflight() == ARSENAL_HOOKS_BACKEND); CHECK(f.diagnostics().backend_status == 21); }
    { Fixture f; f.resolver.missing = 4; CHECK(f.provider.preflight() != 0); CHECK(f.provider.rollback() == 0); f.resolver.missing = -1; CHECK(f.provider.preflight() == 0); CHECK(f.diagnostics().resolved == 30); }
}
void validation_and_commit() {
    Fixture f;
    CHECK(f.bind(0) == ARSENAL_HOOKS_NOT_CLAIMED);
    CHECK(f.provider.claim("test") == ARSENAL_HOOKS_NOT_READY);
    CHECK(f.provider.preflight() == 0); CHECK(f.resolver.lookups == 31);
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
    for (std::size_t i = 1; i < OPENRAC_ARSENAL_REQUIRED_HOOKS; ++i) CHECK(f.bind(i) == 0);
    CHECK(f.backend.applies == 0); CHECK(f.provider.commit() == 0); CHECK(f.backend.applies == 1); CHECK(f.backend.active() == 30);
    CHECK(f.diagnostics().bound == 30); CHECK(f.diagnostics().active == 30); CHECK(f.diagnostics().stage == ARSENAL_HOOKS_ACTIVE);
    CHECK(f.backend.creates == 30); CHECK(f.backend.queue_calls == 30); CHECK(f.backend.owned.count(pointer(OptionalHook)) == 0);
    CHECK(f.provider.commit() == ARSENAL_HOOKS_COMMITTED);
    CHECK(f.bind(4) == ARSENAL_HOOKS_COMMITTED);
    CHECK(f.bind(OptionalHook) == ARSENAL_HOOKS_COMMITTED);
    CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    CHECK(f.diagnostics().stage == ARSENAL_HOOKS_READY); CHECK(f.diagnostics().consumer[0] == 0);
    CHECK(f.provider.claim("next-session") == 0); CHECK(f.bind(0) == 0); CHECK(f.provider.rollback() == 0);
}
void optional_bindings() {
    // A legacy consumer's 30-hook transaction remains valid even if the
    // optional symbol is missing from an otherwise supported executable.
    {
        Fixture f; f.resolver.missing = static_cast<int>(OptionalHook); f.prepare();
        CHECK(f.provider.ready()); CHECK(f.resolver.lookups == 31);
        CHECK(f.diagnostics().required == 30); CHECK(f.diagnostics().resolved == 30);
        CHECK(f.bind(OptionalHook) == ARSENAL_HOOKS_UNRESOLVED);
        CHECK(f.diagnostics().stage == ARSENAL_HOOKS_COLLECTING); CHECK(f.backend.creates == 0);
        CHECK(f.diagnostics().bound == 0); CHECK(f.diagnostics().active == 0);
        f.bind_required(); CHECK(f.provider.commit() == 0);
        CHECK(f.backend.active() == 30); CHECK(f.backend.queue_calls == 30);
        CHECK(f.backend.owned.count(pointer(OptionalHook)) == 0); CHECK(f.provider.ready());
        CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    }
    // New consumers may bind the optional hook in any order; diagnostics
    // count every detour while the required prefix stays at 30.
    {
        Fixture f; f.prepare();
        const auto& spec = hook_specs()[OptionalHook]; void* original = pointer(50);
        CHECK(f.provider.bind(spec.id, "void(u32)", pointer(40), &original) == ARSENAL_HOOKS_SIGNATURE);
        CHECK(original == nullptr); CHECK(f.backend.creates == 0);
        CHECK(f.bind(OptionalHook) == 0); CHECK(f.bind(OptionalHook) == ARSENAL_HOOKS_DUPLICATE);
        CHECK(f.diagnostics().bound == 1); CHECK(f.diagnostics().active == 0);
        CHECK(f.provider.commit() == ARSENAL_HOOKS_INCOMPLETE); CHECK(f.backend.queue_calls == 0);
        f.bind_required(); CHECK(f.diagnostics().bound == 31); CHECK(f.diagnostics().resolved == 30);
        CHECK(f.provider.commit() == 0); CHECK(f.backend.active() == 31); CHECK(f.backend.queue_calls == 31);
        CHECK(f.diagnostics().bound == 31); CHECK(f.diagnostics().active == 31); CHECK(f.provider.ready());
        CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
        CHECK(f.diagnostics().bound == 0); CHECK(f.diagnostics().active == 0); CHECK(f.provider.ready());
        CHECK(f.provider.claim("legacy-after-new") == 0); f.bind_required();
        CHECK(f.provider.commit() == 0); CHECK(f.backend.active() == 30);
        CHECK(f.backend.owned.count(pointer(OptionalHook)) == 0); CHECK(f.provider.rollback() == 0);
    }
    // Counting 30 bindings is insufficient: the optional hook must never
    // stand in for any of the 30 required hooks.
    for (std::size_t missing = 0; missing < OPENRAC_ARSENAL_REQUIRED_HOOKS; ++missing) {
        Fixture f; f.prepare();
        for (std::size_t i = 0; i < hook_specs().size(); ++i) if (i != missing) CHECK(f.bind(i) == 0);
        CHECK(f.diagnostics().bound == 30);
        CHECK(f.provider.commit() == ARSENAL_HOOKS_INCOMPLETE);
        CHECK(f.backend.active() == 0); CHECK(f.backend.queue_calls == 0); CHECK(f.backend.applies == 0);
        CHECK(f.bind(missing) == 0); CHECK(f.provider.commit() == 0); CHECK(f.backend.active() == 31);
        CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    }
}
void transactional_failures() {
    for (int fail = 0; fail < static_cast<int>(hook_specs().size()); ++fail) {
        Fixture f; f.prepare(); f.backend.create_failure = fail;
        for (int i = 0; i < fail; ++i) CHECK(f.bind(static_cast<std::size_t>(i)) == 0);
        CHECK(f.bind(static_cast<std::size_t>(fail)) == ARSENAL_HOOKS_BACKEND);
        CHECK(f.backend.active() == 0); CHECK(f.provider.commit() == ARSENAL_HOOKS_NOT_READY);
        CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); CHECK(f.backend.foreign_active);
    }
    for (int fail = 0; fail < static_cast<int>(hook_specs().size()); ++fail) {
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
    { Fixture f; f.prepare(); f.bind_all(); CHECK(f.provider.commit() == 0);
      f.backend.disable_failure = pointer(OptionalHook); CHECK(f.provider.rollback() == ARSENAL_HOOKS_ROLLBACK);
      CHECK(f.backend.owned.size() == 1); CHECK(f.backend.owned.count(pointer(OptionalHook)) == 1);
      CHECK(f.diagnostics().bound == 1); CHECK(!f.provider.ready());
      f.backend.disable_failure = nullptr; CHECK(f.provider.rollback() == 0); CHECK(f.backend.owned.empty()); }
    { Fixture f; f.prepare(); CHECK(f.bind(OptionalHook) == 0); f.backend.remove_failure = pointer(OptionalHook);
      CHECK(f.provider.rollback() == ARSENAL_HOOKS_ROLLBACK); CHECK(f.backend.owned.size() == 1);
      CHECK(f.diagnostics().bound == 1); CHECK(f.diagnostics().active == 0);
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
    catalogue(); region_validation();
    for (const auto region : {Region::Ntsc, Region::Pal}) {
        testing_region = region;
        preflight_failures(); validation_and_commit(); optional_bindings(); transactional_failures(); reentry_and_thread();
    }
    std::cout << checks << " arsenal provider checks passed\n";
}

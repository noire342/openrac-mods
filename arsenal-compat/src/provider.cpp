// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "provider.hpp"
#include <algorithm>
#include <cstring>

namespace arsenal_compat {
namespace {
template<std::size_t N> void copy(char (&out)[N], const char* in) noexcept {
    if (!in) in = "";
    std::size_t i{};
    for (; i + 1 < N && in[i]; ++i) out[i] = in[i];
    out[i] = '\0';
}
bool bounded(const char* value, std::size_t maximum) noexcept {
    if (!value || !*value) return false;
    for (std::size_t i = 0; i <= maximum; ++i) if (!value[i]) return true;
    return false;
}
struct Scope {
    bool& busy;
    explicit Scope(bool& value) : busy(value) { busy = true; }
    ~Scope() { busy = false; }
};
}

Provider::Provider(Resolver& resolver, Backend& backend)
    : resolver_(resolver), backend_(backend), thread_(std::this_thread::get_id()) {
    diagnostic_.size = sizeof(diagnostic_);
    diagnostic_.version = OPENRAC_ARSENAL_HOOKS_ABI_V1;
    diagnostic_.required = OPENRAC_ARSENAL_REQUIRED_HOOKS;
}
Provider::~Provider() { rollback(); }
bool Provider::enter() noexcept { return !busy_ && std::this_thread::get_id() == thread_; }
int Provider::fail(int result, const char* text, const char* id, int backend) noexcept {
    diagnostic_.last_status = result;
    diagnostic_.backend_status = backend;
    copy(diagnostic_.message, text);
    copy(diagnostic_.hook_id, id);
    return result;
}
bool Provider::owns_id(const char* id) noexcept {
    if (!bounded(id, 95)) return false;
    return std::any_of(hook_specs().begin(), hook_specs().end(),
        [id](const HookSpec& hook) { return std::strcmp(id, hook.id) == 0; });
}
bool Provider::ready() const noexcept {
    return diagnostic_.resolved == OPENRAC_ARSENAL_REQUIRED_HOOKS &&
        diagnostic_.stage != ARSENAL_HOOKS_COLD && diagnostic_.stage != ARSENAL_HOOKS_FAILED;
}
int Provider::preflight() {
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (!enter()) return ARSENAL_HOOKS_BUSY;
    Scope guard(busy_);
    if (diagnostic_.stage != ARSENAL_HOOKS_COLD)
        return fail(ARSENAL_HOOKS_NOT_READY, "Preflight is allowed once before binding");
    diagnostic_.stage = ARSENAL_HOOKS_FAILED;
    diagnostic_.resolved = 0;
    bindings_ = {};
    const char* version = resolver_.version();
    if (!version || (std::strcmp(version, "ntsc") && std::strcmp(version, "pal")))
        return fail(ARSENAL_HOOKS_BUILD, "Unsupported disc version: arsenal-compat supports PAL and NTSC-U only");
    region_ = std::strcmp(version, "pal") == 0 ? Region::Pal : Region::Ntsc;
    std::string digest;
    if (!resolver_.executable_digest(digest))
        return fail(ARSENAL_HOOKS_BUILD, "Cannot hash the running OpenRAC executable");
    copy(diagnostic_.executable_sha256, digest.c_str());
    const bool supported = region_ == Region::Pal ? digest == SupportedPalExecutable :
        std::any_of(SupportedExecutables.begin(), SupportedExecutables.end(),
            [&digest](const char* known) { return digest == known; });
    if (!supported)
        return fail(ARSENAL_HOOKS_BUILD, "Unsupported OpenRAC build/disc pair: expected the documented PAL 0.10.0 or NTSC-U 0.9.0/0.10.0 executable");
    const auto& specs = hook_specs(region_);
    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        const auto& spec = specs[i];
        auto* target = resolver_.resolve(spec.symbol);
        if (!target) {
            if (i >= OPENRAC_ARSENAL_REQUIRED_HOOKS) continue;
            return fail(ARSENAL_HOOKS_UNRESOLVED, "Required native symbol is unavailable", spec.id);
        }
        if (!resolver_.main_executable_target(target))
            return fail(ARSENAL_HOOKS_UNSAFE_TARGET, "Native symbol does not point into executable main-module code", spec.id);
        for (std::size_t j = 0; j < i; ++j)
            if (bindings_[j].target == target)
                return fail(ARSENAL_HOOKS_UNSAFE_TARGET, "Two hook IDs resolve to the same native function", spec.id);
        bindings_[i].target = target;
        // ABI v1 reports readiness of its required prefix; optional bindings
        // are reflected in bound/active only when a consumer requests them.
        if (i < OPENRAC_ARSENAL_REQUIRED_HOOKS) ++diagnostic_.resolved;
    }
    const int status = backend_.initialize();
    if (status) return fail(ARSENAL_HOOKS_BACKEND, "Cannot initialize the owned detour backend", "", status);
    backend_ready_ = true;
    diagnostic_.stage = ARSENAL_HOOKS_READY;
    return fail(ARSENAL_HOOKS_OK, region_ == Region::Pal ?
        "Verified stock PAL executable; all 30 required native targets are ready" :
        "Verified stock NTSC-U executable; all 30 required native targets are ready");
}
int Provider::claim(const char* consumer) {
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (!enter()) return ARSENAL_HOOKS_BUSY;
    Scope guard(busy_);
    if (!bounded(consumer, 63)) return fail(ARSENAL_HOOKS_ARGUMENT, "Consumer ID must contain 1..63 bytes");
    if (!ready()) return fail(ARSENAL_HOOKS_NOT_READY, "Provider preflight did not succeed");
    if (claimed_) return fail(ARSENAL_HOOKS_CONSUMER, "This provider permits one consumer; an existing claim must be rolled back first");
    claimed_ = true;
    copy(diagnostic_.consumer, consumer);
    diagnostic_.stage = ARSENAL_HOOKS_COLLECTING;
    return fail(ARSENAL_HOOKS_OK, "Consumer claimed; hooks remain disabled");
}
int Provider::bind(const char* id, const char* signature, void* replacement, void** original) {
    if (original) *original = nullptr;
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (!enter()) return ARSENAL_HOOKS_BUSY;
    Scope guard(busy_);
    if (!bounded(id, 95) || !bounded(signature, 63) || !replacement || !original)
        return fail(ARSENAL_HOOKS_ARGUMENT, "bind requires an ID, exact signature, replacement and original output");
    const auto& specs = hook_specs(region_);
    const auto found = std::find_if(specs.begin(), specs.end(),
        [id](const HookSpec& spec) { return std::strcmp(id, spec.id) == 0; });
    if (found == specs.end()) return ARSENAL_HOOKS_UNKNOWN; // Pure fallback signal.
    if (std::strcmp(signature, found->signature))
        return fail(ARSENAL_HOOKS_SIGNATURE, "Native hook signature mismatch", id);
    if (!claimed_) return fail(ARSENAL_HOOKS_NOT_CLAIMED, "Claim the provider before binding", id);
    if (diagnostic_.stage == ARSENAL_HOOKS_ACTIVE)
        return fail(ARSENAL_HOOKS_COMMITTED, "The active transaction cannot accept more bindings", id);
    if (diagnostic_.stage != ARSENAL_HOOKS_COLLECTING)
        return fail(ARSENAL_HOOKS_NOT_READY, "Transaction must be rolled back before retry", id);
    auto& binding = bindings_[static_cast<std::size_t>(found - specs.begin())];
    if (!binding.target)
        return fail(ARSENAL_HOOKS_UNRESOLVED, "Optional native symbol is unavailable", id);
    if (binding.created) return fail(ARSENAL_HOOKS_DUPLICATE, "Hook ID already bound", id);
    if (replacement == binding.target)
        return fail(ARSENAL_HOOKS_ARGUMENT, "Replacement cannot be its own native target", id);
    const int status = backend_.create(binding.target, replacement, &binding.trampoline);
    if (status || !binding.trampoline) {
        diagnostic_.stage = ARSENAL_HOOKS_FAILED;
        return fail(ARSENAL_HOOKS_BACKEND, "Cannot prepare disabled native detour", id, status);
    }
    binding.created = true;
    ++diagnostic_.bound;
    *original = binding.trampoline;
    return fail(ARSENAL_HOOKS_OK, "Disabled detour prepared", id);
}
int Provider::commit() {
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (!enter()) return ARSENAL_HOOKS_BUSY;
    Scope guard(busy_);
    if (diagnostic_.stage == ARSENAL_HOOKS_ACTIVE)
        return fail(ARSENAL_HOOKS_COMMITTED, "Transaction is already active");
    if (!claimed_) return fail(ARSENAL_HOOKS_NOT_CLAIMED, "Claim the provider before commit");
    if (diagnostic_.stage != ARSENAL_HOOKS_COLLECTING)
        return fail(ARSENAL_HOOKS_NOT_READY, "Transaction failed and requires rollback");
    if (!std::all_of(bindings_.begin(), bindings_.begin() + OPENRAC_ARSENAL_REQUIRED_HOOKS,
            [](const Binding& binding) { return binding.created; }))
        return fail(ARSENAL_HOOKS_INCOMPLETE, "Commit requires all 30 typed bindings");
    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        auto& binding = bindings_[i];
        if (!binding.created) continue;
        const int status = backend_.queue_enable(binding.target);
        if (status) {
            diagnostic_.stage = ARSENAL_HOOKS_FAILED;
            const int restored = cleanup();
            return fail(restored ? ARSENAL_HOOKS_ROLLBACK : ARSENAL_HOOKS_BACKEND,
                "Detour queue failed; owned transaction rolled back", hook_specs(region_)[i].id, status);
        }
        binding.may_be_active = true;
    }
    const int status = backend_.apply();
    if (status) {
        diagnostic_.stage = ARSENAL_HOOKS_FAILED;
        const int restored = cleanup();
        return fail(restored ? ARSENAL_HOOKS_ROLLBACK : ARSENAL_HOOKS_BACKEND,
            "Detour activation failed; owned transaction rolled back", "", status);
    }
    diagnostic_.active = diagnostic_.bound;
    diagnostic_.stage = ARSENAL_HOOKS_ACTIVE;
    return fail(ARSENAL_HOOKS_OK, "All requested typed detours activated in one queued batch");
}
int Provider::cleanup() noexcept {
    // Disable each owned target even if batch apply only partly succeeded.
    // Never remove a trampoline whose target could still execute its detour.
    bool failed = false;
    bool queued = false;
    for (auto& binding : bindings_) if (binding.created && binding.may_be_active) {
        if (backend_.queue_disable(binding.target)) failed = true;
        else queued = true;
    }
    if (queued && backend_.apply()) failed = true;
    for (auto& binding : bindings_) if (binding.created) {
        const int disabled = backend_.disable(binding.target);
        if (disabled) { failed = true; continue; }
        binding.may_be_active = false;
        if (backend_.remove(binding.target)) { failed = true; continue; }
        binding.created = false;
        binding.trampoline = nullptr;
    }
    diagnostic_.bound = static_cast<uint32_t>(std::count_if(bindings_.begin(), bindings_.end(),
        [](const Binding& binding) { return binding.created; }));
    diagnostic_.active = static_cast<uint32_t>(std::count_if(bindings_.begin(), bindings_.end(),
        [](const Binding& binding) { return binding.created && binding.may_be_active; }));
    // Batch errors recovered by individual disable/remove do not retain a failure.
    if (!diagnostic_.bound) failed = false;
    return failed ? ARSENAL_HOOKS_ROLLBACK : ARSENAL_HOOKS_OK;
}
int Provider::rollback() {
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (!enter()) return ARSENAL_HOOKS_BUSY;
    Scope guard(busy_);
    if (backend_ready_ && cleanup()) {
        diagnostic_.stage = ARSENAL_HOOKS_FAILED;
        return fail(ARSENAL_HOOKS_ROLLBACK, "Cannot detach every owned hook; do not unload the consumer");
    }
    claimed_ = false;
    diagnostic_.consumer[0] = 0;
    diagnostic_.stage = backend_ready_ ? ARSENAL_HOOKS_READY : ARSENAL_HOOKS_COLD;
    return fail(ARSENAL_HOOKS_OK, "Owned hooks removed; consumer claim released");
}
int Provider::diagnostics(arsenal_hooks_diagnostics_v1* out) const {
    if (std::this_thread::get_id() != thread_) return ARSENAL_HOOKS_THREAD;
    if (busy_) return ARSENAL_HOOKS_BUSY;
    if (!out || out->size < sizeof(*out)) return ARSENAL_HOOKS_ARGUMENT;
    *out = diagnostic_;
    return ARSENAL_HOOKS_OK;
}
} // namespace arsenal_compat

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "native_arsenal.hpp"
#include "weapon_xp_api.h"
#include <array>
#include <cstdint>

namespace revolverator {

// Owns only the logical Revolverator's progression. The optional XP provider
// supplies balancing/configuration and excludes these hits from native credit.
// No provider module is loaded by this adapter.
class NativeArsenalXP final:public Memory {
public:
    using ProviderResolver=weapon_xp_query_fn(*)();
    struct Stats {
        unsigned level=1,xp=0,level_start_xp=0,next_level_xp=400;
        float progress=0,damage_multiplier=1,ammo_multiplier=1;
    };
    struct LevelUp {unsigned old_level{},new_level{},xp{};};
    NativeArsenalXP(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal,
                    ProviderResolver resolver=nullptr);
    ~NativeArsenalXP();
    NativeArsenalXP(const NativeArsenalXP&)=delete;
    NativeArsenalXP& operator=(const NativeArsenalXP&)=delete;

    void frame();
    // Refresh cached ownership before calling the inner GetHit chain. A quick
    // select change may have happened since the last normal frame callback.
    void before_hit();
    // Call only after the outer GetHit wrapper has accepted/suppressed the hit.
    void confirm_hit(unsigned target,unsigned hit);
    void block_hit(unsigned target)noexcept;
    // Native reward bookkeeping must still be intact: before RecordDeath.
    void death_before(unsigned target);
    bool poll_level_up(LevelUp& event)noexcept;
    Stats get_stats()const;
    bool ready()const{return provider_&&configuration_valid_;}
    bool compatible()const{return !incompatible_;}
    bool provider_present()const{return present_;}
    // Optional v4 tail: call from normal frames, outside provider callbacks.
    // Preserve result codes (especially BUSY versus DISABLED/NOT_FOUND).
    int request_acquisition_presentation(int native_item=18);
    int acquisition_presentation_active()const;
    unsigned flags()const{return ready()?configuration_.flags:0;}
    const weapon_xp_configuration_v3& configuration()const{return configuration_;}
    const weapon_xp_rules_v3& rules()const{return rules_;}
    // Level travel retains fractional XP. Save-slot/new-game resets discard it.
    void reset(bool clear_fraction=false)noexcept;

private:
    static constexpr unsigned MaxReceipts=256,ReceiptLifetime=120,MaxEvents=64;
    struct Identity {unsigned moby{},pvars{},klass{},stamp{};};
    struct Pending {weapon_xp_hit_context_v4 context{};std::uint64_t at{};bool valid=false;};
    struct Accepted {
        Identity identity{};std::array<unsigned char,64> payload{};
        unsigned hit{},epoch{};std::uint64_t at{};
        bool valid=false,credited=false;
    };
    openrac_mod* mod_;
    NativeArsenal& arsenal_;
    ProviderResolver resolver_;
    weapon_xp_query_fn query_{};
    const weapon_xp_api_v4* provider_{};
    unsigned filter_token_{},listener_token_{};
    weapon_xp_configuration_v3 configuration_{};
    weapon_xp_rules_v3 rules_{};
    bool configuration_valid_=false,present_=false,incompatible_=false,logged_incompatible_=false;
    bool active_=false;
    unsigned body_{},held_{},provider_epoch_{};
    int level_=-1;
    std::uint64_t tick_{};
    unsigned quarters_{},observed_xp_{};
    std::array<Pending,MaxReceipts> pending_{};
    std::array<Accepted,MaxReceipts> accepted_{};
    std::array<LevelUp,MaxEvents> events_{};
    unsigned event_head_{},event_count_{};

    void attach();
    void detach()noexcept;
    void refresh_configuration();
    void warn_incompatible();
    static std::int32_t filter(void*,const weapon_xp_hit_context_v4*)noexcept;
    static void provider_event(void*,const weapon_xp_event_v1*)noexcept;
    std::int32_t classify(const weapon_xp_hit_context_v4&)noexcept;
    Identity identity(unsigned moby)const;
    static bool same(const Identity&,const Identity&)noexcept;
    static bool recent(std::uint64_t now,std::uint64_t before,unsigned ttl)noexcept;
    int native_reward(unsigned target)const;
    void queue_level_up(unsigned before,unsigned after,unsigned xp)noexcept;
};
}

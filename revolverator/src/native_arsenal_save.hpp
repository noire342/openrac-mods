// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "native_arsenal.hpp"
#include "arsenal_store.hpp"
#include <filesystem>
#include <optional>

namespace revolverator {
// A native save sees only stock identities. Restore precisely the words this
// view masked, unless the call owns a campaign transition and rebuilt them.
class NativeSaveFields final : private Memory {
public:
    NativeSaveFields(const openrac_api* api, const arsenal::Registry& registry);
    ~NativeSaveFields();
    NativeSaveFields(const NativeSaveFields&) = delete;
    NativeSaveFields& operator=(const NativeSaveFields&) = delete;
    bool accept_campaign_transition();
private:
    struct Word { unsigned address{}; int original{}, masked{}; };
    std::array<Word, 9> words_{};
    unsigned count_{};
    int level_{};
    unsigned campaign_generation_{};
    bool campaign_transition_{};
    void mask(unsigned address, int original, int replacement);
};

class NativeArsenalSave final:public Memory {
public:
    struct Paths { std::filesystem::path card_root,mod_data; bool no_card=false; };
    NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal);
    // Explicit card roots keep host tests and private sessions isolated.
    NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal,Paths paths);
    void begin_save();
    void cancel_prepare();
    void after_prepare();
    void after_io();
    void after_restore_data(unsigned descriptors);
    void reset_campaign(bool preserve_custom);
    bool merge_new_game_plus();
    void import_native_state();
private:
    NativeArsenal& arsenal_;
    openrac_mod* mod_;
    std::filesystem::path card_root_,mod_data_;
    bool no_card_=false;
    std::optional<arsenal::Snapshot> captured_,pending_;
    std::filesystem::path pending_path_;
    unsigned native_state()const{return global(0x13d290);}
    std::filesystem::path active_save_path()const;
    std::filesystem::path save_path(int port,int slot)const;
    void migrate_legacy();
    arsenal::Snapshot vanilla_snapshot()const;
    void report(const char* operation,arsenal::StoreStatus status)const;
};
}

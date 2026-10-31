// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "native_arsenal.hpp"
#include "arsenal_store.hpp"
#include <filesystem>
#include <optional>

namespace revolverator {
class NativeArsenalSave final:public Memory {
public:
    struct Paths { std::filesystem::path card_root; bool no_card=false; };
    NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal);
    // Explicit card roots keep host tests and private sessions isolated.
    NativeArsenalSave(const openrac_api* api,openrac_mod* mod,NativeArsenal& arsenal,Paths paths);
    void begin_save();
    void after_prepare();
    void after_io();
    void after_restore_data(unsigned descriptors);
    void reset_campaign(bool preserve_custom);
    void import_native_state();
private:
    NativeArsenal& arsenal_;
    openrac_mod* mod_;
    std::filesystem::path card_root_;
    bool no_card_=false;
    std::optional<arsenal::Snapshot> captured_,pending_;
    std::filesystem::path pending_path_;
    unsigned native_state()const{return global(0x13d290);}
    std::filesystem::path active_save_path()const;
    arsenal::Snapshot vanilla_snapshot()const;
    void report(const char* operation,arsenal::StoreStatus status)const;
};
}

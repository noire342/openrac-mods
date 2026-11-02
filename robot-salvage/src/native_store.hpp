// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include <filesystem>
#include <functional>
#include <optional>

namespace salvage {
enum class StoreStatus {Ok,Missing,NativeMissing,InvalidNative,Stale,Corrupt,TooLarge,IoError,InvalidSnapshot};
const char* store_status_message(StoreStatus) noexcept;
std::filesystem::path salvage_sidecar_path(const std::filesystem::path& native_save);
// Native packet fingerprint and payload checksum are both checked before either
// output changes. Catalog metadata travels with the inventory for remote levels.
bool save_inventory(const std::filesystem::path&,const Catalog&,const InventorySnapshot&,StoreStatus* =nullptr) noexcept;
bool load_inventory(const std::filesystem::path&,Catalog&,InventorySnapshot&,StoreStatus* =nullptr) noexcept;

class NativeStore final:private NativeMemory {
public:
 struct Paths {std::filesystem::path card_root;bool no_card=false;};
 NativeStore(const openrac_api*,openrac_mod*,Catalog&,Inventory&);
 NativeStore(const openrac_api*,openrac_mod*,Catalog&,Inventory&,Paths);
 void set_restored_callback(std::function<void()> callback){restored_=std::move(callback);}
 void begin_save();
 void after_prepare();
 void after_io();
 void after_restore_data(unsigned descriptors);
 void reset_campaign(bool preserve);
 std::filesystem::path active_save_path()const;
private:
 struct Captured {Catalog catalog;InventorySnapshot inventory;};
 openrac_mod* mod_{};Catalog& catalog_;Inventory& inventory_;Paths paths_;
 std::optional<Captured> captured_,pending_;
 std::filesystem::path pending_path_;
 std::function<void()> restored_;
 void report(const char*,StoreStatus)const;
};
}

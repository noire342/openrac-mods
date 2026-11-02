// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_pause_api.h"
#include "pause_menu_model.hpp"
#include "shared_provider.hpp"
#include <array>
#include <thread>
namespace arsenal_compat {
class PauseRegistry final {
public:
 static constexpr std::size_t MaxOwners=16,MaxEntries=32,MaxStock=16;
 explicit PauseRegistry(ModuleLifetime&);
 ~PauseRegistry();
 bool claimed()const noexcept;
 int acquire(const char*,arsenal_pause_owner_v1*);
 int release(arsenal_pause_owner_v1);
 int register_entry(arsenal_pause_owner_v1,const arsenal_pause_entry_v1*,arsenal_pause_open_v1,std::uint64_t,arsenal_pause_entry_token_v1*);
 int unregister_entry(arsenal_pause_owner_v1,arsenal_pause_entry_token_v1);
 int update_entry(arsenal_pause_owner_v1,arsenal_pause_entry_token_v1,const arsenal_pause_entry_v1*);
 int entry_count(std::uint32_t*,std::uint64_t*)const;
 int entry_at(std::uint32_t,arsenal_pause_entry_copy_v1*)const;
 int claim_presenter(arsenal_pause_owner_v1,const arsenal_pause_capacity_v1*,arsenal_pause_presenter_v1*);
 int release_presenter(arsenal_pause_owner_v1,arsenal_pause_presenter_v1);
 int compose(arsenal_pause_presenter_v1,const arsenal_pause_stock_v1*,std::uint32_t);
 int set_focus(arsenal_pause_presenter_v1,std::uint64_t);
 int snapshot(arsenal_pause_presenter_v1,arsenal_pause_state_v1*,std::uint64_t*,std::uint32_t,std::uint32_t*)const;
 int activate(arsenal_pause_presenter_v1,std::uint64_t);
private:
 struct Owner{std::uint64_t token{};char name[64]{};};
 struct Entry{std::uint64_t owner{},token{},context{};arsenal_pause_entry_v1 descriptor{};arsenal_pause_open_v1 open{};void* module{};};
 ModuleLifetime& modules_;std::thread::id thread_;
 std::array<Owner,MaxOwners> owners_{};std::array<Entry,MaxEntries> entries_{};
 arsenal::pause::Model model_;std::uint64_t generation_{},revision_{},presenter_{},presenter_owner_{};
 arsenal_pause_capacity_v1 capacity_{};std::uint32_t stock_visible_{};unsigned dispatch_depth_{};bool busy_{};
 int access(bool mutation=true)const;
 bool owner(std::uint64_t)const;
 Entry* find(std::uint64_t,std::uint64_t);
 const Entry* find_id(std::uint64_t)const;
 bool room(std::uint32_t custom,std::uint32_t stock)const;
 std::uint32_t visible_custom()const;
 static bool descriptor(const arsenal_pause_entry_v1*);
 static int status(arsenal::pause::Status);
};
}

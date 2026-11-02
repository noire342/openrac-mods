// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "core.hpp"
#include "robot_salvage_api.h"
#include <thread>
namespace salvage {
// Native mod exports openrac_robot_salvage_query(version) by forwarding query.
// Lifecycle and public calls stay on the same game thread as the owned domain.
class ApiService final {
public:
 ApiService(Catalog&,Inventory&,World&,Events&);
 ~ApiService();
 ApiService(const ApiService&)=delete;
 ApiService& operator=(const ApiService&)=delete;
 static const robot_salvage_api_v1* query(std::uint32_t version)noexcept;
 Catalog& catalog;Inventory& inventory;World& world;Events& events;
 bool thread_ok()const noexcept{return std::this_thread::get_id()==thread_;}
private:
 std::thread::id thread_;
};
}

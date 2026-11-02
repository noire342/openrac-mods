// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "garage_bridge.hpp"
#include "campaign_progress.hpp"
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace spacevoyages {
// The optional provider is already loaded by OpenRAC. Never load a second
// plugin instance, bootstrap a profile or infer Home from program18. Keep the
// provider's code alive until the observer is unbound; ownership remains in
// the provider and is revalidated for the current native load each time.
class GarageProvider final {
public:
 explicit GarageProvider(GarageRegion region) noexcept:observer_(region),campaign_(
  region==GarageRegion::Pal?CampaignRegion::Pal:region==GarageRegion::Ntsc?CampaignRegion::Ntsc:CampaignRegion::Unknown){}
 ~GarageProvider(){close();}
 GarageProvider(const GarageProvider&)=delete;
 GarageProvider& operator=(const GarageProvider&)=delete;
 bool bind_loaded() noexcept {
#ifdef _WIN32
  if(module_)return true;
  observer_.unbind();campaign_.unbind();
  const auto loaded=GetModuleHandleW(L"veldin_garage_preview.dll");
  if(!loaded)return false;
  const auto symbol=GetProcAddress(loaded,garage_query_export);if(!symbol)return false;
  HMODULE retained{};
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(loaded),&retained))return false;
  openrac_veldin_garage_query_v1_fn query{};
  static_assert(sizeof query==sizeof symbol,"Windows export pointer size");
  std::memcpy(&query,&symbol,sizeof query);
  // This is optional and independent from the unchanged fleet export. Older
  // providers keep stock travel but cannot authorize a Home progress bypass.
  const auto campaign_symbol=GetProcAddress(retained,campaign_query_export);
  veldin_garage_query_campaign_v1_fn campaign_query{};
  static_assert(sizeof campaign_query==sizeof campaign_symbol,"Windows campaign export pointer size");
  std::memcpy(&campaign_query,&campaign_symbol,sizeof campaign_query);
  module_=retained;observer_.bind(query);campaign_.bind(campaign_query);return true;
#else
  // The provider currently supports the reviewed Windows PAL runtime only.
  observer_.unbind();campaign_.unbind();
  return false;
#endif
 }
 GarageReadStatus poll(std::uint32_t generation) noexcept{return observer_.poll(generation);}
 const GarageObservation& observation()const noexcept{return observer_.observation();}
 CampaignReadStatus poll_campaign(std::uint32_t generation) noexcept{return campaign_.poll(generation);}
 const CampaignObservation& campaign_observation()const noexcept{return campaign_.observation();}
 CampaignReadStatus campaign_status()const noexcept{return campaign_.status();}
 bool campaign_supported()const noexcept{return campaign_.bound();}
 bool bound()const noexcept {
#ifdef _WIN32
  return module_!=nullptr;
#else
  return false;
#endif
 }
 void close() noexcept {
  observer_.unbind();campaign_.unbind();
#ifdef _WIN32
  if(module_){FreeLibrary(module_);module_=nullptr;}
#endif
 }
private:
 GarageBridge observer_;
 CampaignProgress campaign_;
#ifdef _WIN32
 HMODULE module_{};
#endif
};
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_hooks.hpp"
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace salvage {
namespace {void module_marker(){}}
NativeHooks::NativeHooks(const openrac_api* api,openrac_mod* mod)try{
#ifdef _WIN32
 auto module=GetModuleHandleW(L"arsenal_compat.dll");
 if(!module){
  HMODULE own{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&module_marker),&own))throw std::runtime_error("Cannot locate Robot Salvage module");
  std::vector<wchar_t> path(32768);const auto n=GetModuleFileNameW(own,path.data(),DWORD(path.size()));if(!n||n>=path.size())throw std::runtime_error("Cannot locate Robot Salvage package");
  const auto file=std::filesystem::path(path.data()).parent_path()/L"arsenal_compat.dll";
  module=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!module)throw std::runtime_error("Robot Salvage package lacks its shared compatibility library");module_=module;
  const auto start=reinterpret_cast<openrac_mod_load_fn>(GetProcAddress(module,OPENRAC_MOD_LOAD));
  if(!start||start(api,mod))throw std::runtime_error("Shared compatibility library refused this official PAL client");
 }
 const auto query=reinterpret_cast<openrac_arsenal_hooks_query_v2_fn>(GetProcAddress(module,OPENRAC_ARSENAL_HOOKS_QUERY));
 service_=query?query(2):nullptr;
 if(!service_||service_->version!=2||service_->size<sizeof(*service_)||!service_->acquire||!service_->bind||!service_->commit||!service_->release)
  throw std::runtime_error("Robot Salvage requires the shared compatibility v2 library; update the bundled Rotatore utility too");
 const auto status=service_->acquire("robot-salvage",&token_);if(status)throw std::runtime_error("Cannot acquire shared hook ownership: "+std::to_string(status));
 const auto pause_query=reinterpret_cast<openrac_arsenal_pause_query_fn>(GetProcAddress(module,OPENRAC_ARSENAL_PAUSE_QUERY));
 pause_=pause_query?pause_query(1):nullptr;
 if(!pause_||pause_->version!=1||pause_->size<sizeof(*pause_))throw std::runtime_error("Robot Salvage needs the bundled shared pause-menu registry");
#else
 (void)api;(void)mod;throw std::runtime_error("Robot Salvage native adapter currently requires Windows x64");
#endif
}catch(...){
 if(service_&&token_)service_->release(token_);
#ifdef _WIN32
 if(module_)FreeLibrary(static_cast<HMODULE>(module_));
#endif
 throw;
}
NativeHooks::~NativeHooks(){
 if(service_&&token_){const auto status=service_->release(token_);if(status)return;}
#ifdef _WIN32
 if(module_)FreeLibrary(static_cast<HMODULE>(module_));
#endif
}
void NativeHooks::bind(const char* id,const char* signature,void* replacement,void** original){const int status=service_->bind(token_,id,signature,replacement,original);if(status)throw std::runtime_error(std::string("Robot Salvage native hook ")+id+" failed: "+std::to_string(status));}
void NativeHooks::commit(){const int status=service_->commit(token_);if(status)throw std::runtime_error("Robot Salvage native hook activation failed: "+std::to_string(status));}
}

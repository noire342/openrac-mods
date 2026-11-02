// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "legacy_facade.hpp"
#include <cstring>
namespace arsenal_compat {
namespace {bool bounded(const char* s,std::size_t max){if(!s||!*s)return false;for(std::size_t i=0;i<=max;++i)if(!s[i])return true;return false;}}
int LegacyFacade::claim(const char* name){
 if(auto r=shared_.access_status())return r;
 if(!bounded(name,63))return status_=ARSENAL_HOOKS_ARGUMENT;
 if(token_)return status_=ARSENAL_HOOKS_CONSUMER;
 const auto result=shared_.acquire(name,&token_);return status_=result;
}
int LegacyFacade::bind(const char* id,const char* signature,void* replacement,void** original){
 if(original)*original=nullptr;
 if(auto r=shared_.access_status())return r;
 if(!bounded(id,95)||!bounded(signature,63)||!replacement||!original)return status_=ARSENAL_HOOKS_ARGUMENT;
 if(!Provider::owns_id(id))return ARSENAL_HOOKS_UNKNOWN;
 for(const auto& spec:hook_specs())if(!std::strcmp(spec.id,id)&&std::strcmp(spec.signature,signature))return status_=ARSENAL_HOOKS_SIGNATURE;
 if(failed_)return status_=ARSENAL_HOOKS_NOT_READY;
 const auto result=shared_.bind(token_,id,signature,replacement,original);
 if(result==ARSENAL_HOOKS_BACKEND)failed_=true;
 return status_=result;
}
int LegacyFacade::commit(){
 if(auto r=shared_.access_status())return r;
 if(!token_)return status_=ARSENAL_HOOKS_NOT_CLAIMED;
 if(shared_.active(token_))return status_=ARSENAL_HOOKS_COMMITTED;
 if(failed_)return status_=ARSENAL_HOOKS_NOT_READY;
 for(std::size_t i=0;i<OPENRAC_ARSENAL_REQUIRED_HOOKS;++i)if(!shared_.bound(token_,i))return status_=ARSENAL_HOOKS_INCOMPLETE;
 const auto result=shared_.commit(token_);if(result)failed_=true;return status_=result;
}
int LegacyFacade::rollback(){
 if(auto r=shared_.access_status())return r;
 if(token_){const auto result=shared_.release(token_);if(result)return status_=result;token_=0;}
 failed_=false;return status_=0;
}
int LegacyFacade::diagnostics(arsenal_hooks_diagnostics_v1* out)const{
 const auto result=shared_.diagnostics(token_,out);if(result)return result;
 out->version=OPENRAC_ARSENAL_HOOKS_ABI_V1;out->required=OPENRAC_ARSENAL_REQUIRED_HOOKS;out->last_status=status_;
 if(failed_)out->stage=ARSENAL_HOOKS_FAILED;
 return 0;
}
}

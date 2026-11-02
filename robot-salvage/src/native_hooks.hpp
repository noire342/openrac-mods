// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openrac/openrac_mod.h>
#include "arsenal_hooks_api_v2.h"
#include "arsenal_pause_api.h"
namespace salvage {
class NativeHooks final {
public:
 NativeHooks(const openrac_api*,openrac_mod*);
 ~NativeHooks();
 NativeHooks(const NativeHooks&)=delete;
 void bind(const char*,const char*,void*,void**);
 void commit();
 const arsenal_pause_api_v1* pause_api()const{return pause_;}
private:
 const arsenal_hooks_api_v2* service_{};arsenal_hooks_consumer_v2 token_{};
 const arsenal_pause_api_v1* pause_{};
 void* module_{};
};
}

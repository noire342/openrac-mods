// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "shared_provider.hpp"
namespace arsenal_compat {
class LegacyFacade final{
public:
 explicit LegacyFacade(SharedProvider& shared):shared_(shared){}
 int claim(const char*);
 int bind(const char*,const char*,void*,void**);
 int commit();
 int rollback();
 int diagnostics(arsenal_hooks_diagnostics_v1*)const;
private:
 SharedProvider& shared_;
 SharedProvider::Token token_{};
 int status_{};
 bool failed_{};
};
}

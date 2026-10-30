// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
namespace revolverator {
class ArsenalIdentityView {
public:
    virtual ~ArsenalIdentityView()=default;
    virtual void select_identity(bool custom)=0;
};
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_state.hpp"

namespace revolverator {
// A bounded native UI call may temporarily view a logical weapon through its
// native carrier. The adapter owns its snapshot and restores gameplay on exit.
class ArsenalMenuModel {
public:
    virtual ~ArsenalMenuModel()=default;
    virtual arsenal::Registry& registry()=0;
    virtual void project(arsenal::WeaponKey key)=0;
    virtual void commit()=0;
    virtual void restore_gameplay()=0;
};
}

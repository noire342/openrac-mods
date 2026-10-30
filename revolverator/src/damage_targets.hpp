// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace revolverator {
// CrateUpdate (NTSC-U FUN_L00_002cf218) handles the five authored crate
// classes and its temporary 507..510 class changes. The broad native
// "500..540" helper is for crate-stack membership, not damage eligibility:
// it also includes unrelated classes such as the ships at 531..533.
constexpr bool destructible_crate_class(int klass) {
    switch (klass) {
    case 500: // bolt crate
    case 501: // nanotech crate
    case 502: // reinforced metal crate
    case 505: // explosive crate
    case 507: case 508: case 509: case 510: // native class-change states
    case 511: // ammunition crate
        return true;
    default:
        return false;
    }
}
}

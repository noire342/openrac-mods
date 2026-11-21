// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

namespace revolverator {
inline constexpr unsigned hoven_anklebiter_class = 238;
inline constexpr unsigned hoven_blarg_trooper_class = 294;

// Native aim also targets scripted props and attached actors. Retail class IDs
// distinguish those from enemies that can travel with a carry lease.
constexpr bool capture_excluded_class(unsigned klass) {
    switch (klass) {
    case 253: case 625: case 1454: // tanks
    case 1048: case 1202: // armored snappers and toxic crabs
    case 1051: case 1106: case 1422: // Alien Queen, Snagglebeast, Drek
    case 1201: case 1346: // mountable and linked turrets
    case 233: case 326: case 631: // helicopter gunships with their own attached actors
    case 36: case 401: case 1632: // mounted gunner, fighter part, hit proxy
    case 455: case 668: case 755: case 1885: // spawners, Sand Shark nest/egg
    case 541: case 556: case 871: case 1906: // test dummies, scripted boltcrate, Gadgebot
    case 568: case 621: case 643: case 660: case 933:
    case 1110: case 1112: case 1269: case 1355: // mines and boss missiles
        return true;
    default:
        return false;
    }
}

// Only live, nonexplosive retail crate classes. The native update also serves
// temporary class-change actors, which are damageable but cannot be latched.
constexpr bool capturable_crate_class(unsigned klass) {
    return klass==500||klass==501||klass==502||klass==511;
}

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

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace clank {
// World hazards can request Ratchet-only states even in vehicle mode 1. Adapt
// those before native state entry dereferences an incompatible animation bank.
constexpr int clank_state(int requested) noexcept {
    if ((requested >= 0x43 && requested <= 0x52) || requested == 0x63 ||
        (requested >= 0x64 && requested <= 0x67) || requested == 0x70 || requested == 0x71 ||
        requested == 0x72 || requested == 0x74 || requested == 0x77 || requested == 0x78 ||
        requested == 0x7D)
        return requested;
    switch (requested) {
    case 0:
    case 1:
    case 3:
    case 4:
    case 8:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x27:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x40:
    case 0x41:
    case 0x42:
        return 0x43;
    case 2:
    case 0x2F:
        return 0x44;
    case 6:
    case 0x2D:
        return 0x45;
    case 7:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xF:
    case 0x10:
    case 0x11:
        return 0x49;
    case 0xE:
        return 0x4F;
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
        return 0x51;
    case 0x16:
    case 0x76:
        return 0x46;
    case 0x18:
        return 0x4A;
    case 0x19:
        return 0x4B;
    case 0x1A:
        return 0x4D;
    case 0x1B:
        return 0x4E;
    case 0x1C:
        return 0x4C;
    case 0x79:
        return 0x52;
    default:
        return 0x7D; // Water, quicksand and unsupported vehicle/weapon states.
    }
}
} // namespace clank

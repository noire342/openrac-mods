// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace cinematic {
inline constexpr bool guest_range(
    std::uint32_t at, std::size_t bytes, std::uint32_t base, std::uint32_t length
) noexcept {
    return at >= base && at - base < length && bytes <= length - (at - base);
}

// Pad reports are translated C locals, often in the port's mapped arena, not
// PS2 RAM. These two writable regions match port/runtime/include/openrac/guest.h
// in launcher-v0.10.0. Never dereference holes, registers, or wrapped ranges.
inline constexpr bool report_range(std::uint32_t at, std::size_t bytes) noexcept {
    return guest_range(at, bytes, 0, 0x02000000u)
           || guest_range(at, bytes, 0xE0000000u, 0x01000000u);
}
}  // namespace cinematic

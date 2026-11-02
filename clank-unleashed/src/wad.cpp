// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the OpenRAC contributors
//
// Adapted from ReRAC (https://github.com/re-rac/rerac), crates/rc-formats/src/wad.rs and
// docs/formats/disc_layout.md section 3: ISC License, Copyright (c) 2026 ReRAC contributors.
//
// WAD decompression and compression. Packet kinds, by flag byte:
//
//   0x00..0x0f  literal run (4..273 bytes); never followed by another one
//   0x10..0x1f  far match (displacement 0x4000 * (A + 1) + 14 bits), or, with
//               a zero displacement, the pad (skip to the next 0x1000 bytes of
//               the stream) and dummy (only a little literal) packets
//   0x20..0x3f  medium and big match (displacement 1..16384, 3..288 bytes)
//   0x40..0xff  little match (displacement 1..2048, 3..8 bytes)
//
// Every match and the dummy packet end with 0..3 "little literal" bytes, their
// count in the low two bits of the packet's second-to-last byte.

#include "wad.hpp"
#include <cstring>
#include <stdexcept>
namespace clank {
using u8 = std::uint8_t;
using u32 = std::uint32_t;
constexpr std::size_t kWadHeaderSize = 16;
constexpr u8 kMagic[3] = {'W', 'A', 'D'};
template <class... Args> [[noreturn]] void fail(const char *text, Args...) {
    throw std::runtime_error(text);
}
bool is_wad(ByteView bytes) {
    return bytes.size() >= kWadHeaderSize && std::memcmp(bytes.data(), kMagic, 3) == 0;
}

u32 wad_compressed_size(ByteView bytes) {
    // Only streams that start with the WAD header have a compressed size.
    if (!is_wad(bytes)) {
        fail("not a WAD stream");
    }

    return bytes.u32_at(3);
}

std::vector<u8> wad_decompress(ByteView bytes) {
    const std::size_t total = wad_compressed_size(bytes);

    // The size must cover the header and fit the buffer given.
    if (total < kWadHeaderSize || total > bytes.size()) {
        fail("WAD: compressed size {:#x} exceeds the {:#x}-byte buffer", total, bytes.size());
    }

    const u8 *stream = bytes.data() + kWadHeaderSize;
    const std::size_t end = total - kWadHeaderSize;
    std::size_t ptr = 0;
    std::vector<u8> out;
    out.reserve(total * 2);

    auto next = [&]() -> u8 {
        // A read past the stream's end means it was cut short.
        if (ptr >= end) {
            fail("WAD: read past the end of the stream");
        }

        return stream[ptr++];
    };
    auto literals = [&](std::size_t n) {
        // A literal run past the end means the stream was cut short.
        if (end - ptr < n) {
            fail("WAD: a literal runs past the end of the stream");
        }

        if (n > 32 * 1024 * 1024 - out.size())
            fail("WAD output limit");
        out.insert(out.end(), stream + ptr, stream + ptr + n);
        ptr += n;
    };
    auto copy_match = [&](std::size_t displacement, std::size_t length) {
        // A match cannot point before the start of the output.
        if (displacement == 0 || displacement > out.size()) {
            fail("WAD: a match starts before the output");
        }

        // Byte by byte: overlapping copies are run-length expansion.
        if (length > 32 * 1024 * 1024 - out.size())
            fail("WAD output limit");
        const std::size_t source = out.size() - displacement;

        // Copy byte by byte so a match that overlaps its own output repeats it.
        for (std::size_t i = 0; i < length; ++i) {
            out.push_back(out[source + i]);
        }
    };

    // Decode packets until the stream is used up.
    while (ptr < end) {
        const u8 flag = next();

        // A flag below 0x10 starts a literal run.
        if (flag < 0x10) {
            const std::size_t n = flag == 0 ? std::size_t{next()} + 18 : std::size_t{flag} + 3;
            literals(n);

            // The format never puts two literal runs in a row, so a second one means a damaged
            // stream.
            if (ptr < end && stream[ptr] < 0x10) {
                fail("WAD: two literal packets in a row");
            }

            continue;
        }

        std::size_t length = 0;
        std::size_t displacement = 0;
        u8 little = 0;

        // Flags 0x10 to 0x1f are far matches.
        if (flag < 0x20) {
            std::size_t ml = flag & 7;

            // A zero length field means the length follows in the next byte.
            if (ml == 0) {
                ml = std::size_t{next()} + 7;
            }

            const u8 b0 = next();
            const u8 b1 = next();
            const std::size_t a = (flag >> 3) & 1;
            const std::size_t far = std::size_t{b1} * 0x40 + (b0 >> 2);

            // A far match with no displacement is the padding code or the end of the stream.
            if (a == 0 && far == 0) {
                // A length other than 1 is padding: the stream goes on at the next 0x1000 boundary.
                if (ml != 1) {
                    ptr = (ptr + 0xfff) & ~std::size_t{0xfff}; // pad: on at the next 0x1000
                    continue;
                }

                literals(b0 & 3); // dummy
                continue;
            }

            // Bit 3 extends the window (the form verified on the NTSC-U disc).
            displacement = 0x4000 * (a + 1) + far;
            length = ml + 2;
            little = b0;
        } else if (flag < 0x40) {
            std::size_t ml = flag & 0x1f;

            // A zero length field means the length follows in the next byte.
            if (ml == 0) {
                ml = std::size_t{next()} + 0x1f;
            }

            length = ml + 2;
            const u8 b1 = next();
            const u8 b2 = next();
            displacement = std::size_t{b2} * 0x40 + (b1 >> 2) + 1;
            little = b1;
        } else {
            const u8 b1 = next();
            length = (flag >> 5) + 1;
            displacement = std::size_t{b1} * 8 + ((flag >> 2) & 7) + 1;
            little = flag;
        }

        copy_match(displacement, length);
        literals(little & 3);
    }

    return out;
}

} // namespace clank

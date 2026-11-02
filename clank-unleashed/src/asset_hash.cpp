// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_hash.hpp"
#include <array>
#include <limits>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
namespace clank {
bool sha256_matches(const void *data, std::size_t bytes, const char *expected) noexcept {
    if (!data || !expected || std::strlen(expected) != 64 ||
        bytes > std::numeric_limits<ULONG>::max())
        return false;
    std::array<UCHAR, 32> hash{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                   const_cast<PUCHAR>(static_cast<const UCHAR *>(data)), static_cast<ULONG>(bytes),
                   hash.data(), 32) < 0)
        return false;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i)
        if (hex[hash[i] >> 4] != expected[i * 2] || hex[hash[i] & 15] != expected[i * 2 + 1])
            return false;
    return true;
}
} // namespace clank

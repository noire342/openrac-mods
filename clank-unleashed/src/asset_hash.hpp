// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
namespace clank {
bool sha256_matches(const void *data, std::size_t bytes, const char *expected) noexcept;
}

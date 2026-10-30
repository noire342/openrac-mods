// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace skillpoints {
struct Definition {
    std::uint32_t id, points;
    int native_index;
    const char *title, *description, *author, *rule, *rule_sha256;
};

extern const Definition kCatalogue[];
extern const std::size_t kCatalogueSize;

inline const Definition* find_definition(std::uint32_t id) {
    for (std::size_t i = 0; i < kCatalogueSize; ++i) {
        if (kCatalogue[i].id == id) {
            return &kCatalogue[i];
        }
    }
    return nullptr;
}
}  // namespace skillpoints

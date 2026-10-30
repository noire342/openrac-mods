// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstring>

#include "engine.hpp"

#include <openrac/openrac_mod.h>

namespace skillpoints {
class GameMemory final : public IMemory {
public:
    explicit GameMemory(const openrac_api* api) : m_api(api) {}

    bool read(std::uint32_t address, void* output, std::uint32_t bytes) const override {
        // RA operands already name the active NTSC-U console address, including
        // level-specific RAM. Do not relocate a live pointer or a RA operand a second time.
        if (address >= 0x02000000u || bytes > 0x02000000u - address) {
            return false;
        }
        std::memcpy(output, m_api->memory + address, bytes);
        return true;
    }

private:
    const openrac_api* m_api;
};

}  // namespace skillpoints

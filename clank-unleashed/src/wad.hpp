// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace clank {
class ByteView {
  public:
    explicit ByteView(const std::vector<std::uint8_t> &data) : m_data(data) {}
    std::size_t size() const noexcept { return m_data.size(); }
    const std::uint8_t *data() const noexcept { return m_data.data(); }
    std::uint32_t u32_at(std::size_t at) const {
        if (at > size() || size() - at < 4)
            throw std::runtime_error("Truncated asset header");
        std::uint32_t value;
        std::memcpy(&value, data() + at, 4);
        return value;
    }

  private:
    const std::vector<std::uint8_t> &m_data;
};
std::vector<std::uint8_t> wad_decompress(ByteView);
} // namespace clank

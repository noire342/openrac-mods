// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>

#include "progression.hpp"

namespace weaponxp {

class Store {
public:
    Store() = default;
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    void open(const std::filesystem::path& path, Progression& state);
    void save(const Progression& state);
    void close();

    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    // OS lock held for the whole session: another process cannot overwrite this profile.
    std::intptr_t m_lock = -1;
};

std::filesystem::path profile_path(const std::string& disc, const std::string& profile);

}  // namespace weaponxp

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>

#include "ledger.hpp"

namespace skillpoints {
class IStore {
public:
    virtual ~IStore() = default;
    virtual void save(const Ledger&) = 0;
};

class FileStore final : public IStore {
public:
    explicit FileStore(const std::filesystem::path&, Ledger&);
    ~FileStore();
    FileStore(const FileStore&) = delete;
    FileStore& operator=(const FileStore&) = delete;
    void save(const Ledger&) override;

private:
    std::filesystem::path m_path;
    std::intptr_t m_lock = -1;
};

std::filesystem::path profile_path(const std::string& profile);
}  // namespace skillpoints

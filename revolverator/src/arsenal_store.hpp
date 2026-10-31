// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include "arsenal_state.hpp"
#include <filesystem>

namespace arsenal {
enum class StoreStatus {
    Ok,
    Missing,
    Stale,
    Corrupt,
    NativeMissing,
    InvalidNative,
    InvalidSnapshot,
    TooLarge,
    IoError
};

inline constexpr std::size_t MaximumSidecarBytes=256*1024;
inline constexpr std::size_t MaximumNativePrefixBytes=4*1024*1024;

// saveN.bin -> saveN.bin.arsenal. The native save is always an input and is never
// opened for writing. Only its first 8+mainSize+20*recordSize bytes are hashed;
// unrelated native trailers do not invalidate the matching logical inventory.
std::filesystem::path sidecar_path(const std::filesystem::path& native_save);
const char* store_status_message(StoreStatus status)noexcept;

// Both boundaries catch exceptions and return a descriptive status. save writes
// a unique temporary sidecar in the same directory, flushes it, then atomically
// replaces only our final sidecar. It never modifies or replaces native_save.
// load leaves output byte-for-byte semantically unchanged on every failure.
// New writes use codec2. Codec1 loads with golden=false; codec2 uses the old
// per-record reserved byte as a canonical 0/1 Golden flag (no layout growth).
bool save(const std::filesystem::path& native_save,const Snapshot& snapshot,
          StoreStatus* status=nullptr)noexcept;
bool load(const std::filesystem::path& native_save,Snapshot& output,
          StoreStatus* status=nullptr)noexcept;
}

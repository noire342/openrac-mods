// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace arsenal::pause {

// Application identities, never guest addresses or SDK handles. Zero means no entry.
using EntryId = std::uint64_t;
enum class Status { Ok, Invalid, Duplicate, NotFound, Exhausted };

struct Entry {
    EntryId id{};
    std::int64_t order{};
    bool visible = true;
};
bool operator==(const Entry&, const Entry&) noexcept;
bool operator!=(const Entry&, const Entry&) noexcept;

// A value snapshot: retaining or modifying a copy cannot affect the registry.
struct Snapshot {
    std::uint64_t revision{};
    EntryId focus{};
    std::vector<EntryId> visible;

    bool contains(EntryId) const noexcept;
    EntryId previous(EntryId) const noexcept;
    EntryId next(EntryId) const noexcept;
};

// Pure composition, also usable by adapters that already own their registry.
// Sort by order; ties retain stock input order, then ascending custom ID.
// Validate ALL entries, including hidden ones. Duplicate or zero identities
// leave output untouched. Successful composition increments previous.revision.
// Focus survives by identity. If removed/hidden, use the first remaining old
// successor (circular), otherwise the new first entry. Empty rings have focus0.
// Allocation failure throws before output changes; callers serialize access.
Status compose_snapshot(const std::vector<Entry>& stock,
                        const std::vector<Entry>& custom,
                        const Snapshot& previous,
                        Snapshot& output);

// Owns custom registrations and the last complete native stock description.
// No rendering, native pointers, callbacks, module lifetime, or SDK dependency.
// Each successful mutation rebuilds and commits one complete snapshot. Failed
// mutations (including allocation failure) preserve registry, stock and focus.
// Identical inputs are idempotent and do not advance the revision.
class Model final {
public:
    Status register_entry(Entry);
    Status remove_entry(EntryId);
    Status set_visibility(EntryId, bool);
    Status set_order(EntryId, std::int64_t);
    Status compose(const std::vector<Entry>& stock);
    Status set_focus(EntryId);
    Snapshot snapshot() const { return snapshot_; }

private:
    using Registry = std::map<EntryId, Entry>;
    Registry custom_;
    std::vector<Entry> stock_;
    Snapshot snapshot_;

    Status commit(std::vector<Entry> stock, Registry custom);
};

} // namespace arsenal::pause

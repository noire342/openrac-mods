// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "pause_menu_model.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <set>
#include <utility>

namespace arsenal::pause {

bool operator==(const Entry& a, const Entry& b) noexcept {
    return a.id == b.id && a.order == b.order && a.visible == b.visible;
}
bool operator!=(const Entry& a, const Entry& b) noexcept { return !(a == b); }

bool Snapshot::contains(EntryId id) const noexcept {
    return id != 0 && std::find(visible.begin(), visible.end(), id) != visible.end();
}
EntryId Snapshot::previous(EntryId id) const noexcept {
    const auto it = std::find(visible.begin(), visible.end(), id);
    if (!id || it == visible.end()) return 0;
    return it == visible.begin() ? visible.back() : *std::prev(it);
}
EntryId Snapshot::next(EntryId id) const noexcept {
    const auto it = std::find(visible.begin(), visible.end(), id);
    if (!id || it == visible.end()) return 0;
    return std::next(it) == visible.end() ? visible.front() : *std::next(it);
}

namespace {
EntryId retained_focus(const Snapshot& old, const Snapshot& next) noexcept {
    if (next.contains(old.focus)) return old.focus;
    const auto previous = std::find(old.visible.begin(), old.visible.end(), old.focus);
    if (previous != old.visible.end()) {
        auto it = previous;
        for (std::size_t step = 0; step < old.visible.size(); ++step) {
            if (++it == old.visible.end()) it = old.visible.begin();
            if (next.contains(*it)) return *it;
        }
    }
    return next.visible.empty() ? 0 : next.visible.front();
}
} // namespace

Status compose_snapshot(const std::vector<Entry>& stock,
                        const std::vector<Entry>& custom,
                        const Snapshot& previous,
                        Snapshot& output) {
    if (previous.revision == std::numeric_limits<std::uint64_t>::max()) return Status::Exhausted;
    std::set<EntryId> identities;
    std::vector<Entry> ordered;
    ordered.reserve(stock.size() + custom.size());
    auto append = [&](const Entry& entry) {
        if (!entry.id) return Status::Invalid;
        if (!identities.insert(entry.id).second) return Status::Duplicate;
        if (entry.visible) ordered.push_back(entry);
        return Status::Ok;
    };
    for (const auto& entry : stock) {
        const auto status = append(entry);
        if (status != Status::Ok) return status;
    }
    auto custom_order = custom;
    std::sort(custom_order.begin(), custom_order.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });
    for (const auto& entry : custom_order) {
        const auto status = append(entry);
        if (status != Status::Ok) return status;
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Entry& a, const Entry& b) { return a.order < b.order; });
    Snapshot candidate;
    candidate.revision = previous.revision + 1;
    candidate.visible.reserve(ordered.size());
    for (const auto& entry : ordered) candidate.visible.push_back(entry.id);
    candidate.focus = retained_focus(previous, candidate);
    // Value assignment is nonallocating with the default vector allocator.
    output = std::move(candidate);
    return Status::Ok;
}

Status Model::commit(std::vector<Entry> stock, Registry custom) {
    std::vector<Entry> entries;
    entries.reserve(custom.size());
    for (const auto& [id, entry] : custom) { (void)id; entries.push_back(entry); }
    Snapshot candidate;
    const auto status = compose_snapshot(stock, entries, snapshot_, candidate);
    if (status != Status::Ok) return status;
    stock_.swap(stock);
    custom_.swap(custom);
    snapshot_ = std::move(candidate);
    return Status::Ok;
}

Status Model::register_entry(Entry entry) {
    if (!entry.id) return Status::Invalid;
    if (custom_.count(entry.id) || std::any_of(stock_.begin(), stock_.end(), [&](const Entry& item) { return item.id == entry.id; })) return Status::Duplicate;
    auto staged = custom_;
    staged.emplace(entry.id, entry);
    return commit(stock_, std::move(staged));
}
Status Model::remove_entry(EntryId id) {
    if (!id) return Status::Invalid;
    if (!custom_.count(id)) return Status::NotFound;
    auto staged = custom_;
    staged.erase(id);
    return commit(stock_, std::move(staged));
}
Status Model::set_visibility(EntryId id, bool visible) {
    if (!id) return Status::Invalid;
    const auto found = custom_.find(id);
    if (found == custom_.end()) return Status::NotFound;
    if (found->second.visible == visible) return Status::Ok;
    auto staged = custom_;
    staged.at(id).visible = visible;
    return commit(stock_, std::move(staged));
}
Status Model::set_order(EntryId id, std::int64_t order) {
    if (!id) return Status::Invalid;
    const auto found = custom_.find(id);
    if (found == custom_.end()) return Status::NotFound;
    if (found->second.order == order) return Status::Ok;
    auto staged = custom_;
    staged.at(id).order = order;
    return commit(stock_, std::move(staged));
}
Status Model::compose(const std::vector<Entry>& stock) {
    if (stock == stock_) return Status::Ok;
    return commit(stock, custom_);
}
Status Model::set_focus(EntryId id) {
    if (!id && snapshot_.visible.empty()) return Status::Ok;
    if (!id) return Status::Invalid;
    if (!snapshot_.contains(id)) return Status::NotFound;
    if (snapshot_.focus == id) return Status::Ok;
    if (snapshot_.revision == std::numeric_limits<std::uint64_t>::max()) return Status::Exhausted;
    snapshot_.focus = id;
    ++snapshot_.revision;
    return Status::Ok;
}

} // namespace arsenal::pause

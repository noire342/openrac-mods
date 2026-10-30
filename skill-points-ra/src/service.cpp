// SPDX-License-Identifier: GPL-3.0-or-later
#include "service.hpp"

#include <limits>

#include "runtime.hpp"

namespace skillpoints {
static int totals(sp_totals* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    *out =
        {sizeof(*out),
         static_cast<std::uint32_t>(kCatalogueSize),
         static_cast<std::uint32_t>(active->ledger.count()),
         667,
         active->ledger.earned_points()};
    return SP_OK;
}

static int get(std::uint32_t id, sp_achievement* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    const auto* d = find_definition(id);
    if (!d) {
        return SP_NOT_FOUND;
    }
    const auto measured = active->engine.measured(id);
    *out =
        {sizeof(*out),
         d->id,
         d->points,
         d->native_index,
         d->title,
         d->description,
         d->author,
         active->ledger.unlocked(id) ? 1u : 0u,
         measured.first,
         measured.second,
         active->ledger.timestamp(id)};
    return SP_OK;
}

static int at(std::uint32_t i, sp_achievement* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (i >= kCatalogueSize) {
        return SP_NOT_FOUND;
    }
    return get(kCatalogue[i].id, out);
}

static int enabled(std::uint32_t id) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!find_definition(id)) {
        return SP_NOT_FOUND;
    }
    return active->policy.enabled(id) ? 1 : 0;
}

static int active_totals(sp_totals* out) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!out || out->size < sizeof(*out)) {
        return SP_INVALID;
    }
    *out =
        {sizeof(*out),
         active->policy.count(),
         active->policy.unlocked(active->ledger),
         active->policy.points(),
         active->policy.earned_points(active->ledger)};
    return SP_OK;
}

static int subscribe(sp_listener fn, void* user, std::uint32_t* token) {
    if (!active || active->stopping) {
        return SP_UNAVAILABLE;
    }
    if (active->dispatching) {
        return SP_BUSY;
    }
    if (!fn || !token) {
        return SP_INVALID;
    }
    if (active->next_token == std::numeric_limits<std::uint32_t>::max()) {
        return SP_BUSY;
    }
    for (auto& l : active->listeners) {
        if (!l.fn) {
            l = {active->next_token++, fn, user};
            *token = l.token;
            return SP_OK;
        }
    }
    return SP_BUSY;
}

static int unsubscribe(std::uint32_t token) {
    if (!active) {
        return SP_UNAVAILABLE;
    }
    if (!token) {
        return SP_INVALID;
    }
    for (auto& l : active->listeners) {
        if (l.token == token) {
            l = {};
            return SP_OK;
        }
    }
    return SP_NOT_FOUND;
}

extern const sp_api service{
    sizeof(sp_api),
    OPENRAC_SKILL_POINTS_ABI,
    totals,
    at,
    get,
    subscribe,
    unsubscribe,
    enabled,
    active_totals
};
}  // namespace skillpoints

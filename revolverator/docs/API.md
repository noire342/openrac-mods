# Revolverator API v2

The manifest also exposes `extensions["noire342.arsenal"]`, version1: a
declarative discovery contract for future launchers. It lists stable logical
keys, names, capacity20, weapon/ammunition icon frames and the independent
save codec. Reading it does not load or execute the DLL. This is this mod's
extension contract; the current OpenRAC launcher does not implement it.

Runtime consumers use the C ABI below. Offline save tools can reuse the pure
`arsenal_state` and `arsenal_store` modules from `revolverator_core`: `load`
validates the native-save fingerprint and sidecar CRC; `save` atomically
replaces only the `.arsenal` sibling. Preserve unknown records and retain the
native Walloper state. Never write custom key65554 into native arrays. A future
editor changing native save bytes must load the matching sidecar first and
rebind it to the new native prefix, with backup/rollback of both files; a stale
sidecar must not be silently accepted. No launcher files are modified by this
mod or this API.

`include/revolverator_api.h` is the authoritative C ABI. Resolve
`openrac_revolverator_query` from the already loaded provider and request
version 2. The return type is the v1 pointer for ABI compatibility; cast the
successful version 2 result to `const revolverator_api_v2*`. Version 1 remains
available with its original member offsets. Unknown versions and unloaded
sessions return null. Do not load another copy of the plugin to discover it.

All calls run on OpenRAC's game thread. Outputs contain copied values, never
guest pointers. Clear borrowed service pointers when the provider unloads;
the Revolverator service has no unload subscription. `api->function` resolves
game functions, not this custom service. Windows consumers can use
`GetModuleHandleW(L"revolverator.dll")` and `GetProcAddress`; other platforms
must inspect an already loaded library handle.

## Identity and size checks

| Key | Meaning |
| --- | --- |
| `REV_NATIVE_WALLOPER_KEY` / `18` | Original native Walloper |
| `REV_REVOLVERATOR_KEY` / `65554` / `0x10012` | Independent logical Revolverator |

Never pass 65554 to native GiveItem, equip, pickup or item-table functions.
The adapter projects the logical weapon through native item 18 only for the
operation that needs it. `get_state().item_id` remains 18 as a transport field;
it is not the logical inventory identity.

Check `service->version`, each required pointer and its byte range before use.
Every output starts zeroed with `size = sizeof output`. Getters return 1 on
success and 0 for an unavailable session or invalid/undersized output. A failed
call does not supply a new snapshot.

```c
#include <stddef.h>
#include "revolverator_api.h"

#define REV_HAS(s, member) \
    ((s)->size >= offsetof(revolverator_api_v2, member) + sizeof((s)->member))

static void inspect(revolverator_query_fn query)
{
    const revolverator_api_v2* s = query
        ? (const revolverator_api_v2*)query(2u) : NULL;
    if (!s || s->version != 2u || !REV_HAS(s, get_progress) ||
        !s->get_inventory || !s->get_progress)
        return;

    revolverator_inventory_v2 weapon = {0};
    weapon.size = sizeof weapon;
    if (!s->get_inventory(REV_REVOLVERATOR_KEY, &weapon))
        return;

    revolverator_progress_v2 progress = {0};
    progress.size = sizeof progress;
    if (s->get_progress(&progress)) {
        /* weapon.owned, weapon.ammunition, progress.level, progress.xp */
    }
}
```

For a mandatory provider, a consumer manifest can require Revolverator>=0.2.0.
Optional consumers can use `load.after` and handle an absent service normally.
The library export map must include `openrac_revolverator_query` on Linux.

## v2 members

| Function | Contract |
| --- | --- |
| `get_state(out)` | Controller phase, target class, charge and session capture/throw/impact counters |
| `cancel()` | Cancel the current capture/flight; restore leased native state; no ammo refund or XP change |
| `get_mechanics(out)` | Revolverator ammunition/capacity and motor angle/speed, dry-fire and synchronization state |
| `get_inventory(key, out)` | Custom inventory snapshot; native keys 0..36 can also be inspected |
| `get_quick(out)` | Eight logical favourite keys, preserving distinct Walloper/Revolverator bindings |
| `grant(key, equip)` | Grant the registered Revolverator key through the native acquisition path; returns 1/0 |
| `get_progress(out)` | Revolverator level, total XP, current/next threshold, progress and effective optional multipliers |
| `poll_level_up(out)` | Pop one queued custom level transition; returns 0 when empty |

`grant` currently accepts only 65554. It changes ownership and optionally
equipment, does not perform a purchase or charge bolts, and is not an inspection
operation. There is no direct XP setter or arbitrary weapon-registration API.

`get_inventory` reports native transport ID, ownership, ammunition, capacity,
custom XP, icon bank, standard price and first-level availability. The native
Walloper ownership/economy values remain independent. Its `xp` field is not a
Weapon XP statistics query; read the Weapon XP service for native item 18 XP.

`get_progress` success means a snapshot is available. It does not assert that
Weapon XP is loaded, that the weapon is owned, or that a hit was accepted. Without
the optional provider, progression awards are disabled. Use reported thresholds
rather than hard-coded level boundaries. The normal base capacity is 20.

`poll_level_up` consumes a shared queue; multiple consumers must coordinate.
Events contain old/new level and total XP. The built-in presentation has its
own queue, so inspecting this API does not cancel its animation. Controller
impact counters alone do not prove damage, death or an XP award.

## Weapon XP ownership

The optional provider must expose v4 hit filters. The tested private companion
is `0.6.3-arsenal`; older providers cannot safely attribute a custom weapon
transport and are rejected when present. Standalone Revolverator remains valid.

The ownership filter copies hit receipts and excludes Revolverator hits from
native Walloper credit/scaling. The outer combat wrapper confirms accepted hits
or rejects suppressed ones. A matching native death awards the logical record
using the native enemy's bolt payout, global rate/cap and item 18 rules. Actor
identity, world epoch, 120-frame expiry and consumed-death checks reject stale or
duplicate credit. Quarter-XP remainders remain session state; total custom XP is
stored with the logical inventory in the native-save sidecar.

Damage/capacity upgrades default off. Charged throws apply their optional damage
multiplier in the world adapter; accepted native melee applies it once after
filtering. Repeated GetHit observations do not multiply it again. Custom level
presentation can use the provider's optional v4 acquisition tail without
changing the Walloper's level or XP.

Future ABI changes must preserve existing offsets. Append size-checked tails or
introduce a new query version; never expose C++ objects or guest RAM pointers.

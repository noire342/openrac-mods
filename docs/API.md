# Mod APIs

## Skill Points API v1

[`skill_points_api.h`](../skill-points-ra/include/skill_points_api.h) exposes a
copy-based read interface for the catalogue, unlocks and totals, plus achievement
and shutdown events. It cannot award achievements manually. Resolve the service
from the loaded producer and observe its game-thread/lifetime rules. The
[API guide](../skill-points-ra/docs/API.md) and MIT-licensed
[observer example](../skill-points-ra/examples/achievement_observer.c) document
size checks and optional integration. It has no dependency on Weapon Experience.

Cinematic Journeys exposes launcher settings and no additional public C service.

## Revolverator APIs v1/v2/v3

Resolve `openrac_revolverator_query(2)` from the already loaded provider and cast
the compatible v1 return pointer to `const revolverator_api_v2*`. Version 1 keeps
its original prefix. Check version, table size, required function pointers and
each caller-owned output's `size`; all calls run on the game thread.

| Key | Meaning |
|---|---|
|18 | Original Walloper |
|65554 / `0x10012` | Independent Revolverator |

Never pass 65554 to native item arrays or GiveItem. The adapter scopes native
transport18 internally. `get_state().item_id` remains the legacy transport field.

| Member | Copied data / operation |
|---|---|
| `get_state`, `get_mechanics` | Controller, charge/counters, ammunition/capacity and motor state |
| `get_inventory(key, out)` | Independent ownership/ammo/XP, native carrier, icons and economy |
| `get_quick(out)` | Eight logical keys; Walloper and Revolverator remain distinct |
| `grant(key, equip)` | Controlled custom grant through native acquisition; not a purchase |
| `get_progress(out)` | Level, XP, thresholds, progress and effective optional multipliers |
| `poll_level_up(out)` | Consume one logical level transition from the public event queue |

Base ammunition capacity is 20. Optional XP reaches level 10 and uses independent
state; without the XP provider progression awards are disabled. No arbitrary
third-party weapon registration or direct custom XP setter is exposed.

Query version 3 for the appended `get_upgrade` method: Golden ownership,
normal/Gold-Bolt prices, blast charge, release recoil and copied explosion/hit
counters. It preserves the v2/v1 prefixes. Gold is an upgrade of logical key
65554; it does not create another ammo/XP record. Initialize output sizes and
check the requested table version/size before reading any optional tail.

The [full contract and C example](../revolverator/docs/API.md) cover discovery,
size checks, lifetime, offline sidecar codec and the declarative
`extensions["noire342.arsenal"]` manifest metadata. That extension is a discovery
contract for future utilities; the current launcher has no implementation.

## Weapon Experience API v4

The intentionally MIT-licensed [`weapon_xp_api.h`](../weapon-xp/include/weapon_xp_api.h)
is the authoritative interop header. Resolve `openrac_weapon_xp_query` on an
already loaded module; do not load a second provider.

| ABI | Purpose |
|---|---|
|1 | Native weapon stats, explicit awards and events |
|2 | Idempotent co-op hit/death receipts with provenance; no network transport |
|3 | Copied rules/settings, revisioned configuration, reload, diagnostics and flush |
|4 | Hit-ownership filters and optional native acquisition presentation helpers |

The 0.6.4 source keeps older prefixes compatible. Filters exclude custom
hits before native Walloper credit/damage handling. Callbacks copy context and
must obey the [full API callback restrictions](../weapon-xp/docs/API.md).
[`hit_owner.c`](../weapon-xp/examples/hit_owner.c) demonstrates the v4 ownership
contract. Older 0.6.1 release binaries do not provide the required companion API.

Optional damage and ammunition bonuses are off by default. Configuration edits
are session-only and preserve earned XP. Never retain service pointers after
provider shutdown, and coordinate shared event consumers.

## Arsenal Compatibility API v1

The MIT [arsenal_hooks_api.h](../arsenal-compat/include/arsenal_hooks_api.h) exposes
`openrac_arsenal_hooks_query(1)` from an already loaded provider. Its table has
claim/bind/commit/rollback/diagnostics and pure `owns_id` discovery. Exactly one
consumer prepares all 30 typed bindings before activation. Other hook IDs remain
with the original SDK `api->hook`; the SDK table is never replaced.

`NativeRuntimeHooks` owns the Revo claim and rolls it back before unload or on
initialization failure. The compatibility mod requires the exact stock Windows
client hash; it is not a general multi-mod hook-chain service. The complete
[provider contract](../arsenal-compat/README.md) documents state, errors, teardown
and whole-stock-save-wrapper semantics.

## Private asset-file utility

The MIT [file_overlay_api.h](../arsenal-file-overlay/include/file_overlay_api.h)
exposes standalone install/remove/last_error/stats exports without requiring
SDK mod registration. The normal weapon package loads it from its own DLL
directory after staging absolute-path manifest aliases. The helper has no
independent launcher entry in the repository index.

`NativeAssetBundle` owns only a unique cache directory and known staged files.
`NativeCompatibilityModules` owns private DLL handles/initialization;
`NativeRuntimeHooks` owns its consumer claim and transaction. Teardown restores
session state, removes native hooks, removes the file alias, releases helpers,
then cleans owned staged files. Original asset manifests remain read-only.

## Composition

Declare dependencies/load order in each consumer manifest. Restore a native
override only while it is still owned by that adapter. A passing host test
checks the adapter contract; native gameplay and other-mod combinations need
their own evidence.

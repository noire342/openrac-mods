# Weapon Experience native C API (v1, v2, v3)

The MIT header `include/weapon_xp_api.h` is the authoritative plain-C contract. Resolve
`openrac_weapon_xp_query` from the already loaded provider; never load a second instance.
Declare `requires: [{"id":"weapon-xp","version":">=0.6.0"}]` for a v3 consumer.
The Windows observer demonstrates GetModuleHandle/GetProcAddress discovery. Other platforms
must use their platform's loader handle. Sandboxed Luau cannot call this native service.

`query(1)`, `query(2)` and `query(3)` return their own exact table sizes/versions; unsupported
versions return null. v3 contains unchanged v2 and v1 prefixes. Check both version and size.
Initialize each input/output struct's `size`. Calls and callbacks use the game thread.
Do not retain a service after STOPPING/provider unload; unsubscribe when the consumer unloads.

## Progression (v1)

- `weapon_count` / `weapon_id_at`: enumerate the twelve supported items.
- `get_stats`: copied XP/level/thresholds/effective multipliers, borrowed static English name.
  Always use reported thresholds. Levels are 1..10; storage supports up to 1e9 XP.
- `award_xp`: explicit integer reward, no inventory unlock/refill. Uses notifications and XP
  persistence. Combat multipliers are not applied to this caller-selected amount. Returns
  DISABLED when that weapon's XP multiplier is zero. Never truncates XP above a revised cap.
- `subscribe` / `unsubscribe`: up to 16 listeners. Queries and unsubscribe are allowed inside
  callbacks; awards, subscriptions and configuration mutations return BUSY. Exceptions from
  a listener disconnect it. No nested dispatch is performed.

CHANGED precedes LEVEL_UP for crossed thresholds. HIT is outgoing hit-record observation,
not proof a target accepted damage. A pending hit can be observed again on a later tick.
LEVEL_LOADED and STOPPING have item -1. Legacy V5_EFFECT=6 stays reserved and is never emitted.
CONFIGURATION_CHANGED=7 has item -1; query v3 for its new revision. Existing XP is unchanged.

## Co-op provenance (v2)

`begin_external_scope(session)` namespaces prepared/replicated events; zero clears it.
`prepare_local_hit` validates a local native ring hit, records an idempotent receipt and applies
the outgoing damage policy once. `confirm_external_death` awards only from that receipt,
once, using the same native-bolt cap/global XP rate/per-weapon XP weight as local deaths.
`mark_replicated_hit` skips local XP/scaling for the exact marked payload; a changed ring
payload is not suppressed. Unchanged markers expire after 8 ticks, prepared receipts after
600 ticks. Never serialize local guest addresses. HIT/output damage is before the target's
own native reaction, which may replace or reject it. This API is not transport/authentication.

## Configuration (v3)

- `get_configuration`: effective session flags, rate, per-kill cap, level count and revision.
- `get_rules(item)`: copies that weapon's ten thresholds, damage/ammo curves and XP weight.
- `set_configuration(config, revision)`: validates the full settings snapshot, then commits.
- `set_rules(array, count, revision)`: validates all 1..12 fixed-size v3 entries, rejects duplicate
  items, then commits the complete patch. Invalid values leave every weapon unchanged.
- `reload_rules(revision)`: reparses the configured file and replaces rules as one transaction.
- `flush`: atomically saves dirty XP now; never writes an original game-save slot.
- `last_error(buffer, capacity)`: copies the last rules/file diagnostic, returns required bytes
  including NUL. A short buffer is truncated and NUL-terminated. Status codes remain authoritative.

All mutations require the exact current revision. A stale revision returns STALE_REVISION;
read again and reconcile intentionally. Revisions are monotonic within one provider session.
Configuration edits return BUSY during acquisition/pending presentations, callbacks and active
co-op scopes. They cannot change the interpretation of an in-flight network receipt.
XP is never clamped when rules change. Capacity restores the last value actually written before
reapplying a different rule; enabling capacity never fills ammunition. Native HUD/names update
on the next frame. Damage/capacity flags default off. A successful mutation emits one config event.

Flags are 1 damage, 2 capacity, 4 HUD, 8 reduced motion, 16 acquisition animation,
32 versioned names, and 64 `WEAPON_XP_FLIP_MORPH_MODEL`. Flag 64 is available since
provider 0.6.1 and defaults off: it rolls only the held Morph-o-Ray model around its
firing axis, leaving native aim/beam/transformation unchanged. Require >=0.6.1 when
using it; it does not add Morph-o-Ray progression to the XP weapon catalog.

API mutations are session-only. They do not overwrite launcher settings or `rules.ini`.
The consumer owns its override: retain a snapshot and restore on unload only if the values/revision
still belong to it. A rules-file reload explicitly replaces API overrides with file/default values.
See `examples/rules_client.c`, compiled as C in the test build, for a minimal independent client.

Limits are documented in CONFIGURATION.md. New fields require an ABI revision or an explicitly
compatible appended tail; never alter existing v1/v2 layouts. The API exposes policies and events,
not raw writable game memory or private C++ state.

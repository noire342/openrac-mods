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
firing axis, retaining native aim/transformation code. Since 0.6.2 the native beam
origin follows the flipped muzzle. Require >=0.6.2 for that fix; >=0.6.1 when
using it; it does not add Morph-o-Ray progression to the XP weapon catalog.

API mutations are session-only. They do not overwrite launcher settings or `rules.ini`.
The consumer owns its override: retain a snapshot and restore on unload only if the values/revision
still belong to it. A rules-file reload explicitly replaces API overrides with file/default values.
See `examples/rules_client.c`, compiled as C in the test build, for a minimal independent client.

## Custom hit ownership (v4, 0.6.3-arsenal preview)

`query(WEAPON_XP_ABI_V4)` returns `weapon_xp_api_v4`. Its complete v1/v2/v3 prefix is unchanged;
the returned base reports ABI 4 and the full v4 size. Querying an earlier ABI still returns its
original version and size. This preview is an isolated companion build for the additive arsenal.

`register_hit_filter(callback, user, &token)` installs one of at most 16 synchronous ownership
filters. The copied 64-byte context includes target/source classes, candidate native item,
directional origin class, raw damage, native hit address, tick and a reset epoch. Origin class
is -1 unless the native directional-hit tag is present. An address is valid only during the
callback; the epoch is a runtime lifecycle identifier, not a save/profile or persistent actor ID.

Return `WEAPON_XP_HIT_EXTERNAL` for packets owned by a custom weapon using a native item as
transport. The first claiming filter stops classification. Weapon XP forgets pending native
credit for that target, emits no native HIT event and leaves native damage unscaled. If the
same packet had already been scaled by this provider, its unchanged owned edit is restored.
The native hit remains usable. This callback observes a hit record before reaction; it does
not prove the enemy accepted damage or died. The custom owner must verify those events and
maintain its own progression. A filtered v2 `prepare_local_hit` returns DISABLED and creates
no native receipt; this classification does not alter the global co-op scope.

Callbacks run on the game thread. Do not mutate guest memory, recurse into native game calls
or call XP/configuration mutations. Read queries are allowed. Registering another filter and
mutating XP/configuration return BUSY during dispatch. `unregister_hit_filter(token)` is safe
inside a callback, including self-unregistration. An exception disconnects that callback and
claims the current packet to prevent accidental native credit. A forbidden recursive hit is
also classified external. Registrations survive level changes and end with the provider runtime.

Keep the provider loaded while retaining its API pointer. Unregister before unloading the
consumer so no callback points into unloaded code. Tokens are never reused in one runtime;
duplicate callback/user registrations are rejected. See `examples/hit_owner.c`, compiled as
C11 with fixed-layout assertions, for a minimal consumer independent of SDK/C++ types.

The optional v4 tail adds `request_acquisition_presentation(native_item)` and
`acquisition_presentation_active()`. Before reading either pointer, require `base.size >=
offsetof(weapon_xp_api_v4, member) + sizeof(api->member)`; a hit-filter-only v4 provider has
the same prefix and a shorter size. The request queues one supported native weapon's original
acquisition scene without awarding XP, changing its native level/caption or emitting XP events.
The caller projects the logical weapon's custom model/name while the request is pending.

Request on a normal game frame after the custom progression owner has confirmed a level-up,
never inside an event/filter callback. OK means queued; BUSY means a scene is already pending
or a callback is executing; DISABLED means the animation setting is off or that carrier has no
native acquisition archive. Unsupported item IDs return INVALID_ARGUMENT. The active getter
returns 1 while that external request is queued, preparing, playing or waiting for its native
actors to retire, then 0. It does not report unrelated native XP presentations. Both calls return
NOT_READY when the provider runtime is absent. The existing quiet-input phase, player-body
visibility guard and two-frame actor retirement are shared with native level-up scenes.

Limits are documented in CONFIGURATION.md. New fields require an ABI revision or an explicitly
compatible appended tail; never alter existing v1/v2 layouts. The API exposes policies and events,
not raw writable game memory or private C++ state.

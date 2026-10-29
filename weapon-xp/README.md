# Weapon Experience 0.6.1

Twelve weapons gain ten persistent levels. The default is progression only: original
damage, ammo capacity, projectiles, aiming, colors, particles and weapon behavior.
All former level-based special abilities and cosmetic weapon modifications have been removed.
An independent, opt-in Morph-o-Ray model toggle is available; it defaults to off.

XP remains inside the original weapon HUD, including the Suck Cannon's vertical
gauge. Localized names use v2 through v10. Level increases retain the native acquisition
presentation. RYNO has no short retail acquisition scene and uses its native banner.
The mod does not unlock weapons or refill ammunition.

## Extensible rules and API v3

Rules are data, independent of game-memory adapters. Every weapon can have its own
ten XP thresholds, ten damage/ammo multipliers and integer XP weight (0 pauses awards).
Edit `rules.ini` beside the XP profiles; omitted values retain shipped defaults.
Invalid files are rejected as a whole. See [Configuration](docs/CONFIGURATION.md).

The existing launcher also controls the per-kill XP limit, native level-up animation
and vN names. There is no new in-game panel. Default behavior matches 0.5.0.

Public ABI v3 exposes copied rules/settings, atomic batch edits, revision checks,
file reload, diagnostics and explicit XP flush. ABI v1/v2 consumers remain supported.
API edits are session-only, work on the game thread and preserve earned XP. Rule
mutations wait until acquisition and active co-op scopes end. See [API](docs/API.md)
and the compiled plain-C [consumer example](examples/rules_client.c).

## Settings

Both optional bonuses are **off by default**, independently configurable in the
launcher's existing mod settings. Restart the game after changing settings.

| Level | Total XP | Optional damage | Optional ammo capacity |
|---|---:|---:|---:|
| 1 | 0 | 1x | 1x |
| 2 | 400 | 1.15x | 1.125x |
| 3 | 1400 | 1.30x | 1.25x |
| 4 | 3200 | 1.50x | 1.375x |
| 5 | 6000 | 1.75x | 1.50x |
| 6 | 10000 | 2x | 1.625x |
| 7 | 15500 | 2.25x | 1.75x |
| 8 | 23000 | 2.50x | 1.875x |
| 9 | 33000 | 2.75x | 2x |
| 10 | 46000 | 3x | 2.125x |

`scale_damage` and `scale_capacity` each default to false. Without these switches,
all multipliers are 1 at every level. Capacity rounds upward and never grants ammo.
`xp_rate` defaults to 1 (0.25 to 4); `hud` defaults to true; `reduced_motion` to false.

`flip_morph_model` defaults to false. When enabled, the held Morph-o-Ray is rolled
180 degrees around its firing axis: upside down, with the barrel still toward the
enemy. Native targeting, beam and transformation run before the render-only change.
Switching it off restores the original pose. This is a cosmetic option, independent
of XP levels; the Morph-o-Ray is not part of the twelve-weapon XP catalog.
The attribution to an original concept drawing has not been independently confirmed.

Attributed enemy deaths award XP from native nominal bolt payouts, including the
game's depleted source budgets. Crates and scenery do not grant weapon XP. The
per-kill cap is 500 XP before the rate multiplier. Small fractional rewards persist.

## Existing progress and compatibility

The first five thresholds are unchanged. Existing format-2 profiles keep exact XP,
levels and fractional XP. An old maxed V5 stays V5 at 6000 XP and can continue to V10.
Format-3 profiles also retain exact XP, including maxed level-10 weapons.
Format-1 profiles preserve their previous level and relative progress on the first
five stages. New writes use format 4, with the untouched old profile backed up as
`.v1.bak`, `.v2.bak` or `.v3.bak` before migration. Older mod versions cannot read format 4. The storage limit is independent of the
configured curve: lowering thresholds never erases previously earned XP.

Profiles are independent of game-save slots. Use a separate profile name for each
campaign; loading an older save does not roll back XP. PAL and NTSC are separate.
Windows location: `%LOCALAPPDATA%/OpenRAC/weapon-xp/`. Checksums, atomic replacement
and exclusive locking protect profiles. Dirty progress saves every 300 ticks,
on planet changes and normal shutdown. Forced termination may lose unsaved XP.

The mod needs no OpenRAC source changes. Six hooks observe native hit/death events,
planet lifecycle, the existing HUD relay, acquisition input, and the optional held
Morph-o-Ray pose. No projectile, ammo-consumption or particle modifications remain.
The default mode can observe other damage mods; enabling optional bonuses alongside
another damage/capacity mod requires separate composition testing.

Public C ABI v1 and co-op ABI v2 retain their layouts and function contracts; v3
adds configuration access without changing either previous prefix.
`get_stats` reports effective multipliers (1 when disabled) and current thresholds.
Legacy V5 effect event number 6 stays reserved and is never emitted. See
`include/weapon_xp_api.h`, `docs/API.md` and `examples/xp_observer.c`.

## Build and validation

Build using CMake and the pinned OpenRAC mod SDK 1.3, then run CTest in Release.
Twelve test groups cover ten level boundaries, independent bonuses, both-disc DLL
integration, replay-safe co-op receipts, fractional persistence, format-1/2 migration,
native HUD geometry, acquisition lifecycles and isolated model rotation/restoration.
Native runtime evidence is separate
from simulated-host tests; see the release validation receipt for the exact DLL.
Native PAL gameplay and a complete campaign playthrough have not been validated.

Progression, combat, persistence, events, UI, native adapters and service integration
remain separate modules. The removed effect planners/renderers are absent from this
source release. QA helpers and test save files are never included in the game ZIP.
Install the ZIP through the launcher's Mods page and restart the game.

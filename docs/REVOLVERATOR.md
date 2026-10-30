# Revolverator 0.2 — preview source

The [current implementation](../revolverator/README.md) adds the removed drill
weapon alongside the original Walloper. They have independent ownership,
ammunition, favourite bindings and progression.

| Identity | API key | Native transport |
|---|---:|---:|
| Original Walloper |18 |18 |
| Revolverator |65554 (`0x10012`) |18 only inside scoped native operations |

The custom key is never used as a native array index. Render class 2017 belongs
to the drill; class 180 retains the original Walloper model. Pickup/render aliases
32000 and 2016 use native ammunition 225 without changing the original item arrays.

Hold Circle for at least 0.18 seconds near an eligible enemy to catch and lift
it at the contact point, spin it above the tip and release to throw. A quick
tap performs the native melee response. Bosses and other uncapturable targets
keep their melee response. The tip turns slowly at rest, matches the captured
enemy's spin and stops without ammunition; an empty trigger produces a short jerk.

The weapon is offered after visiting Hoven for 15,000 bolts. Capacity is 20,
ordinary pickups give 2 and vendor ammunition costs 25 bolts per round. The
existing HUD shows an authored cartridge icon; inventory/quick selection use
the drill glyph. Model and texture recipe for the cartridge were created with
AI assistance, as described in [Credits](../CREDITS.md).

The native weapon and quick-select editors use 4×5 visible cells, scrolling
beyond 20 entries and the original eight favourites. Both weapons can be bound
at the same time. No extra inventory panel is introduced.

## Runtime, progression and saves

The [GPL arsenal bridge patch](arsenal-bridge/README.md) on OpenRAC base
`77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225` is required. A DLL and ordinary model
replacement pack alone are insufficient. The [asset builders](../revolverator/docs/BUILD_AND_ASSETS.md)
create local extended manifests while preserving native class 180.

Optional Weapon Experience integration requires API v4; the matching source
here is 0.6.3-arsenal. Custom hits are excluded from Walloper awards, and the
Revolverator owns a separate XP record. Level progression reaches 10; optional
bonuses remain off by default. An older XP provider lacking the ownership filter
is rejected when loaded alongside the development weapon.

Native saves retain original item IDs. Revolverator state and logical favourites
are saved atomically in `saveN.bin.arsenal`, bound to the exact native save's
fingerprint and protected by a CRC. Stale or malformed sidecars are not applied.
The public [API v2](../revolverator/docs/API.md) exposes copy-only inventory,
progress, quick slots and controlled grant operations.

## Verified scope and development status

[Native evidence](../revolverator/docs/NATIVE_VALIDATION.md) distinguishes real
combat XP, vendor/pickup checks, save round trips, menus and the limited-memory
HUD runs on Rilgar, Nebula G34 and Quartu. The last three are standalone gameplay
smokes, not full level completion or packed-level menu coverage.

The development source builds without original game art, using native glyphs
and the authored cartridge data. Optional 128px private glyphs fall back to 32px
when presentation memory is limited. Original drill art and the optional HD
header remain local outputs and are excluded from public source and CI artifacts.

Preview packages and exact setup instructions are published separately through
[Releases](https://github.com/noire342/openrac-mods/releases). This is not a final release.
PAL, a full campaign, every capture target/boss, interruption and mod combination
remain outside the verified scope. Historical 0.1 replacement behavior and its
10-charge evidence are not the behavior of the 0.2 source.

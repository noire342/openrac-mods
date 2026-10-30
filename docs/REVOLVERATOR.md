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

The [unchanged official Windows x64 NTSC-U client](COMPATIBILITY.md) is used
with privately bundled utility DLLs. The provider supplies 30 typed in-memory
boundaries through a versioned mod API; Revolverator retains 24 ordinary SDK
hooks. No custom executable or engine patch is required. The [asset builders](../revolverator/docs/BUILD_AND_ASSETS.md)
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

[Native evidence](../revolverator/docs/NATIVE_VALIDATION.md) separates current
stock-client checks from historical modified-runtime results for real
combat XP, vendor/pickup checks, save round trips, menus and the limited-memory
HUD runs on Rilgar, Nebula G34 and Quartu. The last three are standalone gameplay
smokes, not full level completion or packed-level menu coverage.

The development source builds without original game art, using native glyphs
and the authored cartridge data. Optional 128px private glyphs fall back to 32px
when presentation memory is limited. Original drill art and the optional HD
header remain local outputs and are excluded from public source and CI artifacts.

The native [preview.1 release](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.2.0-preview.1)
uses `revolverator-0.2.0-windows-x86_64.zip`: paste the release-tag URL into the
normal Mods page or import this native ZIP. Assets and utility DLLs load
automatically from the installed weapon package.
The [stock native receipt](validation/revolverator-0.2.0-stock-native.json) records
trial, commerce, earned XP, save/reload and native menu checks. This is not a final release.
PAL, a full campaign, every capture target/boss, interruption and mod combination
remain outside the verified scope. Historical 0.1 replacement behavior and its
10-charge evidence are not the behavior of the 0.2 source.

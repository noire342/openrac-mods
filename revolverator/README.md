# Revolverator 0.3.4 (development)

By **lorenzogarrone**. The Italian in-game name is **Rotatore**.

Restores the cut Revolverator as a separate Ratchet & Clank weapon: strike,
catch, spin and throw enemies. It uses the game's weapon menus, ammunition
HUD, acquisition presentation and save flow. The original Walloper remains a
separate weapon.

Version 0.3.4 adds charge-gated throws and a deliberate early ground drop.
The development runtime keeps the original game sounds; the custom
sound experiment is parked in source only. The published
[0.3.3 release](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.3.3)
remains available. The local 0.3.4 candidate passed 38 Release host test groups
and 81 targeted native PAL checks across the base and Golden variants. The
[native validation receipt](docs/validation/native-audio-0.3.4.json) identifies
that exact binary and its tested scenarios. These source changes do not publish
a new installable release.

Native metal sparks follow the drill's contact with a spinning enemy. Their
rate follows the rotation speed. The weapon's models are prepared
automatically from the player's installed disc data, so the public package
contains no copied game models, textures or HUD artwork.

## Install

1. Use the official OpenRAC launcher on Windows x64, with **mod interface 1.12
   or newer** and your own extracted PAL or NTSC-U copy of Ratchet & Clank.
2. In **Mods**, add [this repository](https://github.com/noire342/openrac-mods),
   choose **Revolverator**, and install it. Alternatively, import
   revolverator-0.3.4-windows-x86_64.zip with **File** when this update is released.
3. Enable the mod and start the game. The first start prepares its local
   graphics cache automatically. No separate Python installation, graphics
   pack, utility mod card or replacement client is required.

To try the weapon immediately, enable **Revolverator unlocked from the start**
in its settings. It grants the weapon once with 20 rounds if it is not already
owned. Existing ammunition and progress are preserved.

The requirement is a minimum interface and the capabilities the mod uses,
rather than one fixed OpenRAC version. Later clients can work when those
capabilities remain available. The release receipt identifies the exact client
used for testing; it is evidence, not an executable-version requirement.
See [compatibility](docs/COMPATIBILITY.md).

If OpenRAC asks to refresh the extracted levels, use **Check and play** in the
official launcher. Preparation errors are reported in the mod's load status
and local log; players do not need to run build commands.

## Controls and balance

Tap **Circle** for a melee strike. Hold it continuously for at least 0.18 seconds
to catch an eligible enemy. Once captured, keep holding until the existing
charge bar is full, then release to throw. The bar fills over 1.6 seconds of
spinning; an uninterrupted hold still throws automatically after three seconds.
Releasing before the displayed bar is full drops the live enemy onto the ground
in front of Ratchet, returning it to native enemy behavior and its counterattack.
That early drop does not launch a projectile, trigger a Golden explosion,
play the throw animation or refund the round. See [charge release](docs/CHARGE_RELEASE.md).
Ratchet moves at up to 50% of his native
horizontal speed while holding an enemy; the limit ends when the target is
released. A brief 150 ms movement hold gives the throw weight.

| Property | Value |
| --- | ---: |
| Normal availability | After visiting Hoven |
| Weapon price | 15,000 bolts |
| Capacity | 20 rounds |
| Cost per activation | 1 round |
| Normal ammunition pickup, when owned | 2 rounds, capped at 20 |
| Vendor price per round | 25 bolts |
| Golden upgrade | 50,000 ordinary bolts, no Gold Bolts |

Tanks, Toxic Crabs and the beige armored Alien Snapper cannot be caught. They
still receive native Walloper-style melee damage, including the game's armor
and vulnerability rules. **Blarg Commanders can be caught.** Bosses remain
melee targets. See [enemy eligibility](docs/HEAVY_ENEMIES.md).

The drill turns slowly at rest, stops when empty, and follows the captive's
rotation while spinning. An empty activation produces a short failed motor
jerk. The attack pulse briefly enlarges only the head; the head stays at its
normal size while carrying an enemy. Selecting the weapon builds its housing
first and screws the drill into place over 0.6 seconds. Reopening the weapon
menu without changing equipment does not replay that presentation.

The Golden upgrade is sold in an additional case in Novalis's native Gold
weapon shop. It shares ammunition, XP and quick-select bindings with the base
weapon. Thrown enemies explode on collision; the blast grows with spin time
up to a fixed cap. Switching weapons does not change a shot already in flight.

## Native menus, saves and optional XP

The weapon and its ammunition use the existing UI. The cartridge appears in
the gameplay ammunition HUD; the drill glyph appears in inventory and quick
select, with a Golden variant for the upgrade. The weapon grid has 20 visible
cells and scrolls for additional entries. The original eight quick-select
slots can hold Walloper and Revolverator independently.

| Identity | API key |
| --- | ---: |
| Original Walloper | 18 |
| Revolverator | 65554 / 0x10012 |

The native save retains the original items. Custom ownership, ammunition,
Golden upgrade, XP and favorites use an atomic saveN.bin.arsenal sidecar bound
to the matching native save. Missing, corrupt or mismatched sidecars are not
applied. PAL and NTSC-U cards are separate; automatic cross-region save
conversion is not provided.

Weapon Experience is optional. A compatible **API v4** provider gives this
weapon its own XP total and up to ten levels. The original Walloper's progress
stays separate. Damage and ammunition-capacity bonuses remain off by default.
The mod-to-mod interface is documented in [API.md](docs/API.md).

## Assets and attribution

The weapon model, Golden finish and native icon variants are derived locally
from the player's own installed assets. The ammunition model and texture are
original artwork created with AI assistance. The public release discloses
that AI tools helped develop the mod.

The runtime uses the game's original sounds and ships no custom WAV files.
The experimental procedural sound set, its generators, license and hash
provenance remain in the development source for possible later work. It is not
connected to the weapon or loaded by the current runtime. No extracted game
audio is distributed. See [audio status](docs/AUDIO.md).

Authored code is [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 lorenzogarrone.
Third-party code retains its original notices. The code license does not
relicense Insomniac Games / Sony's game artwork. See
[runtime assets](docs/RUNTIME_ASSETS.md) and [packaging](docs/PACKAGING.md).

## Documentation and validation

- [Contact sparks](docs/CONTACT_SPARKS.md)
- [Charge-gated release](docs/CHARGE_RELEASE.md) and [audio status](docs/AUDIO.md)
- [Compatibility and later clients](docs/COMPATIBILITY.md)
- [Build instructions and asset preparation](docs/BUILD_AND_ASSETS.md)
- [Native validation and historical receipts](docs/NATIVE_VALIDATION.md)
- [Golden upgrade](docs/GOLDEN_UPGRADE.md) and [impact effects](docs/GOLDEN_EFFECTS.md)
- [Visual ownership](docs/VISUAL_PRESENTATION.md) and [weapon menu](docs/WEAPON_MENU_AUDIT.md)
- [Publication checklist](docs/PUBLICATION.md)

Validation is specific to the recorded binary, region and scenarios.
Historical receipts stay associated with their original builds; they do not
establish whole-campaign coverage or every combination of mods.

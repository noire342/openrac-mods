# Revolverator

By **noire342**. The Italian in-game name is **Rotatore**.

Restores the cut Revolverator as a separate Ratchet & Clank weapon: strike,
catch, spin and throw enemies. It uses the game's weapon menus, ammunition
HUD, acquisition presentation and save flow. The original Walloper remains a
separate weapon.

Version 0.3.5 uses OpenRAC's official Gadgetron icon-strip hook from interface
1.20 onward and keeps the adapter for earlier supported interfaces. It also
scopes equipment changes during acquisition scenes and save operations to
the mod's own state. Challenge Mode carries over the Rotatore's Golden upgrade,
ammunition, XP and favourite when the slot is available.

The [0.4.8 release page](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.4.8)
includes the installable Windows package, corresponding source and validation
receipt. This version integrates with [Weapon Experience 0.7.0](https://github.com/noire342/openrac-mods/releases/tag/weapon-xp-v0.7.0)
through API v5 for shared XP rules and localized level names. Existing API v4
providers are supported.

Version 0.4.7 fixed the ammunition HUD when Ratchet switches to his wrench
or dies. Regular enemies such as the Hoven Anklebiter can be caught, and Hoven
Blarg troopers keep their rifles through the spin and throw.
Thrown enemies stop at terrain, and the Gadgetron retains the Revolverator's
price and preview after a checkpoint. Invalid Quick Select entries are
discarded when loading stock inventory.

Version 0.4.6 fixed the Walloper label and selection in glawks'
[Expanded Quick Select](docs/COMPATIBILITY.md#optional-expanded-quick-select-integration)
when the Revolverator is equipped. Walloper selection works in either mod load
order.

Native metal sparks follow the drill's contact with a spinning enemy. Their
rate follows the rotation speed. The Golden upgrade uses the green spark
tint from the Golden Devastator. The rear valve pulses while the drill turns,
venting a faint wisp at idle and darker smoke under load. It stays quiet during
an empty trigger pull. The weapon's models are prepared
automatically from the player's installed disc data, so the public package
contains no copied game models, textures or HUD artwork.

## Install

1. Use the official OpenRAC launcher on Windows x64, with **mod interface 1.12
   or newer** and your own extracted PAL or NTSC-U copy of Ratchet & Clank.
2. Download `revolverator-0.4.8-windows-x86_64.zip` from that release and import
   it with **Mods → Add a mod → File**. If the catalog already shows 0.4.8,
   you can install or update from there.
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
spinning. Releasing at full charge starts the native Walloper strike immediately.
The enemy detaches from the drill tip at the end of Ratchet's forward drive.
An uninterrupted hold starts the same strike automatically after three seconds.
A new press is accepted after the native attack has recovered.

The drill tip starts to glow near the end of the first automatic spin. Once
heated, successive charged enemy throws spread the glow up the spiral. Four
quick throws from the first hot release can cost Ratchet one point of health
after the throw animation. Cooling reduces the retained heat, so a gap between
throws can raise the number needed to overheat. A fully heated drill cools in
about five seconds after a short carry-over window. Only enemy throws add heat.

The Rotatore can also lift and throw one ordinary crate. The strike
still hits the other crates in a stack, and explosive crates keep their native
detonation behavior. A thrown crate breaks on impact or at the native fall
height over a void. Scripted respawning and platform-mounted crates stay under
level control.
Releasing early puts the enemy on the ground in front of Ratchet and restores its
normal behavior. The **Immediate counterattack on an early drop** setting is off
by default and can restore the immediate hit. See [charge release](docs/CHARGE_RELEASE.md)
for the original release implementation.
Ratchet moves at up to 50% of his native horizontal speed while spinning an
enemy. Crates allow full movement speed. Jumping is blocked while either target
is held. The native Walloper lunge takes over when Ratchet starts the throw.

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

The native save retains the original items. The mod stores custom ownership,
ammunition, the Golden upgrade, XP and favorites in its OpenRAC data folder.
Each record is bound to the matching native save. At startup, the mod moves
legacy `saveN.bin.arsenal` files out of the active card before the game lists
its slots. Enable this version for one launch to recover an affected card.
The migration preserves the native save and archives any legacy file that
conflicts with an existing mod record. PAL and NTSC-U cards keep separate records.

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

Authored code is [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342.
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

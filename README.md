# OpenRAC Mods

[![Build and test](https://github.com/noire342/openrac-mods/actions/workflows/build.yml/badge.svg)](https://github.com/noire342/openrac-mods/actions/workflows/build.yml)

My Ratchet & Clank mods for the native OpenRAC port, using the existing HUD,
menus and native presentation.

**This branch contains current preview source. Revolverator 0.2 adds a separate
weapon and preserves the original Walloper**, including its ownership,
function, favourites and progress. Revolverator has its own 20-round inventory,
XP and save sidecar.

| Component | Current source | Download status |
|---|---|---|
| [Revolverator](revolverator/README.md) | **0.2.0 PREVIEW source**, additional weapon and API v2 | Preview packages are published separately through Releases |
| [Weapon Experience](weapon-xp/README.md) | **0.6.3-arsenal PREVIEW companion**, APIs v1–v4 and custom-hit ownership | Latest published stable download remains **0.6.1** |

The earlier Revolverator 0.1 source preview reused the Walloper slot as a
replacement. That historical implementation is superseded by the additive 0.2
source on this branch. See [current scope](docs/REVOLVERATOR.md).

## Stable Weapon Experience download

Download `weapon-xp-0.6.1-windows-x86_64.zip` from
[Releases](https://github.com/noire342/openrac-mods/releases), import it through
the OpenRAC launcher's Mods page and restart the game. Its existing release
archives are unchanged. Damage and ammo bonuses are off by default.

Stable 0.6.1 can run independently on its supported runtime. The preview
Revolverator needs the matching API v4 companion source here when XP is enabled;
0.6.1 is not that companion. Revolverator also runs without Weapon Experience.

## Preview setup

Revolverator 0.2 requires the [arsenal bridge source patch](docs/arsenal-bridge/README.md)
on the documented OpenRAC base. Its 30 typed hooks cover native inventory,
previews, vendor transactions, drops and save lifecycle. All weapon policy
stays in the mod. A stock executable without these hooks cannot run the 0.2 mod.

The drill and optional upscaled icon are generated from the player's own
NTSC-U extraction. They are excluded from this repository. The original
procedural ammunition recipe and its authored 32px HUD data are included.
Build and asset preparation are documented in [Building](docs/BUILDING.md) and
[the local asset workflow](revolverator/docs/BUILD_AND_ASSETS.md).

| Revolverator economy | Base value |
|---|---:|
| First vendor availability | Visit Hoven |
| Weapon price | 15,000 bolts |
| Capacity | 20 rounds |
| Pickup | 2 rounds, capped at 20 |
| Ammo unit price | 25 bolts |

`start_unlocked` is off by default. Enable it for an immediate trial grant;
normal progression retains Hoven availability and the 15,000-bolt purchase.

The native weapon panel shows 16 weapons in a 4×5 grid, with scrolling prepared
for larger catalogs. The original eight quick slots remain; Walloper and
Revolverator can occupy separate favourites.

## Developer information

- [API guide](docs/API.md): Revolverator API v2, Weapon Experience API v4 and compatibility.
- [Build and bridge requirements](docs/BUILDING.md).
- [Installation and development status](docs/INSTALLATION.md).
- [Validation and limits](docs/VALIDATION.md).
- [Credits and asset provenance](CREDITS.md).

Native gameplay evidence is Windows NTSC-U. PAL gameplay, complete campaigns,
all bosses and every mod combination have not been verified. Host builds on
Linux do not establish native Linux gameplay coverage. No runtime executable,
ISO, original game art, save or RAM dump is included in this source update.

## License

Mod implementations are [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342,
following the upstream mod repository's convention. OpenRAC's SDK and the
intentionally MIT-licensed Weapon Experience interoperability header/examples
retain their own notices. Engine files and game assets keep their respective
rights. See [licensing scope](docs/LICENSING.md).

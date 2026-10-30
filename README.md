# OpenRAC Mods

[![Build and test](https://github.com/noire342/openrac-mods/actions/workflows/build.yml/badge.svg)](https://github.com/noire342/openrac-mods/actions/workflows/build.yml)

My Ratchet & Clank mods for the native OpenRAC port. I want them to feel like part
of the original game: the existing HUD, native presentation, and settings that
make optional changes explicit.

| Mod | Status | What it does |
|---|---|---|
| [Weapon Experience](weapon-xp/README.md) | **0.6.1 available** | Ten persistent weapon levels, native HUD integration and a versioned C API. Damage and ammo bonuses are off by default. |
| [Revolverator](revolverator/README.md) | **0.1.0 source preview** | Capture, spin and throw reconstruction with native ammunition/HUD, localized text and a C API. Build code and generate required art locally. |

Download Weapon Experience from [Releases](https://github.com/noire342/openrac-mods/releases).
Each mod is distributed separately. Revolverator is currently available as
source and local asset-building tools; no Revolverator binaries or game-derived
art are published. See its [current scope and limitations](docs/REVOLVERATOR.md).

## Install Weapon Experience

1. Download `weapon-xp-0.6.1-windows-x86_64.zip` from its release.
2. Add the ZIP through the OpenRAC launcher's **Mods** page and enable it.
3. Select a separate XP profile for each campaign, then restart the game.

The Windows ZIP has the manifest at its root and contains only the native mod,
license and player documentation. You do not need to compile or modify OpenRAC.
See [Installation](docs/INSTALLATION.md) for settings, profiles and compatibility.

## For developers

- [Build from source](docs/BUILDING.md)
- [API guide](docs/API.md)
- [Weapon Experience configuration](weapon-xp/docs/CONFIGURATION.md)
- [Revolverator API](revolverator/docs/API.md) and [asset/build workflow](revolverator/docs/BUILD_AND_ASSETS.md)
- [Validation and compatibility](docs/VALIDATION.md)
- [Credits and asset provenance](CREDITS.md)

Code is organized into small controllers, native adapters, presentation and
persistence modules. Public interfaces use versioned C tables; other mods do not
need to link against private implementation code.

## Compatibility

Weapon Experience declares PAL and NTSC-U support. Native gameplay evidence is
currently **Windows NTSC-U**; PAL has host-test coverage, not a completed native
playthrough. Revolverator currently targets NTSC-U only. No complete campaign
playthrough is claimed for either mod.

This is a personal mod project, independent of the OpenRAC team, Insomniac Games
and Sony Interactive Entertainment. Game assets and game files are not included.

## License

Project code is GPL-3.0-or-later unless a file says otherwise. The public Weapon
Experience API header is MIT-licensed. Retain the notices in copied source files.

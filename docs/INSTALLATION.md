# Installation

## Weapon Experience 0.6.1

Use the Windows x86-64 ZIP from the release. Import it through the OpenRAC
launcher's Mods page, enable Weapon Experience, and restart the game. The ZIP
includes a root `openrac-mod.json` and the DLL under `lib/windows-x86_64/`.

The default is progression only. Weapon damage, ammunition capacity, projectiles,
aiming and effects remain native. The launcher's existing mod settings separately
enable damage scaling and capacity scaling. Both are off by default.

XP appears inside the original weapon HUD. The Suck Cannon retains a vertical
layout. Weapon names use v2 through v10, and level-ups use the native acquisition
presentation where the original game provides one.

### Profiles and saves

XP profiles are independent of the game's save slots. Choose a different profile
name for each campaign. Loading an older game save does not rewind XP.

On Windows, profiles are stored in `%LOCALAPPDATA%/OpenRAC/weapon-xp/`. The mod
uses checksums, an exclusive profile lock and atomic replacement. It saves dirty
XP periodically, on planet changes and on normal shutdown. Forced termination
can lose progress since the previous write.

Version 0.6.1 reads older supported profile formats and writes format 4. Migration
preserves earned XP and creates a backup of the older file. Older mod releases
cannot read format 4. Back up profiles before downgrading.

### Compatibility

The manifest declares PAL and NTSC. Windows NTSC-U has native runtime evidence.
PAL has host-test coverage only; a native PAL campaign has not been verified.
Other mods that change damage or ammunition capacity need composition testing
when the optional bonuses are enabled.

No original game files or OpenRAC source changes are required by Weapon Experience.

## Revolverator

Revolverator is still being integrated and tested. It replaces the native
Walloper slot while enabled and currently targets NTSC-U. No finished download
is advertised in this revision.

Its gameplay module and graphics are distinct. The original drill companion is
built locally from the player's extracted game assets; original models/textures
are not redistributed here. A custom ammunition render class additionally needs
the generated extended-level manifests. The present ordinary launcher model-pack
importer cannot add an absent class by itself.

The final preview instructions must name the exact asset builder and `--levels`
output to use. Importing only a DLL ZIP must not be described as sufficient for
the full custom-art setup.

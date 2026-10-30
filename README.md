# OpenRAC Mods

[![Build and test](https://github.com/noire342/openrac-mods/actions/workflows/build.yml/badge.svg)](https://github.com/noire342/openrac-mods/actions/workflows/build.yml)

My Ratchet & Clank mods for the native OpenRAC port. Revolverator adds a separate
weapon alongside the original Walloper, with native menus, 20 rounds and its own
progress. Weapon Experience adds ten persistent weapon levels.
Skill Points: RetroAchievements brings the achievement set into the native
Skill Points menu. Cinematic Journeys prevents accidental cutscene skipping.

## Install Skill Points and Cinematic Journeys

Add [this repository](https://github.com/noire342/openrac-mods) in the launcher's
Mods page, select the mod, install it and enable it. Local ZIP import is also
available through the individual releases:

| Mod | Download | Behavior and scope |
|---|---|---|
| [Skill Points: RetroAchievements](skill-points-ra/README.md) | [0.1.3 Windows x64](https://github.com/noire342/openrac-mods/releases/tag/skill-points-ra-v0.1.3) | NTSC-U; 178 offline achievements integrated into Pause → Goodies → Skill Points; Kerwan timed challenge off by default, giving 177 active |
| [Cinematic Journeys](cinematic-journeys/README.md) | [0.1.1 Windows x64](https://github.com/noire342/openrac-mods/releases/tag/cinematic-journeys-v0.1.1) | Protects cutscenes; deliberate skip with L1+L2+R1+R2+Start; configurable 5-second minimum flight; NTSC-U tested, PAL adapter included but not tested in-game |

These two standalone mods are verified on the official **OpenRAC 0.10.0 Windows NTSC-U** client. See [the exact compatibility evidence](docs/validation/openrac-0.10.0.json). They need no modified OpenRAC client. Skill Points works offline without
an account or server submissions. It stores its own progress independently from
the game's memory cards. Load it after other level-loading hooks, including XP,
if managing `plugins.json` by hand. Full details and validation limits are in the
module READMEs and [validation notes](docs/VALIDATION.md).

## Install the Revolverator preview

1. Set up [official OpenRAC 0.9.0](https://github.com/OpenRAC/OpenRAC/releases/tag/launcher-v0.9.0)
   with your own R&C1 NTSC-U game data.
2. In the launcher's Mods page, add this **preview release address**:
   [https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.2.0-preview.1](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.2.0-preview.1)
3. Select **Revolverator**, install it, enable it and start the game normally.
   To try it immediately, enable **Revolverator sbloccato da subito** in its settings.

The native archive is `revolverator-0.2.0-windows-x86_64.zip`; it is also suitable
for local ZIP import in the Mods page. The weapon package contains its assets
and privately loaded utility DLLs. No helper mod cards, external launch script,
custom `--levels` path or replacement client EXE is required.

Use the explicit release address for this preview: the launcher skips
prereleases when given only the repository's root address. The root index lists
Weapon Experience, Revolverator, Skill Points and Cinematic Journeys; utility source folders are not extra
installable cards.

| Component | Current source | Role |
|---|---|---|
| [Revolverator](revolverator/README.md) |0.2.0 preview | Additional weapon, native inventory/API v2, automatic asset staging |
| [Weapon Experience](weapon-xp/README.md) |0.6.3-arsenal | Optional matching API v4 progression companion |
| [Skill Points: RetroAchievements](skill-points-ra/README.md) |0.1.3 | Native menu, offline rules and read-only/event C API v1 |
| [Cinematic Journeys](cinematic-journeys/README.md) |0.1.1 | Cutscene protection and configurable original flight animation |
| [Arsenal compatibility](arsenal-compat/README.md) |0.1.0 | Private bundled native-hook utility |
| [File overlay](arsenal-file-overlay/README.md) |0.1.0 | Private bundled read-only manifest alias utility |

Weapon Experience is optional. If enabled with Revolverator, use the matching
0.6.3-arsenal companion; stable 0.6.1 does not implement its ownership API.
The existing stable 0.6.1 release archives remain available for independent use.
Damage/ammunition bonuses default off. Revolverator's base capacity is 20;
normal purchase starts at Hoven for 15,000 bolts, pickups give 2, ammo costs 25 bolts per round.
`start_unlocked` defaults off and does not refill an already owned weapon.

## Compatibility and source

The Revolverator native package targets the unchanged official Windows x64 NTSC-U
client 0.9.0, commit `77a2f052`, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
Other builds fail clearly. The plugin reads original manifests and stages
owned temporary copies; exact read-only aliases let the engine load the bundled
classes while the original extraction and client remain unchanged.

- [Installation](docs/INSTALLATION.md) and [compatibility](docs/COMPATIBILITY.md).
- [APIs](docs/API.md), [building/source boundaries](docs/BUILDING.md).
- [Native/host evidence and limits](docs/VALIDATION.md).
- [Credits and AI ammunition provenance](CREDITS.md), [licensing](docs/LICENSING.md).

This is a preview. Native proof is scoped to its exact binaries and scenarios;
PAL, other platforms, complete campaigns and all mod combinations are not certified.
Original game artwork is excluded from Git; source builders read the player's
own extraction. The [old engine patch](docs/arsenal-bridge/README.md) is historical.

Implementation code is [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342.
Explicit MIT interoperability files, OpenRAC SDK notices, vendored OpenRAC GPL
JSON reader and MinHook BSD notices retain their respective licenses.

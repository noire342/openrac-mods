# OpenRAC Mods

[![Build and test](https://github.com/noire342/openrac-mods/actions/workflows/build.yml/badge.svg)](https://github.com/noire342/openrac-mods/actions/workflows/build.yml)

My Ratchet & Clank mods for the native OpenRAC port. Revolverator adds a separate
weapon alongside the original Walloper, with native menus, 20 rounds and its own
progress. Weapon Experience adds ten persistent weapon levels.
Skill Points: RetroAchievements brings the achievement set into the native
Skill Points menu. Cinematic Journeys prevents accidental cutscene skipping. Clank Unleashed adds
optional independent Clank control and an electrical distance reserve on PAL.

## Recupero componenti / Robot Salvage (development source)

[Recupero componenti](robot-salvage/README.md) retains real robot fragments as
physical, collectable parts, with a native **Componenti** pause inventory,
PAL save sidecars and a versioned C API for future custom levels. Suck Cannon
integration reuses the original component model as its projectile.

This is **development source**, not an installable or final release. Windows
x64 / official OpenRAC 0.10.0 / PAL Europe v2.00 is the current target. See
[build instructions](robot-salvage/README.md#build) and the
[verified scope and remaining work](robot-salvage/docs/VALIDATION.md), including
the known variable-size fragment limitation. Existing release downloads remain
available; their binaries do not contain this development utility update.

## Install Clank Unleashed / Clank libero (PAL preview)

Use **official OpenRAC 0.10.0**, Windows x64, and your own **PAL Europe v2.00 / SCES_509.16** disc and extracted data. Add this repository in the launcher's Mods page, select **Clank libero**, install it and enable it. If an older NTSC preview is installed, update it first.

[Download preview.5](https://github.com/noire342/openrac-mods/releases/tag/clank-unleashed-v0.1.0-preview.5) or import `clank-unleashed-0.1.0-preview.5-windows-x86_64.zip` locally. **L3 + Triangle** detaches Clank; **Triangle near Ratchet** rejoins him. Native melee, glide/propeller and the Gadgebot wheel are retained. The optional distance reserve uses the original glass HUD and electrical particles, with a native audible alarm. Its default consumption matches Ratchet's original oxygen rate.

This is a preview tested in Aridia and Novalis, not a full-campaign certification. It includes no disc artwork or audio and needs no engine patch. See [controls, settings and building](clank-unleashed/README.md) and [validation scope](clank-unleashed/docs/validation-pal-0.10.0.md).

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

## Install Rotatore / Revolverator 0.3.1

1. Set up [official OpenRAC 0.10.0](https://github.com/OpenRAC/OpenRAC/releases/tag/launcher-v0.10.0)
   with your own **PAL Europe v2.00 / SCES_509.16** data, the reference region.
   NTSC-U has a separate supported binding profile. Use **Check and play** if
   the updated client asks to rebuild the extracted levels in format 4.
2. In **Mods**, add [this repository](https://github.com/noire342/openrac-mods),
   select Revolverator and install it.
3. Enable Revolverator and start normally. Italian UI calls the weapon
   **Rotatore**; **Rotatore sbloccato da subito** enables an immediate trial.

The native package contains the weapon assets and its two private utility DLLs.
It requires no extra utility cards, external launch script, custom `--levels`
path or replacement client. The Gold upgrade adds a Novalis Gold-shop case:
**50,000 ordinary bolts, zero Gold Bolts**. It retains the same ammunition,
independent XP and quick-select identity. Gold-thrown enemies create a
charge-scaled impact explosion.

For local import, choose `revolverator-0.3.1-windows-x86_64.zip` with **Mods → File**.
The [0.3.1 release](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.3.1)
is available through ordinary repository discovery. It carries all four native
packages: Weapon Experience, Revolverator, Skill Points and Cinematic Journeys.
Skill Points retains its declared NTSC-U scope; its presence in the catalog does
not make it PAL-compatible.

| Component | Current source | Role |
|---|---|---|
| [Revolverator](revolverator/README.md) |0.3.1 PAL reference | Additional weapon, Gold upgrade, native inventory/API v3 and automatic asset staging |
| [Weapon Experience](weapon-xp/README.md) |0.6.4 | Optional API v4 progression companion with PAL HUD/camera corrections |
| [Skill Points: RetroAchievements](skill-points-ra/README.md) |0.1.3 | Native menu, offline rules and read-only/event C API v1 |
| [Cinematic Journeys](cinematic-journeys/README.md) |0.1.1 | Cutscene protection and configurable original flight animation |
| [Clank Unleashed](clank-unleashed/README.md) |0.1.0-preview.5 PAL | Independent native Clank control, Gadgebot wheel and optional electrical reserve |
| [Recupero componenti](robot-salvage/README.md) |0.1.0 development source | Physical robot fragments, native collection inventory and C API v1; release validation in progress |
| [Arsenal compatibility](arsenal-compat/README.md) |0.1.0 | Private bundled native-hook utility |
| [File overlay](arsenal-file-overlay/README.md) |0.1.0 | Private bundled read-only manifest alias utility |

Weapon Experience is optional. If enabled with Revolverator, use the matching
0.6.4 companion; stable 0.6.1 does not implement its ownership API.
Update the companion too: the earlier 0.6.3-arsenal package does not include
these PAL HUD/camera fixes.
The existing stable 0.6.1 release archives remain available for independent use.
Damage/ammunition bonuses default off. Revolverator's base capacity is 20;
normal purchase starts at Hoven for 15,000 bolts, pickups give 2, ammo costs 25 bolts per round.
`start_unlocked` defaults off and does not refill an already owned weapon.

## Compatibility and source

The current reference is the unchanged official Windows x64 PAL client 0.10.0,
SHA256 `e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e`.
The corresponding NTSC-U 0.10 client is also recognized through an explicit
region profile. Unknown/mismatched builds fail clearly. The plugin reads source
manifests and mounts owned temporary copies while the extraction and client
remain unchanged. See [PAL scope](revolverator/docs/PAL.md) and
[XP PAL evidence](weapon-xp/docs/PAL.md) for tested scenarios and limitations.

- [Installation](docs/INSTALLATION.md) and [compatibility](docs/COMPATIBILITY.md).
- [APIs](docs/API.md), [building/source boundaries](docs/BUILDING.md).
- [Native/host evidence and limits](docs/VALIDATION.md).
- [Credits and AI ammunition provenance](CREDITS.md), [licensing](docs/LICENSING.md).

This is a preview. Native proof is scoped to its exact binaries and scenarios;
other native platforms, complete campaigns and all mod combinations are not certified.
The original drill geometry is read from the player's extraction. The authored
Gold recolour and ammunition recipe retain their explicit AI/art provenance. The [old engine patch](docs/arsenal-bridge/README.md) is historical.

Implementation code is [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342.
Explicit MIT interoperability files, OpenRAC SDK notices, vendored OpenRAC GPL
JSON reader and MinHook BSD notices retain their respective licenses.

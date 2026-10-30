# Installation

## Revolverator preview through the normal launcher

Use [official OpenRAC 0.9.0](https://github.com/OpenRAC/OpenRAC/releases/tag/launcher-v0.9.0)
with your own NTSC-U game data. In the Mods page add
[the explicit preview release address](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.2.0-preview.1),
select Revolverator, install it and enable it. Launch the game normally.
The launcher verifies the native archive against the release's `SHA256SUMS`.

For local import choose `revolverator-0.2.0-windows-x86_64.zip` in the Mods page.
This is one ordinary native mod package containing assets and utility libraries.
Do not install the utility source folders as separate mod cards. No external
PowerShell launcher, custom level directory, manual manifest edits or client EXE
replacement is part of this installation flow.

The preview URL must include `/releases/tag/revolverator-v0.2.0-preview.1`.
The launcher excludes prereleases from its normal repository-root selection;
the explicit tag selects this preview. The repository index exposes
Weapon Experience, Revolverator, Skill Points and Cinematic Journeys.

## Trying the weapon and preserving progress

Normal progression offers Revolverator at Hoven for 15,000 bolts, capacity 20,
pickups give 2 and ammo costs 25 bolts per round. For an immediate trial enable
**Revolverator sbloccato da subito** (`start_unlocked`) in the mod settings.
The default is off; an already owned weapon is not refilled on reload.

Use the game's normal saves. Custom ownership, ammo, XP and logical favourites
are stored in a fingerprinted `saveN.bin.arsenal` sidecar. The normal save keeps
native IDs and original Walloper state. The mod reads the original asset
extraction and creates only its own temporary manifest cache; it does not
rewrite the client or installed source manifests.

## Optional Weapon Experience

Revolverator runs without XP. If XP is enabled, install the matching
0.6.3-arsenal/API v4 companion; older stable 0.6.1 cannot filter custom ownership
and is refused clearly in this combination. Damage/ammo bonuses remain off
by default. Stable 0.6.1 archives remain unchanged for standalone use.

See [Compatibility](COMPATIBILITY.md) for the exact supported executable and
[Validation](VALIDATION.md) for binary-specific native scope. The earlier outer
portable ZIP and helper scripts are historical development workflows.

## Skill Points: RetroAchievements

Add the repository root in Mods or import
`skill-points-ra-0.1.3-windows-x86_64.zip` from its release. Enable the mod and
use Pause → Goodies → Skill Points (Italian: Pausa → Premi → Punti stile).
Goodies is available directly below Options from the beginning of the campaign.
The Kerwan timed obstacle challenge defaults off; enable it in the mod settings
to include all 178 achievements instead of the default 177. Existing unlocks
remain in the separate local profile. No login is required and nothing is
submitted to RetroAchievements. This release supports NTSC-U only.

## Cinematic Journeys

Add the repository root in Mods or import
`cinematic-journeys-0.1.1-windows-x86_64.zip`. Enable it and launch normally.
Hold L1+L2+R1+R2 and press Start to skip deliberately. Ordinary menu/pause Start
is retained. The flight minimum defaults to five seconds and can be changed
from 0 to 30 in the launcher's mod settings. Development `--skip-movies` bypasses
movie playback before the mod and is outside this protection. PAL is declared
by the adapter, but the preserved in-game tests cover NTSC-U.

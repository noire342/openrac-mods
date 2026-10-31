# Installation

## Rotatore / Revolverator 0.3.1 through the normal launcher

Use official OpenRAC 0.10.0 Windows x64 with your own PAL Europe v2.00
(`SCES_509.16`) data, the reference region. NTSC-U remains a separate supported
profile. Re-extract with **Check and play** if format-4 levels are requested.
In **Mods**, add [the repository](https://github.com/noire342/openrac-mods), select
Revolverator and install it. The 0.3.1 release is a normal discoverable release,
so the repository-root link works. Local import of
`revolverator-0.3.1-windows-x86_64.zip` through **Mods → File** is also available.
The launcher verifies the exact asset against that release's `SHA256SUMS`.

Enable the mod and launch normally. Its two utility DLLs are private components,
not separate mod cards. No replacement EXE, external launch script, custom
`--levels` argument or manual manifest editing is part of this flow. The catalog
still lists all four public mod IDs. Older historical prereleases require their
explicit tag address; that restriction does not apply to 0.3.1.

## Progress and upgrades

Rotatore is additional logical item 65554, alongside native Walloper 18. Normal
purchase starts after visiting Hoven, at 15,000 bolts. Base capacity is 20,
pickups give 2, and ammo costs 25 bolts per round. `start_unlocked` defaults off;
enabling it does not refill an already owned weapon after every load.

The additional Novalis Gold-shop case upgrades it for 50,000 ordinary bolts and
zero Gold Bolts, subject to the native Gold-shop access gate and base ownership.
The upgrade retains the same ammo, XP and favourites. Native saves retain native
IDs; the fingerprinted `.arsenal` codec2 sidecar stores custom state and reads
the prior codec1 format. PAL and NTSC save profiles remain separate; no automatic
campaign migration or overwrite is performed. See [PAL/save details](../revolverator/docs/PAL.md).

## Optional Weapon Experience 0.6.4

The weapon works without XP. When XP is enabled, install the matching 0.6.4 / API v4
companion. It includes the PAL HUD/camera fixes and preserves older ABI prefixes.
The old 0.6.3-arsenal binary is not the PAL-fixed companion; stable 0.6.1 remains
available separately for standalone use. Damage/capacity bonuses default off.
See [XP PAL coverage](../weapon-xp/docs/PAL.md) for exact native scope.

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

# Installation

## Stable Weapon Experience0.6.1

Download the existing Windows archive from [Releases](https://github.com/noire342/openrac-mods/releases),
import it in the OpenRAC launcher's Mods page and restart. Its archives and
stable behavior are unchanged. Damage and ammunition bonuses default off.
Profiles remain under `%LOCALAPPDATA%/OpenRAC/weapon-xp/` as documented for that
release. Stable0.6.1 runs independently; it does not provide this preview's
custom-weapon API v4 ownership filter.

## Additive Revolverator preview

The preview uses your installed [official stock Windows x64 NTSC-U client](COMPATIBILITY.md).
It does not contain or replace the client EXE. Install
[OpenRAC 0.9.0](https://github.com/OpenRAC/OpenRAC/releases/tag/launcher-v0.9.0),
complete setup with your own NTSC-U game data, and download
[the preview.1 ZIP](https://github.com/noire342/openrac-mods/releases/download/revolverator-v0.2.0-preview.1/revolverator-0.2.0-preview.1-windows-ntsc.zip).
Extract the outer archive to a writable folder, then double-click `Avvia-Prova.cmd`.
Use the included script launcher; do not import this outer ZIP in the Mods page.

- `Avvia-Prova.cmd` starts an isolated empty Novalis trial with the Revolverator
  and 20 rounds. Trial card/XP data are separate from persistent progress.
- `Avvia-Revolverator.cmd` uses the persistent `.profile`. Initial setup copies
  existing card/settings/XP data once; later sessions reuse that copy.
- `Start-Revolverator.ps1 -PrepareOnly` validates inputs and prepares a session
  without starting gameplay. `-Runtime` may point to a matching installed EXE;
  its exact SHA256 must match before the compatibility provider can activate.

The launcher loads **arsenal-compat, optional matching XP, Revolverator**.
Arsenal Compatibility 0.1.0 supplies 30 exact native boundaries as a mod;
Revolverator retains 24 ordinary SDK hooks. No engine patch is required.
For source setup build the three components, generate the local companion
assets and use the documented package inputs in [Building](BUILDING.md).

Revolverator is additional; the original Walloper remains independent.
Normal vendor progression starts at Hoven for 15,000 bolts, capacity 20, pickups 2
and 25 bolts per round. `start_unlocked` is off by default. `-StartUnlocked`
optionally grants an unowned weapon in a persistent session; reloading an
already owned weapon does not refill ammunition.

Custom state lives in a fingerprinted `saveN.bin.arsenal` sidecar alongside the
native save in the isolated profile. The official installation and original
user card stay untouched. Current stock-client evidence and older custom-runtime
results are separated in [Validation](VALIDATION.md). This preview is distinct
from a final release; PAL/full-campaign/all-mod compatibility is not certified.

# Single-package stock-client compatibility

The current native archive runs as one ordinary Revolverator mod in the official
unchanged Windows x64 R&C1 NTSC-U client. Utility DLLs and assets are included
privately in that package; they do not create extra launcher cards.

| Input/component | Requirement |
|---|---|
| Official client |0.9.0, commit `77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225` |
| EXE SHA256 | `b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081` |
| Revolverator |0.2.0, generic mod API 1.1; public inventory API v2 |
| Optional XP |0.6.3-arsenal with API v4; loaded before the weapon |
| Bundled utility DLLs | arsenal_compat.dll 0.1.0 and arsenal_file_overlay.dll 0.1.0 |

The Revo manifest lists only 24 ordinary SDK hooks. Thirty extra boundaries stay
under `extensions.noire342.arsenal.compatibilityHooks`; private bundled utilities
are described separately by `bundledUtilities`. The consumer discovers its own
DLL directory and uses `LoadLibraryExW` with restricted search flags. No global
SDK table replacement or client file patch is performed.

At load, `NativeAssetBundle` finds the actual original `--levels` root, or the
native `--data/port/ntsc` fallback. It reads 19 source manifests and stages unique
owned copies with absolute paths to packaged classes 2017/32000/2016. Original
Walloper 180 and native 225 remain present. The overlay redirects only exact
configured read-only `OPEN_EXISTING` manifest opens through KernelBase CreateFileW;
other paths and writes pass through. The map is immutable for worker threads.

The hook provider verifies the EXE hash and executable function targets,
prepares 30 disabled detours, then activates one complete consumer transaction.
The overlay has the same exact-client gate. Failure produces diagnostics and
owned cleanup; unsafe unload failures retain code/trampolines rather than leave
dangling calls. Native saves and normal game progression remain the game's own.

The launcher repository index lists four public mods. An explicit prerelease
tag URL is required to select preview assets, whose filenames and hashes follow
the stock launcher's native import convention. See [Installation](INSTALLATION.md).

PAL, other native platforms and different executable hashes are unsupported by
the utility backend. Linux builds test portable adapters and service exports.
The [former engine patch](arsenal-bridge/README.md) is historical reference;
it is not required for this package.

## Standalone skill points and cinematics

The exact executable gate above applies to Revolverator's bundled utilities.
The two standalone modules use ordinary SDK hooks and do not require those utilities.

| Module | Scope |
|---|---|
| Skill Points: RetroAchievements 0.1.2 | R&C1 NTSC-U (SCUS_971.99), generic mod API 1.1; offline sidecar profile; 178-rule snapshot from 2026-10-28 |
| Cinematic Journeys 0.1.0 | PAL and NTSC-U adapters, generic mod API 1.1; native evidence on Windows NTSC-U only |

Their preserved native receipts use OpenRAC 0.9.0/reference commit `2a457b30`.
They do not establish an exhaustive campaign or every combination with the current
Revolverator preview. The Skill Points adapter must load after other
`rac1.level.load_gameplay` hooks; see its [architecture](../skill-points-ra/docs/ARCHITECTURE.md).

# Stock-client compatibility

The current additive preview uses the official **unchanged** OpenRAC Windows
x64 R&C1 NTSC-U executable. No custom client build is required or shipped.

| Component | Supported input |
|---|---|
| Stock OpenRAC |0.9.0, commit `77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225` |
| Executable SHA256 | `b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081` |
| Arsenal Compatibility |0.1.0, Windows x64, service API v1 |
| Revolverator |0.2.0, generic mod API1.1, inventory API v2 |
| Optional Weapon Experience |0.6.3-arsenal with API v4; stable 0.6.1 is not the matching companion |

Load **arsenal-compat → optional weapon-xp → revolverator**. The Revo manifest
lists only its 24 ordinary SDK hook requirements. Its 30 additional boundaries
are declared separately under `extensions.noire342.arsenal.compatibilityHooks`
and supplied by the compatibility provider's versioned API.

The [provider](../arsenal-compat/README.md) hashes the running executable,
resolves all 30 names through `api->function` and checks executable main-module
memory with `VirtualQuery`. It uses pinned MinHook 1.3.4 to prepare disabled
in-memory detours. Claim/signature/duplicate checks precede one queued commit.
An unsupported hash, missing symbol, incomplete transaction or backend failure
prevents activation and produces diagnostics. There is no permissive bypass.

The executable on disk remains unchanged; in-memory detours belong to the
provider and are removed on consumer teardown. The SDK table is not replaced.
The official client and the player's extracted data remain local inputs.
The preview ZIP contains mod DLLs, corresponding plugin sources, launcher
scripts and companion assets, **no client executable, ISO or player save**.

Save_Restore and NewGame hook the whole stock host wrappers, preserving their
native trampolines. Native tests must exercise these wrapper semantics; older
checks on a modified runtime do not certify the new boundary. Current and
historical evidence are separated in [Validation](VALIDATION.md).

PAL and other native platforms/builds are outside this provider's support.
Linux source builds test portable contracts and service exports only.
The [old engine patch](arsenal-bridge/README.md) is retained for historical
comparison; applying or compiling it is not part of current preview setup.

# OpenRAC 0.10.0 compatibility

Reference: official release `launcher-v0.10.0`, commit
`d015ea1c6925de90b4be91ddb349af83361d9d4b`.
Windows NTSC-U executable SHA-256:
`f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`.
The test executable comes from the official MSI, extracted without installing over
the player's client. No engine patch is required.

The standalone build pins SDK **1.10.1**, SHA-256
`5d7455a9c0e7bb6067ee514565cbce9abf12e132f9eb5053d8d3618c16084470`.
The DLL still requests **API 1.1** and uses only that prefix; the official client
reports **API 1.10**. Loading checks structure size and all services the mod uses.
The repository CI also checks the SDK 1.3 baseline. SDK compatibility and host tests
alone do not establish native gameplay.

## Reproduce

From this module's directory:

```sh
python tools/fetch_sdk.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python tools/package_delivery.py --build build --output dist
```

The packager reads the built manifest and version. It takes no personal QA folder,
profile or game file as an input. `--validation receipt.json` optionally includes
an existing factual receipt; packaging never invents native results. Use a fresh
output folder for each run. The source ZIP can build independently of the monorepo.

In the full repository, `tools/run_standalone_native.ps1` takes an explicit official
EXE, the player's extracted data, a build with the QA fixtures enabled and a new
output directory. It uses isolated cards, settings and LOCALAPPDATA. The fixtures
must never be distributed in the installable mod. Its screenshots and logs are
evidence of the requested scenario, not an exhaustive campaign test.

## Skill Points scope

R&C1 NTSC-U / SCUS_971.99 only. The 178-rule offline snapshot is unchanged; the
Kerwan time trial is disabled by default, leaving 177 active achievements. The
versioned profile and public C ABI v1 are compatible with 0.1.2. Updates preserve
existing local unlocks. Other mod listeners still use the same public header.

The original Pause → Goodies → Skill Points screen is extended in place. No new
page or HUD panel is registered. Callback maps match all 19 overlays at the pinned
release. Native controller navigation, notifications and profile persistence are
checked in isolated sessions. Completing all 178 rules manually, online co-op,
PAL and every third-party mod combination remain outside this verification.

Load after other `rac1.level.load_gameplay` interceptors. The relay owns only the
patched native menu objects and restores only fields it still owns. The fallback
continues to call the real loader exactly once.

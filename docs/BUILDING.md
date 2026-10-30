# Build preview source

Requirements: CMake 3.20+, Python 3, a C++20 compiler and pinned OpenRAC mod SDK 1.3.0.
The SDK fetcher verifies the URL/SHA256 recorded in `sdk.lock`.

```sh
python tools/fetch_sdk.py
cmake -S . -B build -DBUILD_WEAPON_XP=ON -DBUILD_REVOLVERATOR=ON -DBUILD_ARSENAL_COMPAT=ON -DBUILD_ARSENAL_FILE_OVERLAY=ON -DBUILD_SKILL_POINTS_RA=ON -DBUILD_CINEMATIC_JOURNEYS=ON -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`BUILD_WEAPON_XP` builds the 0.6.3-arsenal companion; `BUILD_REVOLVERATOR` builds
additive 0.2. `BUILD_ARSENAL_COMPAT` defaults to the Revolverator option and builds
the provider on Windows x64, or only its portable core/tests on Linux.
The combined suite contains **41 groups: 19 Revolverator, 13 XP, 1 hook provider,
1 file overlay, 5 skill points and 2 cinematics**.
Disable individual options to build components separately.

`BUILD_SKILL_POINTS_RA` and `BUILD_CINEMATIC_JOURNEYS` default on and are
independent modules. Their folders also support standalone CMake builds using
the pinned SDK. The vendored MIT rcheevos runtime and sanitized achievement
definitions allow offline builds without account credentials or game files.

Select another unpacked SDK with `-DOPENRAC_SDK=/path/to/sdk` or `OpenRACMod_DIR`.
Windows uses Visual Studio 2022 or later. Linux exports the optional Revo
service explicitly and links `${CMAKE_DL_LIBS}`. CI builds/tests both systems;
its success does not create a stable release or prove native Linux gameplay.

## Native package inputs

The native Windows archive is an ordinary single-mod package. Its own assets
and both helper DLLs live under the Revolverator root; `NativeAssetBundle`
stages read-only copies of 19 original manifests at load and utility modules
load from the weapon DLL directory. Source builds need no game assets/client.
The native consumer still requires the [exact official client](COMPATIBILITY.md).

CMake builds utility components separately for development/testing. The release
packager includes their DLLs privately beside `revolverator.dll`; they are not
independent launcher cards. `BUILD_ARSENAL_FILE_OVERLAY` defaults to the Revo
option, and non-Windows builds test only that utility's portable core.

The authored cartridge HUD/recipe are included in Git. Original drill art,
meshes/textures and optional HD glyph header are excluded. Asset builders use
the player's own extraction. Corresponding-source archives for a distributed
binary must include its exact generated inputs separately where required,
plus the GPL JSON reader under `third_party/openrac`, utility code, pinned
MinHook source and retained notices. The ordinary Git source snapshot excludes
game-derived generated headers.

The normal importer expects `revolverator-0.2.0-windows-x86_64.zip` and a release
`SHA256SUMS` entry. Repository discovery is limited by `openrac-mods.json` to
the four public mods. Explicit release-tag URLs select prereleases;
source builds and CI do not publish release assets.

## Artifact evidence

Preview bundles are delivered through Releases after their own hash/native
checks. A source build does not establish gameplay behavior. The stable XP 0.6.1
archives and historical receipts remain unchanged; previous engine-bridge
results are not silently transferred to the stock-client combination.

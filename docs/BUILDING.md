# Build preview source

Requirements: CMake 3.20+, Python 3, a C++17 compiler and pinned OpenRAC mod SDK 1.3.0.
The SDK fetcher verifies the URL/SHA256 recorded in `sdk.lock`.

```sh
python tools/fetch_sdk.py
cmake -S . -B build -DBUILD_WEAPON_XP=ON -DBUILD_REVOLVERATOR=ON -DBUILD_ARSENAL_COMPAT=ON -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`BUILD_WEAPON_XP` builds the 0.6.3-arsenal companion; `BUILD_REVOLVERATOR` builds
additive 0.2. `BUILD_ARSENAL_COMPAT` defaults to the Revolverator option and builds
the provider on Windows x64, or only its portable core/tests on Linux.
The combined suite contains **32 groups: 18 Revolverator, 13 XP, 1 provider**.
Disable individual options to build components separately.

Select another unpacked SDK with `-DOPENRAC_SDK=/path/to/sdk` or `OpenRACMod_DIR`.
Windows uses Visual Studio 2022 or later. Linux exports the optional Revo
service explicitly and links `${CMAKE_DL_LIBS}`. CI builds/tests both systems;
its success does not create a stable release or prove native Linux gameplay.

## Native gameplay inputs

The Windows preview uses the [unchanged official stock client](COMPATIBILITY.md)
with **Arsenal Compatibility 0.1.0 loaded first**. No executable patch or custom
engine build is required. The supported SHA256 is
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
The provider vendors exact MinHook 1.3.4 source and retains BSD notices.
Host builds need no client executable or game assets.

The [asset workflow](../revolverator/docs/BUILD_AND_ASSETS.md) generates the
original drill locally from the player's extraction and extends manifests
with 2017/32000/2016 while preserving native 180. Original game art and optional
`revolverator_hd_icon_data.hpp` remain excluded from Git/CI. The authored
`ammunition_icon_data.hpp` is included for the cartridge HUD.

For a portable preview use `--compat-plugin` and `--compat-source` with the
matching built provider/source. `--runtime` can verify an installed stock
client read-only; it is never archived. XP is optional: `--weapon-xp` and
`--weapon-xp-source` must identify the matching API v4 companion. An optional
`--bridge-patch` is historical reference only. See [packaging](../revolverator/docs/PACKAGING.md).

## Artifact evidence

Preview bundles are delivered through Releases after their own hash/native
checks. A source build does not establish gameplay behavior. The stable XP 0.6.1
archives and historical receipts remain unchanged; previous engine-bridge
results are not silently transferred to the stock-client combination.

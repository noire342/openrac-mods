# Build development source

Requirements: CMake 3.20+, Python 3, a C++17 compiler and pinned OpenRAC mod SDK 1.3.0.
The SDK is downloaded separately; its fetcher verifies the URL/SHA256 in `sdk.lock`.

```sh
python tools/fetch_sdk.py
cmake -S . -B build -DBUILD_WEAPON_XP=ON -DBUILD_REVOLVERATOR=ON -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`BUILD_WEAPON_XP` builds the 0.6.3-arsenal development companion. `BUILD_REVOLVERATOR`
builds the additive 0.2 source and defaults off unless selected. The test suite
contains 17 Revolverator and 13 Weapon Experience host groups. Build either mod
separately by disabling the other option.

An unpacked SDK can be selected with `-DOPENRAC_SDK=/path/to/openrac-mod-sdk-1.3.0`
or an explicit `OpenRACMod_DIR`. On Windows use Visual Studio 2022 or later.
On Linux the optional service symbol is exported explicitly and the consumer
links `${CMAKE_DL_LIBS}`. The workflow builds/tests both modules on Windows and
Ubuntu and uploads test logs; it does not create a stable release.

## Native gameplay requirements

Host builds need no game assets or modified executable. Running Revolverator 0.2
requires R&C1 NTSC-U data and the matching [arsenal bridge](arsenal-bridge/README.md),
which adds 30 typed native hook boundaries on the pinned OpenRAC base. Build that
runtime separately from your own disc and keep the official installation intact.
Weapon Experience by itself does not require this new bridge.

The [local asset workflow](../revolverator/docs/BUILD_AND_ASSETS.md) constructs
the original drill and authored ammunition locally, then extends native level
manifests with 2017/32000/2016 while preserving 180. Original game art and the
optional `revolverator_hd_icon_data.hpp` are not published. The authored
`ammunition_icon_data.hpp` is included so public builds retain the cartridge HUD.

When packaging the optional companion, point `--weapon-xp-source` to this
repository's `weapon-xp` directory and provide its matching built package.
`--engine-source` points to the separately patched OpenRAC checkout. The private
bundle helper's `--runtime` and `--bridge-patch` refer to those exact local inputs.

## Preview artifacts

This source builds the additive preview. Public preview bundles are delivered
separately through Releases after artifact and native-session checks; a host
build alone does not establish gameplay behavior. The published Weapon
Experience 0.6.1 stable archives and historical validation receipts remain
unchanged. The preview is separate from the final stable release.

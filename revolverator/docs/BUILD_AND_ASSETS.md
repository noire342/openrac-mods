# Building Revolverator

The source uses CMake 3.20 or newer, a C++20 compiler and the OpenRAC mod SDK.
The Windows package requires mod interface **1.12 or newer**. Portable
packages require **1.20 or newer** for the official Gadgetron icon-strip hook.
Newer clients are supported when they continue to expose the required capabilities
through the compatible mod interface. The build helper pins SDK archives for
reproducible builds, checks their SHA-256 and extracts the MIT SDK.

## Windows

~~~powershell
python revolverator/tools/fetch_build_sdk.py
cmake -S revolverator -B build-revolverator -DOpenRACMod_DIR="$PWD/.sdk/rotatore-build-sdk/sdk/cmake" -DBUILD_TESTING=ON
cmake --build build-revolverator --config Release
ctest --test-dir build-revolverator -C Release --output-on-failure
~~~

The native package is staged under build-revolverator/package/revolverator.
On Windows the weapon build also builds and stages its private file-overlay
adapter from support/file-overlay. No separately installed provider card or
prebuilt helper download is required to build this source tree.
Run the Python runtime-asset tests with the official client's bundled Python,
or a normal development Python installation:

~~~powershell
python revolverator/tests/runtime_asset_preparation_test.py
~~~

## Linux and macOS

Use a native x86-64 Linux or Apple silicon macOS build host.

~~~bash
python3 revolverator/tools/fetch_build_sdk.py --interface 1.20
cmake -S revolverator -B build-portable -DCMAKE_BUILD_TYPE=Release \
  -DOpenRACMod_DIR="$PWD/.sdk/rotatore-portable-sdk/sdk/cmake" -DBUILD_TESTING=ON
cmake --build build-portable --parallel
ctest --test-dir build-portable --output-on-failure
~~~

## Android

Install Android NDK r28c and set `ANDROID_HOME` to the Android SDK directory.
The official Android game loads an Arm64 NDK shared library.

~~~bash
python3 revolverator/tools/fetch_build_sdk.py --interface 1.20
cmake -S revolverator -B build-android -DCMAKE_BUILD_TYPE=Release \
  -DOpenRACMod_DIR="$PWD/.sdk/rotatore-portable-sdk/sdk/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DANDROID_PLATFORM=android-28 -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_static -DBUILD_TESTING=OFF
cmake --build build-android --parallel
~~~

Build and test artifacts identify their target platform. A successful build
checks the compiler and packaging contract. Runtime validation separately
checks load status, controls, menus, graphics and saves on the target client.

## Parked sound experiment

The source tree retains the frozen procedural WAVs under assets/audio and their
generators under assets/audio/source. The production target does not link the
custom sound adapter, copy these WAVs, or connect a custom sound sink. It keeps
the original native sound behavior. A normal build or game start needs no
NumPy, FFmpeg or sound-lab workspace.

For a new original handoff, maintainers use tools/stage_audio_bundle.py with its
frozen handoff JSON. It verifies the pinned source catalog and every waveform
before creating the English relative catalog, sources/license closure and
provenance inventory. It refuses to overwrite an existing audio directory.
The source packager preserves that audited experiment. The installable packager
excludes it entirely; see [audio](AUDIO.md).

## Public package

Include the native weapon module, original authored ammunition art,
preparation scripts and license notices. The Windows archive also includes
its file-overlay adapter. Portable clients use the OpenRAC loader's class
asset mount. Required scripts are:

- tools/prepare_runtime_assets.py
- tools/build_model_pack.py
- tools/build_golden_assets.py
- tools/build_tip_glow.py
- tools/build_vent_glow.py

Create a portable archive with `tools/package_portable_runtime.py`. Pass the
chosen build's `package/revolverator` directory to `--module`. The packager
checks its manifest, shared-library platform and authored asset hashes.

Do not include copied assets/graphics, copied or recoloured game atlases,
embedded game-derived HD icon headers, extracted levels, ISOs, RAM dumps,
cards or sidecars. The original ammunition model/texture can be generated
from the project's recipe during packaging. Development-only ammunition
builders may require Pillow; players do not need it.

Do not set REVOLVERATOR_HD_ICON_DIR for a public build. Without that private
input, the runtime derives its blue, Golden and AMMO variants from the player's
resident native icon. The cartridge HUD icon is original authored artwork.

## First-start preparation

Players do not run these development commands. The official client contains
Python, which the mod locates from the running client's installation. The
helper uses only the standard library, reads the player's extracted Hoven
drill and held-weapon skeletons, and generates the 19-level model set in a
cache outside both game data and mod files. It also fits the vent and drill-tip
glow effects to that disc. The SDK reads those props from the installed mod's
generated-glow directory, where the helper stages the verified copies.

The Golden model retains the base geometry and UVs. A local colour transform
produces its warm-metal atlas from native shading and alpha. The static
class-2019 shop display removes its skin binding and does not borrow the pose
of the weapon Ratchet is currently holding.

See [runtime assets](RUNTIME_ASSETS.md) for cache checks and diagnostics,
[packaging](PACKAGING.md) for distribution, and
[compatibility](COMPATIBILITY.md) for minimum-interface behavior.

## Historical tools

Older private-bundle scripts and companion-pack commands remain in source
for reproducibility. Their exact-client and manual-launch instructions are
historical, not the public installation path. Corresponding-source archives
retain authored code and notices while excluding private game-derived outputs.

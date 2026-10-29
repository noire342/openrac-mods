# Build from source

Requirements: CMake 3.20+, Python 3, a C++17 compiler and the pinned OpenRAC mod
SDK 1.3.0. The SDK is downloaded as source/header tooling; it is not vendored here.
The mods link only to the SDK interface, not the OpenRAC executable.

```sh
python tools/fetch_sdk.py
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python .sdk/openrac-mod-sdk-1.3.0/tools/package_mod.py build/package/weapon_xp -o dist
```

The exact package directory is printed by the build; the SDK may normalize the
target name to `weapon-xp`. Use the directory that contains the package manifest.
On Windows, Visual Studio 2022 or later supplies the compiler. A generator can be
specified explicitly when several toolchains are installed.

For an SDK already unpacked elsewhere:

```sh
cmake -S . -B build -DOPENRAC_SDK=/path/to/openrac-mod-sdk-1.3.0
```

`sdk.lock` pins the SDK URL and SHA-256. The fetcher rejects a different archive.
An explicit `OpenRACMod_DIR` takes precedence over `OPENRAC_SDK`.

## Targets and tests

`BUILD_WEAPON_XP` defaults on. It builds the stable source snapshot and twelve
test groups, including the native adapter/DLL-host harness. These tests are
separate from native game sessions described in [Validation](VALIDATION.md).

`BUILD_REVOLVERATOR` defaults off until a source snapshot is explicitly released.
Enabling it on a revision without that folder produces a clear configure error.

## Releasing

Each mod has its own tag, for example `weapon-xp-v0.6.1`. Release archives contain
the manifest at the ZIP root and libraries under `lib/<platform>/`. Include a
`SHA256SUMS` asset so OpenRAC can verify the downloaded archives.

The CI workflow builds/tests source changes and retains artifacts; it does not
automatically promote an unverified development build to a public stable release.
Publishing a tested artifact is a deliberate step with version and validation
receipts checked together.

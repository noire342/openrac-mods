# Building Revolverator

The source uses CMake 3.20 or newer, a C++20 compiler and the OpenRAC mod SDK.
The public manifest requires mod interface 1.12 or newer. The module's build
helper pins the exact upstream source archive supplying that SDK and verifies
its SHA256 before extracting only the MIT SDK subtree. This development step
is separate from the player's automatic local-asset preparation.

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

Host tests exercise controller, attachment, rotor, contact effects, ammunition,
inventory, native menu, save sidecars, regional bindings and optional XP. They
do not replace native gameplay or rendered-image checks. Linux source tests
do not establish native gameplay support for a Windows-only release.

## Public package

Include the weapon DLL, its required asset-mount adapter, original authored
ammunition art, preparation scripts and license notices. Required scripts are:

- tools/prepare_runtime_assets.py
- tools/build_model_pack.py
- tools/build_golden_assets.py

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
cache outside both game data and mod files.

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

# Licensing scope

Mod implementations, authored assets/recipes, tests and project-specific build
scripts use [GPL-3.0-or-later](../LICENSE), Copyright (c) 2026 noire342, following
the upstream `OpenRAC/rac1-mods` convention. The earlier MIT policy is superseded
by this normal source commit; Git history and previous artifacts are not rewritten.

| Material | Retained notice |
|---|---|
| `weapon_xp_api.h`, `arsenal_hooks_api.h` and intentionally MIT C interop examples | MIT, Copyright (c) 2026 noire342; explicit file notices remain applicable |
| Downloaded OpenRAC mod SDK | MIT, Copyright (c) 2026 the OpenRAC contributors; not vendored in this source update |
| `tools/fetch_sdk.py` | GPL-3.0-or-later, Copyright (c) 2026 the OpenRAC contributors; preserved unchanged |
| [Historical arsenal engine bridge patch](arsenal-bridge/README.md) | Upstream GPL and file-specific notices; its exact base and GPL text are included |
| Vendored MinHook 1.3.4 and HDE | Original BSD two-clause notices retained under `arsenal-compat/third_party/minhook/LICENSE.txt` and copied into the binary package |
| Original game art/code/assets | Not relicensed by this project; the player supplies them and original artwork is not published here |

The SDK's MIT license is separate from the mod repository's GPL convention.
It does not change the OpenRAC runtime's license. The complete GPL text is in
`LICENSE` and [`LICENSES/GPL-3.0-or-later.txt`](../LICENSES/GPL-3.0-or-later.txt).
The procedural ammunition recipe and authored HUD tile are project material;
AI assistance is disclosed in [CREDITS](../CREDITS.md).

The complete retained MIT text for the interoperability files is in
[`LICENSES/MIT.txt`](../LICENSES/MIT.txt) and each module's `docs/LICENSE-MIT.md`.

# Credits and provenance

Maintained by **Noire** ([noire342](https://github.com/noire342)).

- **OpenRAC contributors:** the native game port, mod SDK, documented interfaces
  and the SDK fetch/packaging tooling used by this project. Upstream:
  [OpenRAC](https://github.com/OpenRAC/OpenRAC) and
  [rac1-mods](https://github.com/OpenRAC/rac1-mods).
- **Insomniac Games / Sony:** Ratchet & Clank, its characters, original art,
  animations, sound and the original drill model used as a restoration reference.
  Those assets remain the property of their respective owners.
- **Tsuda Kageyu and HDE contributors:** MinHook 1.3.4, pinned unmodified source
  with its original BSD notices; used by the exact-build stock-client provider.
- **Revolverator ammunition model:** generated with **AI assistance** from an
  original procedural mesh and texture recipe. It is new mod art; it is not an
  extracted game asset or an original Insomniac design.

The Revolverator is a community reconstruction. Its proposed behavior and reused
native animations should not be presented as recovered, complete original game
code or a verified exact recreation of the removed weapon.

This repository does not contain a game ISO, extracted original models/textures,
memory dumps, saved campaigns, credentials or the OpenRAC executable. Where a
restoration needs original art, the asset builder reads the player's own local
OpenRAC extraction and generates a private local companion.

The optional local HUD-upscaling workflow uses
[Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan).
Its runtime/model files and game-derived output are not vendored here.

Source files retain their own SPDX/copyright notices. Mod implementation code
is GPL-3.0-or-later, Copyright (c) 2026 noire342. The Weapon Experience and Arsenal Compatibility interop
headers and intentionally MIT client examples retain their MIT notices. Downloaded
OpenRAC SDK files remain MIT. The unchanged upstream `tools/fetch_sdk.py` remains
GPL-3.0-or-later, Copyright (c) 2026 the OpenRAC contributors. Engine bridge changes
keep upstream GPL/file-specific notices, and original game assets keep their own
rights. See [licensing scope](docs/LICENSING.md).

The vendored OpenRAC JSON reader retains its upstream GPL notice and contributor
copyright; the project staging adapter/serializer are separate authored code.

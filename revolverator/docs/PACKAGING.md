# Private source and build packaging

Revolverator is published as a source preview. Game-derived assets and native
binaries remain in the private build workflow. Running the source packaging
tool creates a **private snapshot** and never pushes a commit,
creates a GitHub release, or uploads an artifact.

```powershell
python tools/package_source.py --output <private-output>/revolverator-source.zip
```

The archive contains an allowlisted source tree and a SHA-256 inventory. It
excludes SDK/toolchains, compiled binaries, game artwork, game-derived icon
headers, RAM captures, campaign saves and local fixtures. The tool rejects
symbolic external source paths and refuses to overwrite an existing archive.
If another process changes selected source files while packaging, it removes
the incomplete snapshot and reports the conflict.

Build native code against the separately obtained OpenRAC SDK. The private
graphics companion builders read assets extracted from the player's own game.
The ammo recipe is original project data with AI-assisted model provenance.
Do not include the extracted Hoven drill, its textures, generated copies,
derived HUD image/header, the game ISO, or memory/save data in a source release.
The generated HD header is enabled only through a local
`REVOLVERATOR_HD_ICON_DIR` build path. A binary built with that header embeds
game-derived artwork and belongs to the private build workflow.

## Additional service export on Linux

The OpenRAC SDK's `openrac_add_mod` generates an ELF version script exposing
only the three loader entry points. `OPENRAC_MOD_EXPORT` alone does not override
its `local: *` rule. After `openrac_add_mod(revolverator ...)`, the target's
existing generated export map must additionally name its service:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/revolverator.exports"
    "{\n global: openrac_mod_query; openrac_mod_load; openrac_mod_unload; openrac_revolverator_query;\n local: *;\n};\n")
endif()
```

The SDK already passes this exact file to the linker and adds it to
`LINK_DEPENDS`; do not append a second anonymous version script. Verify the
resulting library with `nm -D --defined-only revolverator.so`, then resolve
`openrac_revolverator_query` from its loaded handle. A query returning null
before `openrac_mod_load` is expected; a missing dynamic symbol is an export
failure.

Native host tests, ELF symbol checks and source archive inspection are distinct
from in-game validation. A public release additionally needs the exact final
binary/graphics workflow and its native gameplay evidence.

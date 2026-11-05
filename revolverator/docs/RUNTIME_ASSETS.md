# Automatic assets from the player's disc

The installable package contains the native mod, its required file-overlay
utility, original authored ammunition art and the preparation scripts. It
contains no copied drill model, native game texture, native HUD icon or
recoloured game atlas. The corresponding source uses the same boundary.

On its first start, Rotatore reads the levels already extracted by the official
OpenRAC launcher. It uses the Python bundled with that same client to run
`tools/prepare_runtime_assets.py`; no separate Python or Pillow installation
is needed. The helper uses only the Python standard library and performs no
network requests.

The original Hoven drill is retargeted onto the native held-weapon skeleton,
retaining its independent head component. Base, Golden and the static Golden
shop display are generated in a local cache. The Golden texture applies an
authored warm-metal ramp to the player's original shading and alpha. The
game's resident HUD glyph supplies the normal, Golden and AMMO icon variants.

The cache is under `%LOCALAPPDATA%/OpenRAC/revolverator/assets/derived/`.
It is separate from the installation, mod files and game saves. Its key binds
the disc region, original input hashes and preparation script versions. Each
of the 77 generated files is checked before a cache is reused; a missing or
damaged cache entry is rebuilt. A warm cache does not rewrite its assets.

Original files are read only. The native overlay mounts private staged
manifests and restores its own mount on shutdown. Settings and the weapon's
save/progression data are independent of this asset cache.

If the launcher has not extracted the supported disc's levels, use its normal
**Check and play** action. A failed preparation leaves a diagnostic log beside
the cache and the mod reports why it could not load. The player does not need
to run any build command or copy a graphics companion pack.

The public Windows release declares a minimum OpenRAC interface and required capabilities in
its manifest and validation receipt. Its native PAL validation is separate
from source-level NTSC adapter coverage.

# Preview source and portable packaging

Version 0.2 is an additive preview, separate from the final stable release.
The packaging scripts prepare reviewable artifacts; public downloads are
published separately through Releases. Native evidence and remaining limits
are listed in [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).

## Public source boundary

The source package contains the allowlisted code, documentation, original ammo
recipe and its SHA-256 inventory. Build the native library against a separately
obtained OpenRAC SDK. Reconstruct the original drill and native icon locally
from the player's own NTSC-U assets.

Exclude the ISO, extracted game models/textures, generated copies of the Hoven
drill, game-derived HD glyph/header, RAM captures, memory cards, sidecars,
private fixtures and installed-data links. A local binary compiled with the
HD glyph header embeds game-derived artwork and belongs to the private workflow.

The cartridge model/texture were created with AI assistance; the procedural
builder generates the project's original single-cell design from its recipe.
The selected native HUD glyph is upscaled with
[Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan).
Keep this provenance distinct from Insomniac Games/Sony's original game assets.

`tools/package_source.py` creates a private source snapshot with an allowlist
and hashes; it does not publish it. Inspect that allowlist after adding 0.2 files
so registry, store, menu, XP adapter, headers and documentation are included.
Reject external symbolic source paths and do not include local generated data.

## Runtime and companion

The additive inventory requires the dedicated runtime bridge based on
`77a2f052`, with 30 typed hooks. The existing generic mod ABI remains 1.1; the
extra hook IDs still require that runtime. The corresponding bridge patch and base are documented in
[`docs/arsenal-bridge`](../../docs/arsenal-bridge/README.md). Do not claim an ordinary stock
OpenRAC executable has these hooks.

Weapon XP is optional. The matching preview companion is
`0.6.3-arsenal` provider with API v4 ownership filters and an optional acquisition
presentation tail. Without XP the weapon works normally; an installed provider
without the ownership filter is incompatible.

The portable preview bundle is prepared separately after integration QA. It
uses isolated paths and leaves the official installation intact. It is not a
new standard launcher import format. Its manifest pins runtime, plugin, graphics
and companion hashes. `Avvia-Prova.cmd` starts an empty isolated Novalis trial
with the weapon and 20 rounds. The normal launcher uses persistent `.profile`
state; `start_unlocked` stays off unless explicitly enabled.

## Linux service export

`openrac_add_mod` normally exports the three loader entry points. The mod's
existing ELF version map must also name `openrac_revolverator_query`; visibility
attributes alone do not override `local: *`. The CMake target already extends
that map and links `${CMAKE_DL_LIBS}` for optional provider discovery.

Verify `nm -D --defined-only revolverator.so` and resolve the service from an
already loaded handle. A null query before mod load is expected; an absent
dynamic symbol is an export failure. Export checks and host tests do not prove
native gameplay or an approved graphical result.

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

## Stock client, compatibility provider and companion

The current preview uses the unchanged official Windows x64 NTSC-U client,
commit `77a2f052`, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
The packager excludes every EXE; it does not bundle a replacement client.
Arsenal Compatibility 0.1.0 is loaded before optional XP and Revolverator.
Its versioned mod API supplies 30 exact-build boundaries; the consumer keeps
its 24 ordinary SDK hooks. The SDK table and executable on disk stay unchanged.
The previous engine patch, if included, is historical source reference only.

`--compat-plugin` supplies the built provider and `--compat-source` its complete
corresponding source, including pinned MinHook 1.3.4 with BSD notices. An optional
`--runtime` validates the installed stock EXE read-only without archiving it.
`--weapon-xp` and `--weapon-xp-source` must identify the matching optional
0.6.3-arsenal/API v4 companion. A previous XP binary without hit ownership is
incompatible; running without XP is supported.

`Avvia-Prova.cmd` starts a separate empty Novalis trial with 20 rounds. The
normal launcher reuses `.profile`; `start_unlocked` is off unless enabled.
The manifest records exact plugin hashes, the expected installed client hash
and `runtime_included=false`. Native verification requires a matching receipt;
PrepareOnly success is not gameplay proof. Earlier modified-runtime results
remain historical and do not automatically certify this stock-client boundary.

## Linux service export

`openrac_add_mod` normally exports the three loader entry points. The mod's
existing ELF version map must also name `openrac_revolverator_query`; visibility
attributes alone do not override `local: *`. The CMake target already extends
that map and links `${CMAKE_DL_LIBS}` for optional provider discovery.

Verify `nm -D --defined-only revolverator.so` and resolve the service from an
already loaded handle. A null query before mod load is expected; an absent
dynamic symbol is an export failure. Export checks and host tests do not prove
native gameplay or an approved graphical result.

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

## Single native launcher package

The current release archive is `revolverator-0.2.0-windows-x86_64.zip`, installable
through the normal launcher's preview release URL or local native ZIP import.
The release must include SHA256SUMS. Root discovery lists only Weapon Experience
and Revolverator; helper utilities are bundled privately, not separate cards.

`NativeAssetBundle` finds the original levels root at load, stages 19 read-only
manifest copies and binds absolute paths to the packaged class assets. Utility
DLLs load from the weapon's DLL directory. No client EXE, manual `--levels`,
external PowerShell launcher or extra utility install is part of this flow.

The source bundle must include the GPL OpenRAC JSON reader/NOTICE, serializers,
asset staging code, utility sources, exact MinHook version/notices and all inputs
needed to reproduce the distributed binary. Game-derived generated art is kept
out of the ordinary Git source snapshot and supplied separately with the exact
binary where required. No ISO, private save or RAM dump belongs in either archive.

The former `package_private_bundle.py`, Avvia-Prova scripts and outer ZIP are
historical development tools. Their old validation does not certify the current
automatically installed single package. Use a matching native receipt for each
new set of plugin/utility hashes.

## Linux service export

`openrac_add_mod` normally exports the three loader entry points. The mod's
existing ELF version map must also name `openrac_revolverator_query`; visibility
attributes alone do not override `local: *`. The CMake target already extends
that map and links `${CMAKE_DL_LIBS}` for optional provider discovery.

Verify `nm -D --defined-only revolverator.so` and resolve the service from an
already loaded handle. A null query before mod load is expected; an absent
dynamic symbol is an export failure. Export checks and host tests do not prove
native gameplay or an approved graphical result.

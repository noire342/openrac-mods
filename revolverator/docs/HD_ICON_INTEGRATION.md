# Native glyph and optional HD icon

The selected original drill glyph is bank 59901, frame 13, for inventory and
quick select. The gameplay ammunition HUD uses the cartridge icon. `build_hd_icon.py` extracts
the actual 32×32 PSMT8 image and CSM1 palette from local game data, then uses
[Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan)
for its 128×128 upscale. `encode_hd_icon.py` writes the native indexed image and
palette header. Original input, generated artwork and embedded headers remain
private game-derived build inputs.

Configure `REVOLVERATOR_HD_ICON_DIR` only for a local build with the generated
`revolverator_hd_icon_data.hpp`. A build without that data retains the native
fallback. The upscale changes drill-glyph detail in inventory/quick select. It must not
replace the gameplay cartridge icon or enlarge the native logical footprint.

## Texture adapter contract

`HdIcon` owns cloned image/palette pages and the presentation's appended frame
references. The existing bank and other weapons' references remain under their
owners. In the ammo HUD, the base callback temporarily samples a transparent
32×32 page; the HD draw then samples the full 128×128 texture into the native
32×32 area. Panel layout, text, position, alpha and slide timing remain native.

This describes the texture adapter; the final 0.2 HUD binding must select the
cartridge rather than the drill. Menu cells use explicit screen dimensions. The 4×5 weapon view and eight-slot
ring must show the correct logical weapon's glyph, including when Walloper and
Revolverator are both owned. An alias class or a successful allocation alone
does not prove the renderer selected the intended texture.

Allocate optional pages from the reserved level arena after essential arm/rotor
resources. Failed allocation or conflicting ownership must leave a usable native
fallback. Reset icon references before their frame tables and level arena are
released; never write through pointers belonging to an old level.

## Current evidence

Host tests cover sizing, page metadata, base-callback restoration, cache
invalidation, allocation reuse, fallback and teardown. The initial 0.2 XP
fixture showed a crossed native glyph; that observation is historical. Later
Novalis menu/HUD fixtures verified the corrected additive binding. Rilgar 5,
Nebula 6 and Quartu 15 then completed 2,400-frame standalone runs with exact
cartridge texels/CLUT and displayed ammunition 19/20. Rilgar/Nebula used the
compact native 32px drill glyph; Quartu used the 128px glyph. Remaining HUD heap
was 3,456/624/12,304 bytes. These low-memory runs did not open menus or load XP.

The exact binary hashes and scope are in [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).
The optional game-derived HD header remains excluded from public source. These
fixtures do not establish all aspect ratios, PAL gameplay or campaign completion.

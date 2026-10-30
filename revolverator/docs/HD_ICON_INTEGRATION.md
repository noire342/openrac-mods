# Native HD icon integration

The selected original glyph is bank **59901, frame 13**. `build_hd_icon.py`
extracts the actual 32×32 PSMT8 image and its CSM1 palette from a local native
RAM capture; it does not crop the contact sheet. It invokes the official
[Real-ESRGAN ncnn Vulkan runtime](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan)
with `realesrgan-x4plus-anime`, scale 4, producing a faithful 128×128 image.
The runtime, model files and game-derived input/output are local build inputs.

`encode_hd_icon.py` converts that image into a 255-color PSMT8 image and a native
CSM1 RGBA palette. Index zero remains transparent for the original HUD callback.
The generated header and images contain game-derived artwork and must remain
outside the public source tree.

## Adapter contract

`HdIcon` clones the image/palette page tables and appends HD and transparent
pages. It changes only Presentation's four appended frame references, leaving
the original shared bank and every other weapon reference unchanged. Inventory
grids and quick select draw the full 128×128 texture into their existing cells.

The native ammo HUD function derives icon dimensions from its texture. Merely
replacing its texture with 128×128 would enlarge the HUD icon four times.
`before_base()` temporarily points the four references at a transparent 32×32
page. `after_base()` restores the HD references, and `draw(x,y,alpha)` uses the
native sprite renderer with an explicit **32×32 logical size** and full 128×128
texture sampling. Native panel layout, ammo text, slide position and fade remain
the source of truth.

Add `src/hd_icon.cpp` to the mod target. For a private build, add the generated
header's output directory to include paths and include
`revolverator_hd_icon_data.hpp`. The runtime constructor receives the existing
API and the session's `GuestArena*`. The level-load hook reserves that arena
from the returned heap cursor before any presentation allocation. The native
HUD heap has insufficient spare capacity for an HD image and must not be used
for these runtime allocations. A missing/exhausted arena leaves the stock glyph
available. The no-arena constructor is retained for legacy host-test fixtures.

1. After `bind_icon()` has installed its cloned frame table, call
   `hd.install(first_appended_frame, 4, generated::hd_icon_data())`.
2. Call `hd.frame()` once per tick after binding. It invalidates only the
   appended GS page cache entries; native bank counts do not include them.
3. In `draw_base`, call `hd.before_base()`, the existing native callback, then
   `hd.after_base()` in that order. Always restore immediately after the
   callback, including frames where an additional charge indicator is hidden.
4. In the HUD post-draw path, after calculating native fade `alpha`, draw at
   `x + read<short>(element+0x48)`, `y + read<short>(element+0x4a)`.
   Do this before any `Phase::Spinning` early return.
5. Call `hd.reset()` **before** Presentation restores its frame table/animation
   definition, and reset all arena users before the session resets its arena.
   Reset also occurs before level memory is discarded.

Missing or invalid HD data, failed allocation, or conflicting table ownership
leaves the existing 32×32 native glyph available. A new level must construct or
reset/rebind the adapter. The adapter does not write into stale level memory.
If a later owner replaces the frame table, appended pages remain in the
level-owned arena storage so copied HD references do not dangle.

## Validation boundary

The independent adapter test covers fixed logical size, full-resolution page
metadata, native HUD suppression/restoration, transient GS cache invalidation,
single-allocation reuse, failure fallback, cooperative edits, and level teardown.
These tests do not prove the in-game renderer. Native preview checks and their scope are recorded in
[Validation](../../docs/VALIDATION.md#revolverator-source-preview). Public builds
use the original 32px glyph; Rilgar also uses that fallback in private HD builds
when presentation memory is insufficient. Full campaign coverage is not claimed.

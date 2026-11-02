# Component sources and owned-disc miniatures

The built-in source catalog identifies mechanical **sources**, not every object
whose class name contains `bot`, nor every object sharing a debris update.
Friendly Gadgebots, Infobots, weapon Doom Bots, NPCs, organic enemies and scenery
are excluded. The native factory's actual source and created fragment establish
provenance. The runtime registers new verified pairs using `builtin_part`.

The first eager descriptors have direct PAL source evidence:

| Source | Actual classes | Source reference |
| --- | --- | --- |
| Birdbot459 |1736,1737,1738,1770| `overlays/l01_novalis/elevator_and_birdbot.c`, `birdbot_update`, lines1193–1196 and1220–1223 |
| Peckbot577 |1747,1748,1749| same file, `peckbot_update`, lines1841–1849 |
| Kerwan robot dog573 |1742,1743,1917| `overlays/l03_kerwan/train_locomotive.c`, robot death case21, lines1832–1834 |
| Veldin robot1440 |1933,1934,1935,1963| `overlays/l00_veldin1/vendor_orb_and_robots.c`, death-knockback case, lines459–462 |

These paths refer to the PAL OpenRAC source tree, rooted at `games/rac1/src`.
The curated source roles are paired with the owned PAL format4 manifests' class
names. Availability in those files is static evidence, not native gameplay
certification for every source. Eager descriptor shapes are placeholders; the
native adapter should register actual fitted shapes on first creation instead
of eagerly freezing placeholder dimensions.

`tools/census_parts.py` inventories both PAL and NTSC source factory calls and
the player's extracted assets. The local audit records526 call sites (322 PAL,
204 NTSC),179 referenced fragment classes and37 curated mechanical sources.
Its call list also contains crates, organic parts and decorative effects; it
explicitly does not turn that list into runtime eligibility. Dynamic arguments
remain marked unresolved rather than guessed. No game art, mesh, texture or
absolute user path is included in the audit report.

## Actual component miniatures

`NativeThumbnails` reads the matching original GLB and PNG textures from the
player's own extraction, renders the triangle mesh with a CPU depth buffer and
quantizes it into a128×128 native PSMT8 texture with a CSM1 palette. It does not
substitute stock weapon icons, use AI artwork, require Blender, contact a remote
service or depend on Python in the running game. PNG decode/quantization uses
Windows WIC. The original sources remain unmodified.

Source GLB, texture contents and renderer version identify a private disk cache.
Cached texels/palettes have checksums; invalid or missing entries are regenerated.
The native inventory draw appends one temporary texture/frame in a private
guest-memory table copy, flushes native GIF paging, restores the active table
pointers and propagates original pages' paging state. Foreign ownership changes
are left intact. This avoids replacing the weapon icon banks used by other mods.

All72 supported source/model pairs were rendered and reviewed from the actual
owned-disc PAL meshes. Explicit localized labels in `src/builtin_part_labels.inc`
describe the visible component: for example **Testa di Birdbot / Birdbot head**,
**Cannone di Birdbot / Birdbot cannon** and **Mano di Birdbot / Birdbot hand**.
The complete [label table](COMPONENT_LABELS.md) keeps the robot's provenance in
the name and separately in the descriptor. Four ambiguous models use a neutral
component label with their source robot. Existing sidecars migrate generic
Componente A/B labels to these names without changing identity, physical/model
metadata or quantities. Custom API catalog entries retain their producer's names.

The original census contained37 candidate source classes, not37 proven native
families. The deeper PAL source audit finds17 standard-factory source paths,
four sources using other3D fragment factories, and16 with no recoverable model
factory in the checked death path. The scientist NPC1446 and ambient flying-car
alias1417 are excluded. A positive source/model pair is required: a mechanical
source alone cannot convert an unknown particle or decorative model into loot.
Source coverage and native scenario coverage are separate validation records.

`robot_salvage_thumbnail_test` accepts `MODEL.glb OUTPUT.png` for local asset
inspection. Its default synthetic fixture verifies rendering and table ownership
without needing disc assets; that host result is separate from a screenshot of
the real inventory running inside PAL OpenRAC.

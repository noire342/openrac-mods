# Weapon menu capacity audit

This audit is read-only and concerns the NTSC-U native implementation. The
current Revolverator replaces Walloper item **18**. It consumes the existing
cell, icon bank and quick-select entry, so it does not require smaller icons or
an enlarged menu.

For the player: **the Revolverator already fits the existing weapon menu. Keep
the present icon size.** A sharper128px texture improves its detail within the
same cell; shrinking the visible icon now would reduce clarity without making
another weapon available.

## Screen geometry

`MenuItemGridData` stores explicit rows, columns, margins, selected cell, cell
array and neighboring screens. `draw_menu_item_grid` iterates `rows * cols` and
passes explicit screen width/height to `draw_hud_sprite_subpixel`. Replacing a
32px texture with a 128px texture does not increase a menu cell's screen size.

Changing icon size alone does not add inventory capacity. A future grid change
must update its cell array and navigation together: `update_menu_grid_selection`
contains special mappings when focus moves between three-column and five-column
grids. The item slot editor has a separate fixed eight-slot quick-select array.

Relevant upstream source locations:

- `include/rnc/ui/menus/menu_screen.h:64` — cell fields and explicit grid layout.
- `include/rnc/ui/menus/menu_screen.h:124` — eight quick-select slots.
- `src/ui/menus/draw_menu_item_grid.c:60` — rows/columns, spacing and sizing.
- `src/ui/menus/draw_menu_item_grid.c:90` — drawing the cells and icon variants.
- `src/ui/menus/update_menu_grid_selection.c:73` — three/five-column transition.

## Inventory and save limits

The native item arrays do not share one extensible capacity:

| Native array | Declared entries |
| --- | ---: |
| `alternate_item_available` | 128 |
| `item_unlocked` | 32 |
| `weapon_ammo_counts` | 37 |
| `item_available` | 35 |
| `item_text_variant` | 40 |

Declarations are in `include/rnc/gameplay/state/item_state.h:8`. **37 ammo entries
does not mean 37 free weapon slots.** Existing IDs also include gadgets and
special items. A new item ID must be safe for every array and every caller, as
well as purchase, pickup, equipment and save serialization paths.

`build_filtered_menu_item_list` additionally enumerates a fixed 14 entries from
four native cell groups: six, three, three and two. This is a particular filtered
selection list, not proof that the entire inventory has a universal 14-item
limit. A future genuinely additional weapon must extend that list where used;
shrinking its visual icon cannot make it appear automatically.

Relevant upstream locations:

- `src/ui/menus/build_filtered_menu_item_list.c:49` — fixed collection loop.
- `src/ui/menus/update_menu_preview_class_pose.c` — iteration over existing
  0x25 item definitions.

## Recommendation

Keep the current layout and icon dimensions. For future additional weapons,
first introduce an explicit item registry and audit native ID bounds, native
save representation, all filtered selection lists and navigation resources.
Then choose either a coordinated native grid extension or paging based on an
actual capacity requirement. No native tables were patched as part of this audit.

A future additional weapon has these separate requirements:

1. An inventory identity valid across ownership, unlock, ammunition and text
   arrays, with a compatible native save representation.
2. A definition, ammo/pickup policy, vendor entry and acquisition path; visual
   class32000 is a render model identifier, not an inventory slot.
3. Entries in each applicable static/filtered selection list and preview table.
4. Coordinated grid cells, focus/navigation and selection resources. The current
   three/five-column transitions must be updated if those layouts change.
5. A decision about the independent eight quick-select slots; additional owned
   weapons do not automatically imply more quick-select buttons.
6. Explicit XP registration/migration if the new item must have independent
   progression. Replacing slot18 intentionally reuses that slot's XP identity.

This source audit establishes these constraints. It does not claim that the
inventory has already been expanded or that arbitrary new item IDs are safe.

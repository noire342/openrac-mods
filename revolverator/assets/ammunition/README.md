# Revolverator ammunition art

The pickup is an original mesh and painted texture generated from `recipe.json`.
A single industrial energy cartridge powers the drill motor. It has a red metal
barrel, brass endcaps and electrical contact, dark cooling ribs, and a small amber
charge window. The design follows the original drill's mechanical palette without
copying its geometry or texture. The rejected three-spare-drill-tip design remains
only in the earlier isolated QA output; this source now builds the single cell.

`tools/build_ammunition.py` builds the low-polygon model, its 128 × 128 texture
and a private native-level extension. The source reference is in metres; game
variants are divided by each level's native class-225 scale, as extracted R&C1
geometry is. The in-game pickup is approximately 0.39 metres tall.

The current renderer reads visual classes from level manifests. The generator
therefore clones all existing level manifests into an isolated output, adds
visual class 32000 for world pickups and alias class 2016 for the vendor preview
to the 19 gameplay levels, and links unchanged large assets
back to their installed location. It verifies the original manifest hashes after
preparation. The output is for runtime QA, not an ordinary launcher model pack
that can add a previously absent class.

Run the game with `--levels OUTPUT/levels` and the existing Revolverator model
override pack. The native runtime creates a class-225 ammo pickup and uses visual
class 32000 for Revolverator ammunition only. The vendor alias 2016 references the
same mesh and scale; its native resident slot is provided by `NativeAmmoAlias`.
OpenRAC's executable is unchanged.
Do not edit the linked original files through the QA output.

`tools/preview_ammunition.py` renders the actual generated GLB in Blender. It
undoes Blender's standard glTF axis conversion because OpenRAC GLBs retain the
game's Z-up coordinates.

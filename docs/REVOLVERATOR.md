# Revolverator — development status

Revolverator reconstructs the removed drill weapon for R&C1's native OpenRAC
port. While enabled, it occupies the Walloper inventory slot, preserving the
game's selection, vendor and save mechanisms.

The implementation is being refined in native gameplay. It is not a finished
release, and screenshots of intermediate poses are not final artwork or proof
that every scenario passes.

## Intended behavior

- Press and hold Circle near an eligible regular enemy to catch it.
- Ratchet lifts the weapon outside his head, then turns his wrist to point the
  drill upward.
- The enemy remains attached at the actual contact point while it rotates.
- Release Circle to throw it; locomotion must not wait for a returning wrench.
- The original weapon HUD shows ammunition, the selected native drill/cone glyph
  and a compact charge channel.
- The Walloper's electrical aura is removed. Melee strikes and thrown impacts
  should still damage appropriate crates.

The planned normal vendor introduction is **Hoven**, at **15,000 bolts**. Current
ammunition settings are **20 charges**, **4 per pickup**, and **25 bolts per
charge**. Regular ammo drops should include it only after the weapon is owned.
These settings are targets under verification, not a claim that vendor/drop/save
coverage has already passed.

## Assets

The original Hoven drill is a local art source. The player-side builder retargets
that model to the native held-item skeleton. It does not recover missing original
weapon animations; presentation uses native animation mechanisms and new pose
control.

The ammunition mesh is generated with AI assistance. Its final design is a
single industrial energy cartridge in the drill's red/brass/steel palette. An
earlier three-bit concept was rejected and is not the final design.

The custom pickup uses render class 32000 backed by a native ammunition entity.
An isolated generated level tree adds its metadata while preserving the installed
levels. It needs no OpenRAC executable modification, but it is currently separate
from ordinary launcher model-pack imports.

## Before a release

The release must identify the exact tested DLL and asset setup and distinguish
host tests from native evidence. Outstanding checks include natural poses from
multiple angles, reliable first-attempt captures, weapon visibility throughout
rotation, attachment/contact stability, crate damage, normal ammunition drops,
purchase/collection, persistence and coexistence with Weapon Experience.

The public copy-only C interface is documented in [API](API.md). Its version and
layout will be taken from the source included with the eventual preview.

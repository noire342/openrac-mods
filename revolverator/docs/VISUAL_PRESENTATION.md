# Independent visual ownership

This describes the presentation architecture retained in Rotatore 0.3.2.
Earlier DLLs and source-preservation counts below remain historical evidence
for those individual changes. Current-binary proof is recorded in the
[0.3.2 regression receipt](validation/pal-032-regression.json) and
[heavy-enemy receipt](validation/pal-032-heavy-enemies.json).

The Revolverator remains logical weapon `65554` (`noire342.revolverator`). Its
native transport is an adapter: the ordinary Walloper keeps its own ownership,
ammunition, XP and appearance. Visual code does not own combat or save data.

## Responsibilities

- `NativeWeaponVisual` owns separate held, standalone-preview, Ratchet-preview
  and acquisition poses. Logical equipment comes from the registry; a native
  actor disappearing inside a pause menu is not a new equip event.
- `NativeWeaponAlias` keeps the custom render identity alive across the frame
  that enters pause. Known native calls receive the canonical carrier only in
  an owned scope, followed immediately by publication of the custom model.
  Save restoration explicitly returns actors to the native transport first.
- `NativeRotor` neutralizes the carrier's finger deformation and composes the
  drill pose. Axial growth uses the real shaft socket from source joint35,
  including Y=-4.713099479675293. The motor housing and drill remain connected.
- `EquipAssembly` is a pure cosmetic controller: 0.60 seconds, a housing that
  builds from hidden to a 1.22x overshoot, followed by axial extension from the
  socket and two screw turns. It follows the staged native Blaster/Pyrocitor
  appearance sequences. Pause
  freezes it; attacks receive full geometry immediately. Empty ammunition
  prevents powered rotation. Previews and acquisition actors never trigger it.
- `NativeEquipAudio` consumes one semantic equip event, using the native
  mechanical draw cue. It transfers only the owned actor's audited appearance
  trigger; shared animation definitions and sound banks are never modified.

The existing attack pulse remains 2x radially and 1.2x axially. Holding an enemy
uses normal drill size and the existing 50% movement limit. Controller, native
combat/arm/ammunition, arsenal/save/XP, acquisition and exported API sources
were pinned before this change; all seventeen protected files remain byte-exact.
The 79 original asset files, both compatibility utilities and XP0.6.4 DLL are
also unchanged.

## Validation boundary

Tests cover native callback publication before the first visual frame, scoped
attachment, pause entry, resumed native callbacks, separate original Walloper,
preview interleaving, actor reuse, pause-menu actor reconstruction, real logical
selection changes, empty ammunition and immediate attack handoff. The complete
standalone Windows Release suite has 32 groups. Native receipts are kept by exact
DLL hash; failed intermediate builds are retained and excluded from acceptance.

PAL Europe v2.00 / official OpenRAC0.10 is the native reference. Host tests also
cover NTSC bindings; these visual fixtures are not whole-campaign certification.
## Current equipment-menu correction

The [equipment selection and construction fix](EQUIP_MENU_FIX.md) adds native
PAL Cross confirmation coverage and a longer staged construction sequence.
The previous receipt below remains tied to its original DLL and 0.30-second
envelope; it does not cover the corrected PAL equipment-menu confirmation.

## Previous native proof

The [final visual/assembly/audio receipt](validation/pal-visual-assembly-local.json)
binds DLL `4ffe8a5407ad4338b51d6d0a2ef2319e3e7cc435f21ebf69972171ef8398f8c3`:
base57/57, Gold69/69 and original Walloper20/20. All three processes exit0 with
107 original inputs unchanged each. Actual pause-entry, enlarged attack and
assembly screenshots were reviewed. Both actual equip paths play exactly one
accepted native bank3/sound127 command, with none after deep Armi/pause return.
Sound was verified at the native driver command, not by an audible listening
review of the muted fixtures. The final Windows suite passes32/32.

[Gameplay/XP regression proof](validation/pal-visual-gameplay-regression-cef442.json)
is kept separately and explicitly tied to the preceding `cef442` DLL: carry37/37
and level-up/title/equipped-preview49/49. The final DLL adds only suppression of
the owned actor's legacy appearance cue, without changing that visual or combat
state. Prior failed intermediate DLLs remain excluded in the final receipt.

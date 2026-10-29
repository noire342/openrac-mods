# 0.6.1

- Added `flip_morph_model`, off by default, in the existing launcher settings.
- Rolls only the held Morph-o-Ray model 180 degrees around its firing axis, keeping
  the barrel pointed toward the enemy. Native aim, beam and transformation are unchanged.
- Added the equivalent `WEAPON_XP_FLIP_MORPH_MODEL` flag (64) to configuration API v3.
  Consumers needing it must require provider version >=0.6.1.
- Added both-disc model isolation/restoration tests and native NTSC on/off gameplay proof.
- Provider metadata now takes its version directly from the package manifest.

The requested orientation is implemented; attribution to an original concept drawing
remains unverified. No historical claim is made by the setting.

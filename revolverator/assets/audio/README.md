# Original Revolverator audio

These WAVs are original deterministic procedural synthesis, with no game audio
or external sound samples. Their waveform bytes are preserved from the frozen
author handoff. The distribution license is GPL-3.0-or-later.

The English manifest lists each cue, its original SHA256, PCM parameters and
runtime purpose. source/manifest-original.json preserves the original handoff;
provenance.json inventories every distributed file. Both original generators
are included under source/. Generating new audio is a development operation
using NumPy and FFmpeg; players need neither dependency.

The original handoff contains 19 cues. This bundle distributes 14: the custom
ammo_pickup, dry_fire, equip and xp_level_up WAVs are excluded by the user's
request. Native ammunition-pickup and empty-fire sounds remain game-owned;
no custom construction or level-up cue is added. The original source
manifest and generators remain intact. acceleration_demo is an audition-only
demonstration and is not distributed in the runtime bundle. All 14 distributed
WAVs have runtime mappings.
Alternative spin layers are not simultaneous full-volume defaults.

User listening approval and final in-game mix approval are not implied by file
validation. Native validation must separately cover cue timing, pause, equipment
changes, empty ammunition, teardown and the actual gameplay mix.

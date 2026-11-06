# Revolverator audio status

The current runtime uses the original game sounds. The custom sound adapter is
not linked or connected, and no custom WAV files are included in the installable
archive. The native equip and gameplay sound paths remain in place.

The procedural sound experiment is parked in development source only, including
its waveform files, generators, tests, manifest and provenance. Earlier native
mix recordings apply only to the exact experimental DLL recorded in those
receipts. They do not certify the current native-only binary.

## Historical procedural experiment

The remaining document describes the parked experiment, not current playback.

The parked 0.3.4 experiment used 14 original mono 48 kHz, 24-bit PCM WAVs made with
deterministic procedural synthesis. It contains no game audio or external sound
samples. The source handoff contains 19 cues, including three additions for early
release, actual ground contact and full charge. Custom ammo_pickup, dry_fire,
equip and xp_level_up clips are excluded by the user's request; normal game
sounds remain game-owned. The audition-only acceleration_demo is also not distributed.
No selected waveform is resampled or
re-encoded during staging or packaging.

The source handoff manifest is pinned by SHA256
af6087bae1e02ff18aeee338852ee01facef44808ddff2217b94d1f3c8f93970.
assets/audio/manifest.json is the English runtime catalog with relative paths.
source/manifest-original.json preserves the original author handoff, and
provenance.json records the exact file hashes. Both original generators and
the GPL-3.0-or-later distribution license accompany the sounds.

## Cue map

| Cue | Intended event |
| --- | --- |
| idle_loop | Quiet equipped motor while ammunition remains |
| attack | Circle activation and attack pulse |
| capture | First successful capture |
| spin_loop | The held enemy's motor layer |
| spin_fast_loop | Alternative faster layer for comparison |
| throw | A real charged launch |
| heavy_hit | Melee contact with a target that cannot be captured |
| projectile_impact | Actual impact of a thrown enemy |
| gold_explosion_low | Lower-charge Golden explosion endpoint |
| gold_explosion_full | Full-charge Golden explosion endpoint |
| gold_acquisition | Golden upgrade acquisition |
| early_release | Deliberate drop before the charge bar is full |
| ground_contact | An actual observed landing after a drop |
| charge_ready | One crossing of the full-charge threshold |

This map states event ownership, not a claim that every cue has already been
heard in game. The two motor loops crossfade as charge and rotor speed rise,
with pitch updates and bounded gains. They are not simultaneous full-volume
layers. The Golden endpoints likewise avoid uncontrolled full-volume stacking.
All 14 distributed WAVs have runtime mappings. The manifest explicitly records
excluded_by_user as ammo_pickup, dry_fire, equip and xp_level_up, and preview_only as
acceleration_demo. The untouched 19-cue source manifest and generators preserve
the original handoff without adding those sounds to the game.

## Lifecycle and dependencies

WeaponAudio owns the event and mix policy. NativeWeaponAudio owns resources and
a budget of at most six active voices through the public OpenRAC sound API.
A motor must stop on pause, equipment changes, target loss, death and teardown.
An empty idle weapon is silent; a captive acquired with the last round keeps its
spinning motor until release. A resumed frame must not replay a backlog of event
cues. Release, impact and charge-ready cues
are edges, not events repeated every frame.

Players need no audio-generation tools. NumPy and FFmpeg are development-only
dependencies of the included original generators. Native runtime playback
loads the frozen WAVs from assets/audio/<cue-id>.wav.

## Packaging and verification

tools/stage_audio_bundle.py imports only the completed frozen handoff. It verifies
the source manifest hash, all 19 original waveform hashes and actual PCM frame
parameters, then stages only the selected 14 files and writes the relative
English catalog and file inventory.
tools/authored_audio.py is shared by source and native packagers; unlisted files,
changed WAVs, missing sources/licenses and unsafe paths are rejected.

The [targeted PAL receipt](validation/charge-release-0.3.4.json) records all 14
successful native loads, actual playback/stop handles and two recorded native
stereo PCM mixes against the exact DLL and sound bundle. The Base/Golden mixes
contain nonzero audio with no saturated samples. This establishes runtime
loading and mixing, not subjective listening approval or every cue's behavior
across the campaign. The author's source manifest keeps listening approval false.

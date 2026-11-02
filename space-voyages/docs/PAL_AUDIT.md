# PAL and NTSC native adapter

`src/disc_binding.hpp` selects an explicit address/function profile using
`openrac_api.game` and `.version`. Supported identities are `rac1/pal`
(OpenRAC's canonical PAL v2 / SCES-50916 program) and `rac1/ntsc`
(NTSC-U / SCUS-97199). Unknown versions fail to load. The PAL v1 and Japanese
programs require their own profiles and native validation; their addresses are
not inferred from a region name or a fixed delta.

## Address provenance

Paths below are relative to the OpenRAC checkout used for the matching runtime.
PAL names are the primary `games/rac1/src` / `games/rac1/include` program.
NTSC names are the independent `games/rac1/ntsc` program. The adapter retains
each program's own address and asks the SDK to relocate it on every use.

| Purpose | PAL v2 | NTSC-U | SDK relocation |
|---|---|---|---|
| Shared ship record |`0x0013E130` |`0x0013E030` |Executable |
| Destination coordinates |`0x0013DE48` |`0x0013DD40` |Executable |
| Native visit state |`0x0013DE60` |`0x0013DD58` |Executable |
| Resident boarding playback |`0x0018CC20` |`0x0018CB20` |Executable |
| Menu system |`0x001D5F70` |`0x001D5BF0` |Executable |
| Level cinematic mode |`0x0015F6A8` |`0x0015F5C4` |Level0 |
| Pending destination |`0x0015F6E4` |`0x0015F600` |Executable |
| Mirror flag |`0x0015EEB4` |`0x0015EDB4` |Executable |
| Frame scale / square / time |`0x0015EE60/64/6C` |`0x0015ED60/64/6C` |Executable |

Primary evidence:

- PAL `games/rac1/include/names.h`: `level_available`, `level_visit_state`,
  `level_render_state`, `menu_system`, `cinematic_mode`, `space_load_level`,
  `draw_map51`, `frame_scale`, `g_frame_scale_sq`, `g_frame_time`.
- PAL `src/overlays/shared/ship_mode_and_level_entry.c`:
  `initialize_moby_state_record` stores the actual ship pointer and variant
  into `D_0013E130`; the variant is at record offset`0x26`. Camera source,
  destination and index are at`0x34`, `0x38`, `0x30` respectively.
- PAL `src/game/space.c`: `SpaceTransition` consumes `space_load_level` and
  `level_visit_state`; it does not use an assumed NTSC-plus`0x100` address.
- PAL `src/game/pause.c` and `include/menu.h`: the menu state begins at
  `D_001D5F70`; menu transition fields occupy its leading words.
- NTSC `ntsc/include/rnc/gameplay/state/level_state.h` declares coordinates
  and visits at`D_0013DD40` and`D_0013DD58`.
- NTSC `ntsc/src/gameplay/state/do_space_transition.c` explicitly declares
  `space_load_macro` at`D_0015F600`.
- NTSC `ntsc/src/gameplay/state/update_level_gameplay_frame.c` and
  `ntsc/src/overlays/shared/world/loaders/load_level_gameplay.c` declare the
  shared ship record at`D_0013E030` and select its variant at offset`0x26`.
- NTSC `ntsc/src/world/transforms/build_object_rotation_matrix.c` declares
  `D_0015EDB4`; PAL `src/game/space.c` declares the mirror flag at`D_0015EEB4`.
- Cross-check `port/game/rac1-ntsc/host/rac1_version.h`: explicit mappings for
  PAL ship, menus, cinematic mode, pending destination and timing globals.

The differences are deliberately nonuniform: coordinates/visits differ by
`0x108`, menus by`0x380`, cinematic mode and pending destination by`0xE4`.
The variant write derives from the **relocated shared ship record plus0x26**,
so it cannot accidentally retain the NTSC address on PAL or an old overlay.

## Native departure initializer

PAL calls `func_L00_00290030`; NTSC calls `FUN_L00_0028ed58`. Both correspond to
`prepare_target_level_transition` in PAL
`src/overlays/shared/ship_mode_and_level_entry.c` and its NTSC counterpart.
`games/rac1/config/versions/pal/functions.tsv` and `us/functions.tsv` identify
the same344-byte function with fingerprint`a60e46c5966d56be`, at level0
`0x00290030` and`0x0028ED58` respectively. The SDK function lookup selects the
currently loaded native program's named implementation, including host hooks.

The native transition/save/runtime-initialization hooks keep their generic SDK
names across both versions. This adapter does not call Gemlik overlay code
from a different loaded planet.

## PAL timing and controls

PAL `PlayerFighterFlight` (`func_L13_002BB3E8`) and NTSC
`update_fighter_jet_flight` (`FUN_L13_002ba178`) have the same3820-byte
fingerprint`664c7da983bd5d46` in the per-version function tables. The canonical
source is PAL `src/overlays/l13_gemlik/turrets_and_fighter_jet.c`; the independent
NTSC source is `ntsc/src/overlays/l13/gameplay/entities/elevator.c`.

Retail PAL can select50 or60Hz. `src/game/mobyutil.c:set_video_timing` gives
scale/square/time of`1.2 /1.44 /.02` at50Hz and`1 /1 /1/60` at60Hz. Steering,
bank and camera use those globals. The FOV spring uses fixed`.05 /1 /1`
coefficients even at50Hz, so scaling every spring would not match retail.

OpenRAC runs both supported versions at60Hz:

- `port/game/common/program/vsync.cpp:tick_rate` returns60.
- `port/game/common/options_menu.cpp:update` actively writes the native video
  mode byte to0, the game's own60Hz option.
- `port/game/common/program/game_host.cpp:openrac_game_set_video_mode` documents
  that policy; level initialization sets the corresponding native time base.
- `port/game/common/frontend/transient_pacing.h` advances transient scenes at60Hz.

Space Voyages therefore retains its fixed60Hz simulation and continuous visual
companion on PAL. Region selection applies **no speed multiplier**. Immediately
before boarding takeover, and again before entering the scene, it reads the
active native timing globals. Nonfinite, missing,50Hz or inconsistent values
leave the original ship journey in control and emit a warning. The mod never
writes the native timing globals.

The PAL pad decode in `src/game/pad.c` uses the same centre127, dead zone48,
remaining range76 and mirror convention as NTSC. PAL
`src/overlays/shared/hero_animation_slots.c` copies stick axes directly and
uses the same raw-D-pad fallback below radial magnitude`.25`. The portable
input adapter is shared; Cross remains turbo and Triangle is destination entry.

## Verification boundary

`tests/disc_binding_test.cpp` covers explicit identities, every audited address,
correct relocation service and level number, changing overlays, variant-write
derivation, function names and timing fallback. It rejects guessed uniform
region deltas and unsupported identities. Its source-backed constants are
separate from game-dependent address resolution.

This source audit does not itself establish native PAL in-game success.
The separate `qa-space-pal-native-01` fixture loaded all three hooks and
completed natural Triangle boarding, Kerwan→Novalis and native landing with
physical ship531. Current0.3.3 fixtures repeat natural boarding/landing in
4:3 and16:10. The resolved PAL runtime `cf968a37…` completes all18 destinations
and the final Kalebo→Kerwan return with Weapon XP and Cinematic Journeys loaded.
All252 progress comparisons pass; physical531 is preserved on every trip.

The formerly blocked return was traced to a generated cast call that discarded
the boot-only scaler's identity: its address collides with Kalebo's UI drawer.
FadeToBlack received1446528 frames instead of12. The generic generator fix
preserves explicit executable-only symbol identity without changing dynamic
overlay lookup precedence. See `VALIDATION.md` for exact hashes, negative
controls, the source-level regression test and native evidence boundaries.

The separate PAL card fixture creates an empty card with native routines,
saves a diagnostic bolt change at departure, closes in FreeFlight and
reopens the checkpoint on Kerwan with the changed bolts. It uses no NTSC
payload and does not establish cross-region campaign migration. Ordinary
UI acknowledgement and save enablement are part of that private setup.

All ship variants, narrative paths, graphical replacements and the complete
input/UI matrix still require their own isolated runtime proofs. NTSC runtime
evidence must not be reported as PAL evidence.

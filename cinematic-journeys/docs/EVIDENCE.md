# Native integration evidence

Reference checkout: OpenRAC 2a457b30426ebf01caf40b2831f27ae647d7ad4a.

The only game hook is the stable `rac1.pad.process_report`, `void(u32,u32,i32)`.
SDK `on_pad` runs before `skip_after_mods`; the movie player reads on_pad reports directly, without Pad_ProcessReport. Ordinary menus/gameplay reach Pad_ProcessReport after automatic skip expansion. This allows removing unauthorized Start before automatic expansion and restoring it only for ordinary gameplay. Movie detection does not rely on guessing a game-mode value. Sources: `port/game/common/lib/pad.c`, `program/game_host.cpp`, `program/movie.cpp`, `frontend/input.cpp`.

The original in-engine cutscene accepts Start with all four shoulder buttons held. Scene mode is 2; ship landing/takeoff is mode 6. Ship scenes also accept Triangle/Circle/Cross, whose digital and pressure bytes must be filtered together. Sources: `games/rac1/src/overlays/shared/tieproc_00299108.c` (`update_cutscene_playback`), `games/rac1/src/game/space.c`, `port/game/rac1/host/boot.c`.

In an ordinary interplanetary flight, ship state is 4, loader busy is 1, completion is 0. Ship +0x5c is the pass selection phase: 0 -> another cruising pass, 1 -> final pass, 2 -> finish. While the minimum has not elapsed, change only phase 1 back to 0 before the flight's update. Preserve phase 2: it includes special story arrival sequences and already queued final passes. No sleeping or synthetic extra updates; original camera, banks, graphics and loader run each tick. The original completion releases loader resources only after the ship finishes; this mod never clears/recreates those resources or overrides completion. Sources: `space.c` (`initialize_level_runtime`, transition loop), `port/game/rac1-ntsc/hand/functional/FUN_0022eaa8.c` and PAL counterpart `games/rac1/nonmatching/functional/func_0022FDC0.c`.

Addresses (guest resident, except relocated completion and level mode):

| Field | PAL | NTSC-U |
|---|---|---|
| Pad state | 0x13ca40 | 0x13c940 |
| Ship struct | 0x13e130 | 0x13e030 |
| Loader busy (i16) | 0x15ef4a | 0x15ee4a |
| Completion (relocate_data, i32) | 0x15f6fc | 0x15f618 |
| Game mode (relocate_level_data(0), i32) | 0x15f6a8 | 0x15f5c4 |

PAL/NTSC mapping: `port/game/rac1-ntsc/host/rac1_version.h`; first-pad address also matches the official lock-strafe mod. Tests deliberately use distinct relocated addresses to catch direct access mistakes.

Compatibility limits: SDK hook signatures are checked by the host at load. Existence of the expected native flight update is checked before enabling the adapter. This is not a universal future-build layout detector; a future port changing these guest structures requires an adapter update.

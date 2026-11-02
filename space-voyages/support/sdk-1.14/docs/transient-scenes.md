# Temporary scenes (experimental interface 1.12)

This extension is based on the official OpenRAC 0.10.0 source. The three
official 1.10 developer-menu entries remain unchanged; the two scene entries
are appended after them. It is not part of stock 0.10.0.

`transient_run(mod, source)` is synchronous. A loaded mod calls it from a game
hook or tick after its native departure animation. It owns the window until
`source.tick` returns zero. One owner is allowed; nested requests return TAKEN.
Calls during mod load, invalid sources and unavailable windows return UNKNOWN.

The original game call stack is suspended: no level is loaded, no world AI,
campaign progression, native frame counters, or other mods' frame callbacks
advance. `play_status` reports SCENE while the lease is held. The window and
host diagnostic frame counters advance for scripted input and screenshots.
Normal game option pages, queued travels and native menu updates are suspended
too. The scene owner handles its own pause/navigation, without game RAM writes.

`tick(user, seconds, held, analog)` precedes drawing and receives 1/60 second,
active-high PS2 button bits and RX, RY, LX, LY bytes. Keyboard bindings and the
same controllers as native play feed this report. Cross is 0x4000, Triangle
0x1000, Start 0x0008. A zero return releases the scene. A callback exception
returns UNKNOWN; the mod may then retain the original game's travel behavior.

`draw(user, width, height)` has a black-cleared framebuffer and viewport bound.
Colour/depth are cleared, depth to 1. A mod with an ordinary OpenGL projection
should enable depth testing and choose LESS. `transient_gl_proc` returns the
port's renderer dispatch, including the Vulkan translation implementation;
never mix these pointers with native driver GL calls. Only the functions
named by `OPENRAC_GL_FUNCTIONS` and `OPENRAC_GL_DESKTOP_FUNCTIONS` are exposed.
They are usable only on this scene's thread and while it owns the lease.

`closing(user)` is optional and runs exactly once with a live context before
return, including callback exceptions and window close. Release shaders,
buffers and textures here. The source is borrowed only for the blocking call;
none of its callbacks outlives it. Closing the window runs ordinary program
shutdown after callback resources have been released.

The last presented scene frame becomes the native renderer's retained picture
on release, including the background used by native fades and menus. A zero
tick return does not draw another frame: to finish on a fade or black frame,
present it on the preceding tick. Native camera and guest memory remain intact;
temporal and interpolation history do not cross the scene boundary.

Interface 1.12 appends optional `draw_interpolated(user,width,height,alpha)`
after `closing`. The original 1.11 source prefix remains valid. A source may
provide this callback instead of `draw`; when both are set it takes priority.
Simulation runs at fixed 60 Hz independently of display/cap presentation. Save
previous/current presentation snapshots in `tick`, then blend them with alpha
in 0..1. The first state uses alpha 1 until two ticks exist. Original-rate and
legacy drawing remain at most 60 Hz at alpha 1. A wall-time stall catches up
at most 100 ms; each simulation call still receives exactly 1/60 second.
Several catch-up ticks may run before a draw. An owner that requires a final
black frame must keep returning 1 until its draw callback acknowledges that
frame, then return 0; a tick returning 0 never schedules an additional draw.

Host diagnostic counters advance per simulation tick. Intermediate screenshots
use the native `_bN` naming when `OPENRAC_SHOT_BETWEEN` is set. The development
benchmark `OPENRAC_UNCAPPED` deliberately uses one fixed tick/draw at alpha 1
without waits. `OPENRAC_TRANSIENT_TRACE=path.csv` records every presentation's
elapsed/delta time, simulation count, alpha and event/tick/draw/present/wait
stage times. Native game RAM and counters remain suspended throughout.

`OPENRAC_PRESS`, `OPENRAC_SHOT`, `OPENRAC_SHOT_FROM`, and `--frames` also cover
the temporary frames.
No game assets, spacecraft rules, destinations, galaxy data or UI are provided
by this generic host facility; those remain the mod's responsibility.

## R&C1 travel hook

`rac1.space.transition`, signature `void()`, wraps the native space transition
after take-off and before visit flags, destination resources and loading are
changed. Its originals are PAL `func_00233308`, NTSC-U `FUN_00231ff0`. The mod
must preserve mandatory story branches and call the original once after
selecting a valid destination. The ordinary game remains unchanged when no
mod binds this hook.

`rac1.memcard.save_data`, signature `i32(i32,i32)` (`slot`, `flags`), wraps
native serialization: PAL `func_0020BFC8`, NTSC-U `FUN_0020b178`. A nonnegative
`flags` value selects the planet written into the save; `-1` keeps the current
planet. Travel adapters that prepare a provisional departure must keep the
origin in that save until the player actually chooses a destination. Scope
that argument adaptation to their own preparation call; preserve other saves.

`rac1.level.initialize_runtime`, signature `void()`, wraps the resident
transition-scene initializer: PAL `func_00232278`, NTSC-U `FUN_00230f60`.
The original transition has already selected the destination and presentation
variant when this hook runs, but has not loaded the shared ship archive or the
destination's assets. It runs once in the ordinary transition; the destination
overlay subsequently loads its textures/gameplay and arrival from the resident
state left by that transition.

For an authorized travel adapter, ship presentation uses a resident signed
16-bit variant at NTSC-U `0x0013e056` (PAL `0x0013e156`), with values 0..2 for
classes 531..533. Capture the actual departure ship's class. Adapting this
value before the initializer selects consistent archives, cinematic objects,
textures and the parked arrival ship. Changing it only in `load_gameplay` is
too late because the ship archive and textures have already been selected.
Scope adaptation to the adapter's confirmed native transition call, leave the
chosen variant in resident state after the initializer, and preserve ordinary
story acquisitions and every transition outside that scope.

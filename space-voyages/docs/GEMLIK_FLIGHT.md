# Gemlik flight adaptation

The pure C++ flight controller adapts the original `update_fighter_jet_flight`
(`FUN_L13_002ba178`, PAL `func_L13_002BB3E8`) from OpenRAC/rac1-decomp,
GPL-3.0-or-later. It does not call a function whose Gemlik overlay is absent.
Native game addresses, sound handles, Moby lifetimes and progression are outside
this module. The runtime owns boarding, departure, arrival and resources.

## Retained equations

- Pad input retains native centre127, a48-step raw-axis deadzone and76-step
  remaining range (`Pad_ProcessReport`, `FUN_00217328`). Hero input chooses
  physical D-pad buttons if the post-deadzone stick magnitude is below `.25`.
  The original horizontal mirror setting is respected. Default vertical
  control is Gemlik's aircraft convention: push up to dive, pull down to
  climb. Optional `direct_vertical` flips only that input boundary; it leaves
  flight springs and camera equations intact. Cross remains turbo.
- At native60Hz, speed is displacement per tick. Cruise `.4` corresponds to
  24 world units/s. Cross boost adds `.035` per tick up to `.5` (30units/s).
  Release approaches cruise by ten percent per tick; below cruise it adds `.025`.
- Stick targets use `vendor_spring`: error gain `.03`, damping1, velocity bounds
  `.1` horizontal and `.25` vertical. Roll uses gain `.05`, damping1, bound1.
- Yaw/pitch subtract smoothed stick times `.012271847`; pitch is limited to
  `±1.3962634` radians. Model lean retains `roll + steerX*.5` and
  `pitch - steerY*.5`.
- Camera offsets retain `(-15-steerY-boostFraction*4, -steerX*3,
  4+steerY*2)*.25`, with gain `.15`, damping `.8` and bound1.
  Camera pitch includes `.09817477 - steerY*.012271847`; roll includes
  `roll*.5 + steerX*.03125`.
- FOV springs between `1.134464` and `1.4835298` (65–85 degrees), gain `.05`.
- Scalar and angular springs retain the native clamped damped-velocity helper
  `FUN_L00_0025b798`, scalar snap `0025b8c0`, and wrapped-angle snap `0025bc98`.

## Continuous presentation

`state()` preserves those native equations, including their final `limit*.01`
snap. `presentation_state()` is an independent visual companion: it takes the
same corrected physical position, yaw, pitch and speed, and advances cosmetic
steering, bank, camera offset/orientation and FOV with the **same helper and
coefficients**, without the snap threshold. It never feeds movement or geometry.
This avoids the abrupt last camera correction after releasing the stick and
the FOV jump after releasing turbo. It does not add a second smoothing filter.

The host's optional SDK1.12 `draw_interpolated` callback supplies a fractional
time between fixed60Hz simulation snapshots. Hull, camera, FOV and radar use
that same fractional time, with shortest-arc interpolation at angle wraps.
Pause does not keep animating an old pair. The final Arrival frame is presented
at its completed state so its black handoff remains exact. Original60Hz mode
and the explicit uncapped QA mode draw complete ticks without interpolation.

## Intentional separation

The native mount/launch timing runs in the original departure sequence. Free
flight starts at cruise after that sequence; the two-second fighter mounting
delay is not replayed. The controller excludes guns, rockets, health, sounds,
boss scripts, mission paths, enemy targeting and native arena collision damage.

The original arena guide-to-centre logic is replaced by catalogue geometry:
each body has a keepout sphere (surface+10 units), a slowdown/slide band and
a prompt range (surface+50). A swept segment prevents penetrating a body.
An outer guide begins at1,300 units, with a hard1,450-unit radius as a final guard.
None of these boundaries can trigger an arrival. Only a fresh Triangle press
near an unlocked contact starts the one-second approach. Coordinates are
rechecked during approach before emitting a single destination.

Simulation advances at60Hz, independently of rendering frequency. Paused calls
do not advance simulation or approach time. An OS stall is capped at250ms;
nonfinite clocks/sticks cannot poison the world. Standard C++ trigonometry is
used rather than the original approximations: the arithmetic and coefficients
are retained, but this is not a bit-exact floating-point matching claim.

## Catalogue and prototype

The first three bodies are Novalis1, Kerwan3 and Eudora4 (native ID2 is Aridia). Their coordinates
are fixed regardless of unlocks. Radius and position are rendering-independent
data, checked for overlap, finite values, duplicate body IDs and galaxy bounds.
The catalogue is non-owning and must outlive its controller.
`celestial_body_for_level` merges Veldin0/18 for future entries; the runtime
must still select the actual native landing variant from progress. Orxon is
level10 for both character visits; level14 is Oltanis and must never be merged
with Orxon. Evidence: native `host/front_end.c` and SDK `behaviour.json`.

`flight_test` checks native golden values, render-rate independence, angle
wrapping, pitch, camera recovery and invalid inputs. `voyage_test` covers
locked contacts, unlock/revoke during a held button, pause, one-shot arrival,
cancel/restart and three minutes of continuous boost against body/world bounds.
`input_adapter_test` checks native pad boundaries and D-pad fallback, both
vertical conventions, movement and camera-space direction, and bank recovery
after a short control pulse. `render_math_test` compares hull and camera axes
against the native `Rz * Ry * Rx` Euler sequence, including opposite bank signs.
These are host tests, not evidence of in-game boarding or rendering.

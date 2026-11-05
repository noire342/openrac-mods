# Drill contact sparks

While a valid captured enemy spins on the drill, the weapon emits small native
metal-contact streaks at the actual drill tip. They use the game's resident
wrench-contact effect rather than an imported texture or an electrical aura.

The effect is cosmetic. It does not post damage, change ammunition, move the
target, award XP, alter the drill's geometry or draw from gameplay randomness.
Base and Golden use the same contact effect. The Golden explosion remains a
separate effect triggered by a thrown enemy's collision.

## Ownership and timing

ContactSparks is a pure rate limiter. It receives elapsed time, the target's
generation token and angular speed. The rate grows from 24 to 42 streaks per
second across the normal spin-speed range, with at most four emissions per
frame. Invalid time does not accumulate an emission backlog.

NativeContactSparks supplies short tangent/outward velocities around the shaft
and uses the aligned contact point already computed by the held pose.
Dispersion is deterministic and local to the visual component. The native
particle system owns each streak's short lifetime; no particle pointer or
temporary stack address is retained by the mod.

Emission stops when the target is released, lost or destroyed, when equipment
changes, and during menus or pause. Previously emitted streaks finish their
native lifetime. Pause does not accrue a burst for the first resumed frame.
Level teardown clears the rate limiter and target token before resources are
released. Selecting or attacking with an empty drill does not emit contact
sparks.

## Validation

Host tests cover timing at different frame steps, bounded emission, pause,
invalid input, target replacement and teardown. Native validation must also
show that streaks originate at the current contact point while the enemy turns
and stop being emitted after release. Renderer observations and the exact
native binary belong in the release's validation receipt; host success alone
is not proof of the visual result.

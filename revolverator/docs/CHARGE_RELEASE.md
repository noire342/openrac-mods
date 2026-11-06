# Charge-gated release in 0.3.4

Tap Circle for the normal melee strike. Capturing still requires an uninterrupted
minimum hold of 0.18 seconds. After capture, the existing charge bar reaches full
after 1.6 seconds of spinning. Releasing with the bar already full launches the
enemy; holding for the maximum three seconds still launches automatically.

Releasing before the displayed bar is full deliberately drops the live enemy on
the floor in front of Ratchet. The native adapter restores the actor and allows
its own behavior to resume. A nearby, visible enemy posts one native melee
counterstrike against Ratchet (mask 1, damage 1), using the original enemy as
the source. The game's hero update owns damage, Hurt, knockback and invulnerability;
the mod never edits health or resets class-specific enemy state. An obstructed
path or distant fallback cannot cause a phantom ranged hit. It is not a thrown projectile:
there is no flight, thrown-enemy impact, Golden explosion or launch animation.
The activation's ammunition remains spent.

The controller decides from the charge displayed before the release frame.
It does not add that frame's elapsed time and turn an early release into a throw.
An input just below full and an input exactly at full are separate test cases.

## Ownership

Controller owns the input/charge decision. World::drop is a distinct operation;
NativeWorld owns safe floor placement and restoration of the real enemy. The
early-drop path clears the current capture before calling the adapter, so a
reentrant callback cannot release it twice. It does not cancel an older enemy
that is already flying.

Death, equipment changes, target loss and level teardown remain safety
cancellations. They must not be treated as a deliberate early-release event or
force a counterattack in an invalid scene. No other capture eligibility, native
Walloper melee rules, full-charge timing or maximum automatic launch changes
are intended.

## Golden and sound behavior

An early drop does not create a Golden explosion, regardless of the active
upgrade. A fully charged thrown enemy retains the normal Golden collision
effect, whose blast continues growing with spin time up to its existing cap.
Switching equipment does not alter an older projectile's launch provenance.

The current runtime retains the original native sounds and loads no custom cues.
The procedural early-release, landing, charge-ready and motor sounds remain
parked in development source. They do not change current playback.

## Verification status

The current [native-only receipt](validation/native-audio-0.3.4.json) records
81 passing PAL Base/Golden checks against DLL 3282185c…8fcaa63, after restoring
native sounds. Both processes exited normally, original files stayed unchanged,
and no custom sound events or runtime WAVs remained. Native mixed PCM was
nonzero. This rollback also passed all 38 Release host test groups.

The [historical custom-audio candidate receipt](validation/charge-release-0.3.4.json)
records 81 passing PAL Base/Golden checks against DLL 23c9064c…8f2f35b. They include
early drop, native counterstrike and resumed AI, manual full-charge throws,
the maximum automatic Golden throw, and pause/resume with Circle held.
The input adapter retains physical Circle reports through pause filtering;
releasing Circle during pause remains a real release when gameplay resumes.
Host checks cover the threshold, bar pixels, cancellation and input boundaries.
These are targeted scenarios, not whole-campaign certification. That receipt
applies only to its historical custom-audio binary. Previous 0.3.3 receipts
likewise remain evidence only for their original binary and behavior.

# Golden effects and weighted throw

## Version 0.3.1 on OpenRAC 0.10

The base and Golden weapon share the same Circle activation pulse. Even a
one-frame tap retains a 160 ms motor pulse with a short scale fade: head joint 4
reaches 2.0x radial scale, 1.20x axial scale and 55 rad/s. The handle and housing
remain rigid. Entering `Spinning` clears enlargement immediately to 1.0 and
locks rotor motion to the captive's angle. An empty weapon stays still at
idle; its existing dry-fire jerk returns to the initial angle without growth.

This supersedes the earlier 28 rad/s, 1.25x/1.10x activation tuning. It does not
change the Gold explosion's charge, radius or damage rules below. Native 0.10
receipts for the final pulse build cover short/long holds, Gold purchase without
XP, a Gold level-up, weapon switching in flight and save/reload. Each combat
receipt checks the large activation pulse and normal size during the spin.
The separate base-tap and rapid-reuse results are recorded in
[current validation](NATIVE_VALIDATION.md).

The Golden Rotatore uses the original weapon's geometry, UVs and rotor
binding. A separate texture and render class 2018 give it a warm Gold finish.
The Novalis case has an independent rigid display 2019, so its mesh cannot
depend on the weapon Ratchet is holding or on a streamed hand pose.

An uninterrupted hold spins the enemy. Explosion charge grows linearly from
zero to one over the three seconds before automatic release. It is separate
from the original throw's velocity charge, which still reaches full strength
after 1.6 seconds. Radius grows from 2.5 to 6 game units; maximum damage grows
from 8 to 16 before optional XP bonuses. Damage falls to 35 percent at the rim.
The native blast particles, light and camera response also grow with charge.

The shot stores its variant, charge, damage multiplier and source generation
at release. Switching weapons does not change an enemy already in flight.
Only an actual collision produces an explosion; cancellation and flight
timeout do not. Timeout retains the original direct-impact fallback.

The native visual burst and native damage packets are separate, preventing a
second hidden radial hit. A bounded 48-entry plan deduplicates actors, gives
priority to the thrown actor and actual contact, and then admits the nearest
visible combat actors/crates. Native damage handling retains boss armor,
invulnerability, death, drops and experience attribution. Ratchet, story NPCs
and actors behind blocking geometry are excluded.

Every throw has a 150 ms locomotion hold. It starts at release and does not delay
the projectile. The adapter neutralizes only verified horizontal movement
fields, retaining vertical motion, camera, AI and native animation. A pause
preserves its remaining duration. Death, scene changes and switching away
cancel the locomotion hold.

Version 0.3.1 separates the current attack from up to eight independently owned
flights. A new press is buffered during the 150 ms recovery; an older collision,
death or equip switch cannot reset a new captive. Each flight retains its own
charge, Golden variant, damage scaling, lease and XP provenance. Pending native
hits share one 48-hit budget per frame, with a bounded deferred queue.

The release has a 250 ms arm follow-through, interrupted cleanly by a new strike.
Head growth and motor timing follow the activation pulse described above,
around its fixed motor pivot. They are independent of a projectile's lifetime.

API version 3 extends the complete version 2 prefix with `get_upgrade`, which
copies Golden ownership, purchase cost, current blast charge, recoil state
and explosion/damage counters. It exposes no guest pointers. Stable weapon
key, ammunition, XP and quick-select bindings are shared across the upgrade.

# Heavy enemies: native melee, no capture

The capture policy excludes these native enemies:

| Native actor | Class | Example level |
| --- | ---: | --- |
| Bomber Tank | 253 | Batalia |
| Extermitank | 625 | Rilgar |
| Alien Snapper (beige armored crab) | 1048 | Nebula G34 |
| Toxic Crab | 1202 | Orxon |
| Rocket Tank | 1454 | Veldin |

The same policy applies to Base and Gold. Holding Circle never creates a carry
lease for these actors: their AI, position and collision remain owned by the
game. Their ordinary Walloper hits are forwarded unchanged, leaving native
armor, hit reactions, vulnerability windows, damage and death handling intact.
Capture immunity does not grant immunity to Golden impact explosions.

Regular enemies can be captured using the game's creature, aim and health
metadata. This covers enemies such as the Hoven Anklebiter without requiring
an individual AI hook for every enemy. Existing class bindings also cover
enemies with different native targeting data, such as the Blarg Commander.
Bosses and scripted targets such as turrets, mines, spawners and allied
Gadgebots retain their native behavior. A carry lease suspends only the live
actor's update and restores it when control returns to the game.

Tanks and Toxic Crabs were already outside the capture allowlist. Version 0.3.2
removes Alien Snapper1048 from it, and adds the distinct Blarg Commander1371.
The Commander has its own official native update hook, so its native motion
pauses while the weapon owns its carry lease. Both Base and Gold can capture it.

Version 0.3.2 also makes damage preservation part of the native hit-filter
contract itself, rather than
depending on a separate class check at its caller. Only a current, capturable
actor may have the carrier's melee hit suppressed before capture. Stale actor
generations cannot suppress native damage either.

The IDs come from the official RaC1 moby-class records, not display names or
size guesses. In particular, Scoutbot252 is not BomberTank253, and
Extermibot623 and Extermitank625 are distinct classes.

Host regressions cover targeting, rejected capture, native-hit forwarding,
unchanged actor state, Gold blast eligibility, stale targets and selection of a
light enemy behind a closer heavy actor. Native validation is recorded against
the exact release DLL, separately from host tests.

## Version 0.3.2 native proof

The [heavy-enemy receipt](validation/pal-032-heavy-enemies.json) records
33/33 native checks for Alien Snapper, Blarg Commander and Bomber Tank.
The [regression receipt](validation/pal-032-regression.json) adds 264/264
checks in six equipment-menu, Base/Gold presentation, original Walloper and
carry/release scenarios. All nine processes exit 0 with 107 original input
files unchanged in each run. Both receipts identify release DLL
`9d9ba8704588b5838589d6d0042e7a8cc8ee972b4e0fee24cf3c26672fce7a52`.
The full standalone Windows suite passes 33 groups. Other tank classes and
Toxic Crab are covered by the source policy and host regressions; these native
fixtures do not establish every enemy, level or vulnerability phase.

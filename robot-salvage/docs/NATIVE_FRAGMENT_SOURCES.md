# Native fragment-source adapters

The adapter observes four specific native factories after they return. It admits
only catalog-approved source/model pairs, copies robot provenance immediately,
and decodes each factory's own motion fields. It never hooks generic particles,
`CreateMoby`, live projectiles, or ownerless decorative debris.

| Hook | PAL source symbol | Native signature | Result | Velocity / spin in pvars |
|---|---|---|---|---|
| `rac1.moby.spawn_flying_debris` | `func_L14_002BC560` | `u32(u32,f32,u32)` | 81 | `+00` / `+14` |
| `rac1.moby.spawn_plasmabot_death_debris` | `func_L14_002DFE98` | `u32(u32,u32)` | 325 | `+00` / `+10` |
| `rac1.moby.spawn_path_or_ballistic_debris` | `func_L14_002E1570` | `u32(u32,u32,i32)` | 403 | mode1: `+30` / `+20` |
| `rac1.moby.spawn_bomber_jet_debris` | `func_L01_0030D248` | `u32(u32,u32,f32)` | 1510 | `+00` / `+10` |

Native displacements and rotations are per frame. The adapter supplies
`FragmentMotion` in world units and radians per second, using the current
validated native frame duration. The timer/light words at81's `+10` and403's
owner/path fields must never be decoded as movement.

The source-backed combinations are31→81,28/211→325,28→403mode1,
211→403mode0,688→1510. Catalog validation also rejects unknown visual models.
The actor's pvars owner reference must equal the factory's actual source.

## Scripted class403 phase

The source comment saying mode0 later becomes ballistic is misleading. The
checked update routine follows its spline until its timer expires, then directly
explodes and deletes the detached body. It never switches mode0 to mode1.

The adapter lets that native trajectory continue, samples its position on every
gameplay frame, then adopts it when the native counter reaches2 or less, before
the next native destruction tick. Velocity comes from the last actual position
sample, including vertical displacement; spin is zero during this spline phase.
It never reads uninitialized mode1 velocity fields while mode0 is active.

Provenance is a copied `FragmentOrigin`. A removed or recycled owner cannot
rename the component. Pending handles validate native class, pclass, pvars,
update callback, owner-reference field, visit, observed allocation generation
and matrix-counter continuity. A new observed factory result at the same native
address replaces that allocation's pending generation. The caller must reset the
adapter before every gameplay load, including same-planet respawn.

## Integration contract

Construct `NativeFragmentSources(api, adopter)`. The adopter receives copied
origin, a currently validated native actor and decoded motion. A true result
means ownership was accepted exactly once; false means no mutation committed and
permits retry. Its callback may reset the adapter safely.

Call `observe` after each original factory, passing its actual owner, returned
actor and mode for403. Call `frame` before the world physics step. Non-gameplay
menu contexts defer adoption. The decoder never writes game memory itself.

Host fixtures verify decoding, deferred expiry, copied provenance after owner
reuse, allocation reuse, stale handles, pause, load/reset and retry. They do not
establish in-game coverage of the four factories.

## Shipped-binary evidence

The official Windows PAL0.10 executable SHA256 is
`e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e`.
Named function records bind the four symbols to hostVAs431E40,54DDD0,550EA0,
389980 under image base140000000. `.pdata` gives exact body extents. Byte-exact
isolated functions were inspected using REA/Ghidra12.1.4 without executing the
game; removed callee/data sections remain explicitly unresolved.

REA parent evidence IDs, in the table's order:

- `ev_3377308a09398be7df518081b3e0d875d12af8f3739ea4aa270987ee367d4fff`
- `ev_40b3afe466547c1819b351bcd8ab6f398126de27c1f0c330d8ff5a7b95d2374e`
- `ev_36ea471416c400d3468e186e5a3695d02d60daeace512da093924741c6b01e58`
- `ev_459b6c42476b64fc8c27a80e48483050d1512425aa624d6485e8d643bf13aae7`

The ABI evidence includes nativeXMM1 for81's second float argument andXMM2
for1510's third float argument, integerECX/EDX for325, andECX/EDX/R8D for403.
Local retained extraction metadata and complete instruction evidence are under
`work/salvage-native-audit/alternates-{extraction,evidence}.json`; extracted disc
meshes and analysis-only executable fragments are development evidence, not
redistributable mod assets.

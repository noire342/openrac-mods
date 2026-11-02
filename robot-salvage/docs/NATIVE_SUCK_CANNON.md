# Native Suck Cannon component transfer (PAL)

The adapter consumes an existing physical component as ammunition. It preserves the same native Moby address, fragment class/model, copied component identity and source robot. It creates no enemy, calls no enemy update and adds no inventory item. Native Suck Cannon animation, capacity, aiming, projectile collision, lifetime and terminal explosion remain the game's own paths.

## Source contract

The authoritative PAL source is `games/rac1/src/overlays/shared/nanotech_ball_and_suck_cannon.c`, with function names and all 19 overlay placements in `config/names.tsv` and `config/overlays/functions.tsv`. `include/hero.h` identifies the ten native actor slots at Hero+0x2050, stored count +0x2078 and incoming count +0x207C. Normal and Gold capacities are five and ten.

| Shared boundary | PAL symbol | NTSC counterpart | Typed signature |
| --- | --- | --- | --- |
| Get per-actor record | `func_L00_002DCD40` | `FUN_L00_002db890` | `u32(u32)` |
| Class grab callback | `func_L00_002DCDA8` | `FUN_L00_002db8f8` | `i32(u32,u32,u32)` |
| Admit suction candidate | `func_L00_002DB508` | `FUN_L00_002da058` | `i32(u32,u32,u32,u32,u32)` |
| Store at muzzle | `func_L00_002DCFD0` | `FUN_L00_002dbb20` | `i32(u32,u32)` |
| Launch original actor | `func_L00_002DD0E0` | `FUN_L00_002dbc30` | `i32(u32,u32,u32,f32)` |
| Cancel suction | `func_L00_002DD2D0` | `FUN_L00_002dbe20` | `i32(u32)` |
| Terminal explosion/removal | `func_L00_002DDDE8` | `FUN_L00_002dc938` | `void(u32,u32,i32)` |

The native per-frame path is `CarriedMobyUpdate`, PAL `func_L00_002DDEA0`; it uses states 3 (incoming), 4 (stored) and 5/6 (projectile). `suckcannon_update`, PAL `func_L00_002DBC40`, removes the actual actor pointer from a slot and calls its class fire handler with the native launch velocity. The finalizer calls the original cannon explosion and then a class cleanup handler or DeleteMoby.

Stock store calls `SetDeathBits`; invoking that for a robot component would incorrectly re-enter death/drop bookkeeping. The component store hook bypasses it. Stock launch has no death/drop bookkeeping and is reused. The private animation table repeats the fragment's current sequence so native carrying/projectile code does not request an enemy animation on the fragment mesh.

## Ownership and transaction

Each offered materialized part gets a private copy of its complete PClass including the variable-length sequence-pointer table, a six-entry cannon handler table and a 0xA0 record. Typical one/two-sequence classes use 0x120 bytes; size-aware blocks grow for larger classes. Shared native class/model resources are never patched. Handler guest addresses are verified with `function_at` against their canonical source functions before class installation. Its original fragment class ID stays unchanged.

`CannonWorldPort::cannon_claim` obtains an exclusive generation-checked `WorldLease` and detaches only NativeWorld's actor-map entry. World identity and collision storage remain alive. Triangle collection, rematerialization, physics and native synchronization skip the leased instance.

The native slot and incoming counter are installed only after the exclusive claim succeeds. At the muzzle, stock `CarriedMobyUpdate` has already acknowledged the destination by moving incoming count to stored count. Only then may `cannon_capture` take the World lease and remove the single world record. The actual native actor and its collider budget remain owned by the cannon adapter until native terminal cleanup. DeleteMoby marks state 0xFD or 0xFE and defers cleanup; after terminal deletion the adapter retires collider budget once but retains the private class block until no native actor references it. Matching dead relay callbacks are absorbed. A recycled managed actor retaining the same class pointer is restored through the compare/replace port before that block can be reused.

The private class provides the native record-getter callback, which reaches the typed getter detour even when ordinary same-file callers inline their lookup. The wrapper uses `handles_record` before strict ownership and returns zero for a stale/dead private class, preventing self-recursion. The getter is cleared on reset while its owned class block is still readable. Class grab uses the exact argument order `(actor, muzzle, cannon)` and performs admission directly, so an inlined outer candidate cannot skip exclusive custody.

A failed capture cannot immediately clear the hero slot: stock code dereferences it immediately after the store callback. The adapter defers rollback until CarriedMobyUpdate returns. It then restores the native body and releases the same world lease. Temporary restore failures are retried without duplicating the record. A missing/recycled native actor never authorizes writes to its replacement.

The ordinary relay must route all `cannon.owns(actor)` callbacks to the adapter before NativeWorld. `update()` returning false for an offered resting part does not relinquish ownership of its relay argument. Reset the cannon before resetting NativeWorld or its guest arena. Actor classes are restored through `cannon_replace_class`, keeping NativeWorld's generation expectations synchronized.

The helper accepts bounded guest RAM and the runtime's documented guest-stack region for native position/velocity arguments. It does not weaken ordinary native-object bounds. No native client file is modified.

## Validation boundary

Focused host tests exercise exclusive custody, Triangle competition, capacity, destination rejection/deferred rollback, cancellation retry, slot loss, same-address/model launch, one terminal explosion, recycled actors, unload and arena exhaustion. These tests emulate native callbacks to check adapter contracts. They are not evidence of rendered native suction, projectile flight or every supported fragment family. Those paths require the isolated PAL gameplay fixture with the exact released DLLs and official runtime hash.

The isolated `pal-salvage-cannon-native-12` fixture qualifies the Novalis Birdbot path on the official Windows PAL runtime `E720A59D130B4B2289A4EB15CAD3E33F739A72A385992DB171EF43490205FB0E`, with salvage DLL `E661DE06545F63824642CEB5931E805B46688D97CFB1B76D986DB3D74B116A76`. Three real Blaster hits killed a placed Birdbot and produced its four original component classes (1736, 1737, 1738 and 1770). All four settled and survived twelve simulation seconds. Real Circle suction stored two components after native acknowledgment, preserving their actor addresses, class IDs and pvars. One real shot then reached the original terminal finalizer, requested its native explosion and removed that same actor once, without entering component death/loot bookkeeping or adding inventory items. The fixture passed 21 native checks, exited with code 0 and left all 107 original inputs byte-identical. Dense projectile screenshots, cancellation, ordinary-enemy co-use and full-capacity native cases are separate validation scopes.

The adapter itself currently uses PAL bindings. The shared target table lists separately mapped NTSC counterparts for the hook broker; that does not establish NTSC gameplay support for this adapter.

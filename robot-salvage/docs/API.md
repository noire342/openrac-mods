# Robot Salvage API v1

`include/robot_salvage_api.h` is an MIT-licensed C interoperability header.
The mod exports `openrac_robot_salvage_query(1)`. Resolve it from the already
loaded Robot Salvage module; query does not load another DLL or start a session.
An unsupported version or inactive service returns null.

All calls and lifecycle operations run on the OpenRAC game thread. Every
structure starts with `size`, initialized by its caller. Validate table size and
version before calling. Outputs are copies: no guest addresses, mesh pointers,
native object pointers or retained caller buffers cross this interface. Strings
are NUL-terminated UTF-8. Numeric part IDs and quantities are full `uint64_t`;
serialize them exactly, not through floating-point JSON numbers.

## Catalog and inventory

- `catalog_count`, `catalog_entry` and `get_part` enumerate copied descriptors.
  `stable_id` identifies the component independently of current locale, native
  address or object lifetime. Registration with `id=0` derives a deterministic
  FNV-1a 64-bit ID; both duplicate numeric and duplicate textual identities are
  rejected. Caller-provided nonzero IDs are accepted if unique.
- `inventory_count` returns row count and revision. `inventory_entry` returns
  part ID, quantity, revision and whether the catalog currently recognizes it.
  An unavailable optional catalog never silently deletes a saved item.
- `apply_changes` adds/removes a batch atomically. A recipe that lacks any
  component consumes nothing. Invalid IDs, zero quantities and integer overflow
  reject the complete batch. Up to 65,536 changes are accepted per call; this is
  a per-call safety bound, not an inventory-capacity limit. Changes to one part
  are aggregated before mutation, allowing add/remove in the same transaction.

No native weapon/item slot is allocated to inventory components. Custom levels
and future crafting systems use this service instead of writing game memory.

## Spawning for custom levels

`register_part` copies a descriptor with localized names, source identity,
model key and physical shape properties. The native adapter owns model
availability and bindings. Sphere shapes use `radius`; elongated parts use the
same radius plus `capsule_half_height`. Mass must be positive. Known native
class references are metadata, not game-memory addresses.

`spawn_part` adds a logical fragment to the active visit and returns a monotonic
instance ID. Position/velocity/rotation/angular velocity are copied world-unit
values. Rotation is XYZ Euler radians, and angular velocity is radians/second.
The native adapter realizes the record using its registered model resources.
The API does not promise that an unavailable foreign model becomes renderable.
`world_count` and `world_entry` provide copied diagnostics and visit identity.

Same-planet respawn retains the visit records. Travel, explicit load and close
end the visit. Neither distance nor age retires a record: the simulation can
suspend distant records and put resting ones to sleep. Collected inventory is
saved separately by the native campaign adapter and follows its save slot.

## Events without stealing another mod's notifications

Each consumer keeps its own sequence cursor and passes it to `read_events`.
Initialize every output event's `size`. A cursor starts at zero for the current
session. The call advances only that cursor after copying its batch. A zero
output capacity is a valid non-consuming query for `newest`.

The preallocated event ring holds 4,096 entries by default. Event publication
does not allocate or throw after an inventory transaction has committed. If a
consumer falls behind, `STALE` is explicit: refresh catalog/inventory snapshots,
then resume with `cursor=newest`. A retained table safely returns `NOT_READY`
after session unload. Foreign-thread calls return `WRONG_THREAD` without
entering owned state.

Kinds are `COLLECTED`, `INVENTORY_CHANGED`, `SPAWNED`, `VISIT_CHANGED`,
`INVENTORY_RESTORED` and `CATALOG_CHANGED`. Native campaign/visit adapters publish
their lifecycle events. `COLLECTED` refers to exactly one committed physical
pickup; transactions from other mods emit `INVENTORY_CHANGED` instead.

## Pure-core boundaries and tests

The catalog, inventory, event ring, world records, collection policy and physics
depend only on C++17. The native adapter supplies collision sweeps, line of
sight, input precedence, rendering and save identity. A swept-sphere contact's
point is its corrected **sphere center**, and `PlayerBody.position` is the
player's foot position. Native pointers/generation tracking stay outside the
core.

The focused core test can be built from `tests/core` independently of OpenRAC.
Its passing host result does not assert native gameplay or full-campaign
coverage; those need the native adapter's own scenario receipts.

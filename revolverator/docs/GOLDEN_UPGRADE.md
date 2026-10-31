# Golden upgrade and Novalis offer

## Current 0.3.1 shop presentation

The OpenRAC 0.10 native fixtures verify all eleven live Novalis cases, the new
display's size/centre/lighting, a real Triangle purchase from 3.35 m and the
complete Gold acquisition animation, including without Weapon XP. The fixed
display is 2.1 times the held weapon's native scale, about 2.22 m across its full
longest span. Its whole GLB bounds are centred above the case at the original
Gold displays' median height, 1.775 m; placement is not anchored to the hand grip.
Yaw is fixed at -pi/2 and the horizontal interaction radius is 3.5 m.
See [0.10 evidence and scope](OPENRAC_0_10.md); older 0.9 receipts remain historical.

The Gold Revolverator is an upgrade of the existing logical weapon
`0x10012` (`noire342.revolverator`). The Italian name is **Rotatore d'oro**.
`State.golden` is independent of native Walloper/Gold ownership. The purchase
does not add a descriptor, wheel entry, favourite binding or separate ammo/XP
pool. Its price is **50,000 ordinary bolts and zero Gold Bolts**. Ownership of
the base weapon is required.

`Registry::purchase_golden(key, balance, price)` performs the pure transaction.
It returns an explicit result, fails without mutation for an unknown/native
key, missing base, invalid price or insufficient balance, and is idempotent
when already purchased. A successful transaction changes only the custom
Golden bit, revision and caller-owned ordinary-bolt balance.

## Persistence

New `.arsenal` writes use binary codec2. The byte formerly reserved in each
record now stores canonical `golden=0/1`; offsets and record lengths remain
unchanged. Codec1 is accepted with the original reserved-byte invariant and
loads `golden=false`. Other flag values and a Golden-but-unowned record are
rejected atomically. The logical snapshot format remains1; it is separate
from the binary codec version.

The existing CRC, native-save-prefix fingerprint, atomic replacement and
unknown-record retention remain in place. An absent mod can retain the new
flag and reactivate it when its descriptor is reinstalled. Old software that
only understands codec1 rejects codec2 rather than silently applying it.
Native save bytes are never rewritten by the sidecar implementation.

Native Gold state is `item_text_variant` at canonical0x13E520. The ordinary
availability/story table0x13D388 is unrelated. The native available Gold Bolt
count is `clamp(collected - 4 * nativeGoldPurchased, 0, 40)`; purchasing this
upgrade changes none of those flags, collection bits or counters.

## Native shop adapter

`NativeGoldVendor(api, mod, registry, weapon_alias)` owns one additional native
class304 case and one dynamic native304 display. Its Novalis position extends
the existing Gold-room ring at `(250.8, 123.6, 54.4966202)`. It appears only in
level1 with the same challenge-mode access gate used by the native Gold shop
(canonical0x15EE20 greater than zero). Existing room access remains native.

The preview is allocated as native class304, keeps its304 resource and only
then receives renderer identity2019. The separately bundled2019 GLB is a
static, unskinned Gold-weapon display with no joints. Its current instance
scale is `0.1458333283662796 * 2.1`, from `gold_shop_layout.hpp`. Only the unused2019
class-map byte is leased to the native304 slot after validating the reverse
ID and class resource. A foreign mapping is never stolen, and a later foreign
change is preserved on reset. The preview is restored304 before removal.
Neither actor requires class180 or its potentially streamed held-weapon buffer.
The constructor retains the `weapon_alias` parameter for Session source
compatibility but no longer depends on that adapter.

The original Gold-case update would index one of ten native offers and write
its Gold flag. This adapter intercepts only its owned actor, using the existing
SDK hook `rac1.moby.item_offer.update`, signature `void(u32)`. Foreign actors
continue through the original chain. Own cases remain in safe native idle3.
No guest ownership tag is written. Identity stays in a host token containing
the actor address, class resource, pvars pointer, captured +0x38 word and level.
The actor must also remain in its owned idle state3; native reinitialization
clears that state even when the new instance has the same zero stamp. Native
ambient colour bytes at+0x3C are excluded from identity. The native parent pointer
at+0xB8 is untouched: bolt-source code walks that chain until null, so using a
self-parent ownership tag would introduce a cycle.

Nearby interaction uses the original NPC/Gold prompt channel12, native
init/fade/draw callbacks and ordinary native Triangolo/Triangle input. The
prompt is written with a 128-byte bound to the overlay array at canonical
0x17E530. The unrelated resident shared-text buffer0x19A440 is preserved.
There is no added panel or copied NPC
dialogue record. This avoids the placed-NPC lookup's invalid index for dynamic
actors and the native four-Gold-Bolt predicate.

The case and display inherit illumination from their nearest original shop
counterparts: light sets/cross-fade at +0x38, ambient at +0x3C, shine at +0x73,
glow at +0x90 and the 0x10 material-lighting flag. The host ownership word is
captured after this inheritance. Field +0xB0 remains a mission/save index and
is not used as lighting data.

On successful purchase, the adapter commits the ordinary balance, retires
only its preview, requests the existing native autosave and exposes one
`poll_purchase()` event for the owner's acquisition animation. A sold case
stays in the room without its displayed weapon. The native Gold tables and
original ten offers remain unchanged.

## Session integration

1. Register the item-offer SDK hook at load.
2. Call `gold_vendor.frame()` after the normal level state is ready.
3. For each offer callback: `if (!gold_vendor.update(moby)) original(moby);`.
4. `preview_moby()` exposes the owned static display actor. It needs no rotor
   pose, skin or native held-Walloper update. `update` also claims this actor
   without running a sale or the original Gold-case callback.
5. Consume `poll_purchase()` once to queue `NativeAcquisition::request()` and
   the purchase notification. Name, material and gameplay state refresh from
   the persisted upgrade. Its autosave is already requested by the adapter.
6. Call `gold_vendor.reset()` before level-memory release; only its still-owned
   mapping byte and actors are retired.

The existing base registry/store tests and new `golden_upgrade_test.cpp` and
`native_gold_vendor_test.cpp` cover transactions, migration, native-state
isolation, callback ownership, bounded prompt buffers and unavailable preview
sources. The native channel12 caption is the128-byte overlay array at
canonical0x17E530; the separate resident shared-text buffer is preserved.
The middle fade callback can have a different named hook thunk from its
guest-address entry. Both resolve; pointer equality is required only for
the non-hooked initialization/draw endpoints on the verified client.

Integrated native scenarios and their limits are recorded in
`GOLDEN_VALIDATION.md`. They include the actual room, all eleven live cases,
purchase, Gold materials and a fresh-process native save reload.

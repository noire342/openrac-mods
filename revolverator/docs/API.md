# Revolverator native API v1

The C header `include/revolverator_api.h` is the authoritative ABI. Resolve the
symbol **`openrac_revolverator_query`** from the already loaded Revolverator
library, then request version **1**. Unsupported versions, an absent provider or
an unloaded session are unavailable; the query returns null. Do not load a second
copy of the mod just to obtain its service.

This is an in-process, game-thread API. Call it from a consumer's OpenRAC frame
callback, using the same architecture and C ABI as the provider. It exchanges
copied values and exposes no guest RAM addresses. Never call from a worker
thread, serialize a service pointer, retain it across provider unload, or call
through a stale function pointer. The current ABI has no unload event; consumers
must honor the provider lifecycle and clear their cached service on unload.

The mod manifest can declare the required provider:

```json
"requires": [{"id": "revolverator", "version": ">=0.1.0"}]
```

For optional integrations, use a load-order hint and handle an unavailable
provider without failing your mod. The native loader's `api->function` resolves
game functions; it is not the discovery mechanism for this custom DLL symbol.
On Windows, `GetModuleHandleW(L"revolverator.dll")` and `GetProcAddress` obtain
the already loaded service. Other platforms must resolve it through the loaded
provider's dynamic-loader handle. The provider's Linux ELF export map must
explicitly include this symbol; see `PACKAGING.md`.

## ABI size checks

The service starts with `size` and `version`. Verify the requested version and
the last required member's offset before reading that member. The optional
`get_mechanics` function was appended to v1: requiring `sizeof(revolverator_api_v1)`
would unnecessarily reject an earlier provider that still implements
`get_state` and `cancel`.

Initialize output structures to zero and set their `size` to `sizeof` before
each call. A getter returns **1** on success and **0** for an unavailable session,
null output or undersized output. It writes only the structure it implements
and reports that structure's size. Do not assume a field was updated after a
failed call.

This plain-C client demonstrates the required prefix check and the optional
tail check. `query` is obtained from the loaded provider, not linked statically:

```c
#include <stddef.h>
#include <stdio.h>
#include "revolverator_api.h"

#define REV_HAS(service, member) \
    ((service)->size >= offsetof(revolverator_api_v1, member) + \
                        sizeof((service)->member))

static void inspect_revolverator(revolverator_query_fn query, int request_cancel)
{
    const revolverator_api_v1* service = query ? query(1u) : NULL;
    if (!service || service->version != 1u || !REV_HAS(service, cancel) ||
        !service->get_state || !service->cancel)
        return;

    revolverator_state_v1 state = {0};
    state.size = sizeof state;
    if (service->get_state(&state)) {
        printf("item=%u phase=%u charge=%.2f captures=%llu\n",
            state.item_id, state.phase, (double)state.charge,
            (unsigned long long)state.captures);
    }

    /* Read the appended pointer only after checking that its bytes exist. */
    if (REV_HAS(service, get_mechanics) && service->get_mechanics) {
        revolverator_mechanics_v1 mechanics = {0};
        mechanics.size = sizeof mechanics;
        if (service->get_mechanics(&mechanics)) {
            printf("ammo=%u/%u rotor=%.2f rad, %.2f rad/s dry=%u sync=%u\n",
                mechanics.ammunition, mechanics.capacity,
                (double)mechanics.rotor_angle, (double)mechanics.rotor_speed,
                mechanics.dry_firing, mechanics.synchronized);
        }
    }

    if (request_cancel)
        (void)service->cancel();
}
```

The compilable counterpart is `examples/state_client.c`. It has no link-time
dependency on the provider and does not resolve or load libraries itself.

## State and actions

`get_state` copies the controller snapshot:

| Field | Meaning |
| --- | --- |
| `phase` | `REV_IDLE`, `REV_WINDUP`, `REV_SPINNING`, `REV_FLYING`, `REV_RECOVERY` |
| `item_id` | Native inventory item18, the replaced Walloper slot |
| `target_class` | Captured enemy's native class; zero without a target |
| `charge` | Normalized0..1 while spinning; retained throw charge during flight |
| `captures`, `throws`, `impacts` | Controller counters for this loaded mod session |

An impact counter means the controller dispatched its impact path. It is not
proof of damage acceptance, enemy death, bolt reward, or an XP award. The state
does not report ownership, shop unlock status, equipped state or save progress.

`get_mechanics`, when present, copies the ammunition count, effective capacity,
rotor angle in radians and speed in radians per second. `dry_firing` and
`synchronized` are boolean values encoded as0/1. Synchronization means the motor
is following the held enemy's authoritative spin angle. The angle is a current
rendering state, not a persistent progression value.

`cancel()` returns1 when a session exists and0 otherwise. It releases an active
capture, restores its leased native collision/AI state where the same entity
still exists, cancels an in-flight throw, and returns the controller to idle.
It does not refund ammunition, erase counters, remove the weapon, or alter XP.
The input latch requires releasing the attack button before another capture.
It is safe to call when already idle. A consumer must not call cancel merely
to inspect state; cancellation intentionally changes gameplay.

## Weapon XP compatibility

The mods remain separate services. Revolverator's manifest loads it after
`weapon-xp` when that provider is present, and it also works without Weapon XP.
The current restoration reuses **item18**, so XP belongs to the existing
Walloper/item18 record, including previously earned progress. It does not create
a thirteenth XP weapon or a second, independent Revolverator save record.

Revolverator posts native hit records using the native weapon class180. Weapon
XP maps that source class to item18 and can credit confirmed native deaths using
its ordinary bolt-based reward policy. Do not add `award_xp` on every capture,
throw or impact: those are not kills, and a separate award would bypass or
double-count the normal combat attribution. The Revolverator API intentionally
contains no direct XP mutation function.

Weapon XP damage/capacity scaling remains governed by its own settings, both
off by default. `get_mechanics.capacity` reports the live native capacity rather
than assuming10; an enabled external capacity policy may change it. XP HUD
decoration, versioned names and level-up animation require testing with both
mods enabled because the providers share native text and HUD resources. Load
ordering is a declaration of intent, not evidence that every configuration has
passed native gameplay QA. Revolverator's mechanics snapshots do not certify
those integration cases.

Future ABI additions must preserve these member offsets. Append an explicitly
optional tail or introduce a new query version; never repurpose an existing
field or return private C++ objects across the interface.

# Add your first Weapon Experience hook

Use this guide to add a level-based name or effect from a native OpenRAC mod.
The example gives the level-10 Pyrocitor an alternate name and supplies the
hit position to your effect renderer.

## 1. Declare the dependency

Add this field to your mod's existing `openrac-mod.json` if the feature
requires Weapon Experience 0.7.0 or later. OpenRAC loads the required mod first.

```json
{
  "requires": [{"id": "weapon-xp", "version": ">=0.7.0"}]
}
```

For an optional feature, use `"load": {"after": ["weapon-xp"]}` and let the
rest of your mod run when Weapon Experience is unavailable.

## 2. Connect the example to your mod

Copy the MIT [public header](../include/weapon_xp_api.h) and both example files
([C source](../examples/level_effect.c) and [header](../examples/level_effect.h))
into your project. Compile `level_effect.c` with your mod and make both headers
available to the compiler. The example is a component of your mod.

On Windows, obtain the query function from the module OpenRAC already loaded.
On other platforms use the lookup facilities for an existing module handle and
resolve the same `WEAPON_XP_QUERY_SYMBOL`.

```c
#include <windows.h>
#include "level_effect.h"

static blarg_effect_example blarg;

static int32_t attach_blarg(void) {
    HMODULE module = GetModuleHandleW(L"weapon_xp.dll");
    weapon_xp_query_fn query = module
        ? (weapon_xp_query_fn)(void*)GetProcAddress(module, WEAPON_XP_QUERY_SYMBOL)
        : NULL;
    return blarg_effect_attach(&blarg, query);
}
```

- **Normal game frame.** Call `attach_blarg()` to register. After OK, call
  `blarg_effect_tick(&blarg, spawn_my_effect)` on each game frame, supplying
  a function that takes `const float position[3]`. The callback queues a
  copy of the position for this tick function to use.
- **Startup.** Retry NOT_READY on a later frame while the mod is loading.
  NOT_FOUND means the discovered module does not expose the full v5 table.
  An optional consumer can keep the feature disabled in that case.
- **Unload.** Call `blarg_effect_detach(&blarg)` before your mod unloads.
  The example listens for STOPPING and clears its provider reference when
  Weapon Experience shuts down first.

To try a name on another weapon or level, edit `blarg_name` in the example.
`versioned_names` must be enabled for it to take effect. The sample uses
item ID 16 for Pyrocitor and the game's language ID 5 for Italian.

## 3. Choose the hook you need

| Goal | Use |
| --- | --- |
| Read the level when your weapon fires | `get_progress` in your own fire hook |
| Queue an effect on a native hit or level-up | `subscribe_filtered` |
| Replace a title by level and language | `register_name_provider` |
| Apply the shared XP rules to your own weapon | `get_native_reward` and `evaluate_progress` |

For a listener that needs hits and STOPPING, subscribe with
`WEAPON_XP_ANY_ITEM` and check the weapon ID inside the hit branch. An exact
weapon filter excludes session events. Use `WEAPON_XP_SESSION_ITEM` for a
separate lifecycle listener if your mod already owns a weapon-specific
subscription.

See the [API reference](API.md#level-effects-and-names-v5) for callback
lifetime, error results and the distinction between outgoing hit
observations and confirmed damage.

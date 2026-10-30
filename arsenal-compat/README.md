# Arsenal Compatibility 0.1.0 — private bundled utility

Compatibility provider for the additive Revolverator on the **unchanged stock
OpenRAC Windows x64 client**, R&C1 NTSC-U. The executable on disk is neither
replaced nor edited. The provider installs its owned in-memory detours only
after a consumer has prepared all 30 typed bindings and explicitly committed.
It does not supply weapons, visuals, game data or save policy.

Supported executable SHA256:
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
Other builds fail with a descriptive error before hook creation. PAL and other
platforms are not supported by this native provider.

## Loading and ownership

The native Revolverator package loads this utility privately; no additional
launcher mod card is required. The standalone SDK wrapper remains available
for development tests. Weapon XP, when used, loads before Revolverator.
The provider preflights all 30 names through the stock SDK's `api->function`.
Every target must be distinct executable `MEM_IMAGE` code inside the main module,
checked with `VirtualQuery`; there are no hardcoded host addresses or offsets.
The exact IDs, signatures and NTSC symbols are in [docs/hooks.json](docs/hooks.json).

The C ABI is [include/arsenal_hooks_api.h](include/arsenal_hooks_api.h), queried
through `openrac_arsenal_hooks_query(1)` from the already loaded module:

1. Check table size/version and `claim("consumer-id")`. A second claim is refused.
2. Use pure `owns_id(id)` discovery; pass ordinary SDK hook IDs to `api->hook`.
3. `bind(id, signature, replacement, &original)` creates a disabled detour and
   supplies the callable trampoline. Preserve exact integer signedness/results.
4. `commit()` requires all 30 bindings and activates one MinHook queued batch.
5. Call `rollback()` on any initialization failure and before consumer unload.
   Read `diagnostics(out)` for stage, counts, backend status, hook ID and message.

All API calls run on the game/load thread. Foreign threads and reentrant
transactions are refused. Unknown IDs return `ARSENAL_HOOKS_UNKNOWN` without
mutating the transaction or diagnostic state. Binding is single-consumer;
it is not a general multi-mod chaining service.

Rollback disables/removes only targets created by this provider. It never calls
`MH_ALL_HOOKS` or `MH_Uninitialize`. Each detour retains the consumer module until
its trampoline is safely removed. If teardown fails, diagnostics explicitly
require retaining the consumer; provider unload pins itself and remaining
consumer modules rather than leave dangling detours.

## Save wrapper semantics

`rac1.save.restore` resolves stock `FUN_00209298` and `rac1.save.new_game`
resolves stock `FUN_00209370`. These hooks surround the **whole stock host
wrapper** and continue through a MinHook trampoline. They do not insert inside
the engine's native implementation. Consumer scopes must tolerate the wrapper's
own default-state/load behavior. Native save/new-game tests must check this
boundary explicitly; a successful lookup or host test does not prove it.

## Building and tests

```sh
cmake -S . -B build -DOpenRACMod_DIR=/path/to/openrac-mod-sdk-1.3.0/cmake -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Windows x64 builds `package/arsenal_compat`, including the plugin, manifest and
license notices. On Linux only the portable core/tests are built; no unsupported
native plugin is produced. The fake backend covers all 30 missing/unsafe target,
create and enable-queue failure positions, partial activation, cleanup failure
and retry, signature/duplicate checks, foreign-hook preservation, reentry and
thread checks. The current suite passes 5,158 checks on Windows and Linux.
These are host contracts. Native stock-client gameplay, save wrappers and full
mod integration require separate evidence from the consuming mod's exact build.

## Licensing and dependency provenance

Provider implementation: [GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342.
The interoperability header is explicitly MIT; its complete text is in
[docs/LICENSE-MIT.md](docs/LICENSE-MIT.md). The OpenRAC SDK retains its MIT notice.

[MinHook](https://github.com/TsudaKageyu/minhook) is pinned to upstream v1.3.4,
commit `c3fcafdc10146beb5919319d0683e44e3c30d537`. The vendored source is unmodified;
[third_party/minhook.lock.json](third_party/minhook.lock.json) records the exact
archive URL/hash and selected files. MinHook and its bundled HDE code retain
their original BSD two-clause notices in
[third_party/minhook/LICENSE.txt](third_party/minhook/LICENSE.txt), also copied
into the binary package. No game assets or client executable are included.

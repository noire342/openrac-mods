# Mod APIs

## Weapon Experience

The authoritative interface is the MIT-licensed
[`weapon_xp_api.h`](../weapon-xp/include/weapon_xp_api.h). Full contracts are in
[`weapon-xp/docs/API.md`](../weapon-xp/docs/API.md).

Resolve `openrac_weapon_xp_query` from the provider that OpenRAC already loaded.
Do not load a second DLL. Query ABI 1, 2 or 3 explicitly, check the returned
version and table size, and initialize the `size` of every input/output structure.
All calls and callbacks run on the game thread.

| ABI | Purpose |
|---|---|
| 1 | Weapon enumeration, copied progression stats, explicit XP awards and events. |
| 2 | Idempotent co-op hit/death receipts with session provenance. It is not a network transport. |
| 3 | Copied rules/settings, atomic revision-checked edits, rules reload, diagnostics and explicit XP flush. |

v3 preserves the existing v1/v2 prefixes. Optional damage and capacity bonuses
are off by default. Configuration edits are session-only and preserve earned XP.
Never retain a service table after provider shutdown; unsubscribe on unload.

Examples are compiled as part of the tests:

- [`rules_client.c`](../weapon-xp/examples/rules_client.c): minimal C client for
  revision-checked configuration.
- [`xp_observer.c`](../weapon-xp/examples/xp_observer.c): provider discovery and
  event observation on Windows.

Version 0.6.1 adds the independent Morph-o-Ray model-pose flag to ABI v3. It
defaults off and does not add that weapon to the XP catalog.

## Revolverator preview

The current design provides `openrac_revolverator_query` and a versioned,
copy-only C table. It exposes the weapon phase, charge and counters plus explicit
cancellation. Guest pointers and private C++ objects do not cross this boundary.

The preview API remains under integration. Consumers must use the exact header
shipped with the eventual preview and check version/size; this document does not
promise unreleased function-table fields. Like the XP service, it is game-thread
only and must not outlive the provider.

## Composition

Load-order declarations and version requirements belong in the consumer's
`openrac-mod.json`. Do not patch another mod's private state. Restore an override
only while the value/revision still belongs to the consumer; another mod may have
legitimately changed it. A successful host test verifies an adapter contract,
not native gameplay or all-mod compatibility.

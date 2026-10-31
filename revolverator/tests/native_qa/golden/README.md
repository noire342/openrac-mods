# Isolated native Golden QA helper

This test plugin is not part of the player ZIP. Build it against SDK1.3 on
Windows and load it after the Revolverator0.3 and optional XP0.6.3-arsenal
plugins. It uses the copy-only public API3 and ordinary native hooks.

Use only an isolated fixture with its own `--data`, `--plugins`, `--settings`
and empty/copied `--cards` directories. It seeds ownership, ordinary currency,
NG+ access and positions. Never run it on a normal gameplay profile. It does
not seed Golden ownership, health, rewards or XP. Native Gold purchase and
controller input still perform the actual transaction and combat.

The string setting `mode` accepts `shop`, `combatshort`, `combatlong`,
`switch-flight`, `save` or `reload`. `require_xp=false` permits the standalone
shop scenario. `deadline` is a bound on all frame callbacks, including loading.
Save uses slot0 on the copied card; reload must run in a fresh process with
another copy of that completed card. It begins without custom ownership.

Use the untouched official0.9.0 NTSC-U executable, original installed levels,
`--skip-movies --level1 --window --frames1800`, and `--no-card` for the four
non-persistence scenarios. Expand those option/value pairs as separate CLI
arguments. Leave the user's actual card/settings outside the fixture.

The helper emits `GOLDEN_QA` JSON events for guarded setup, native purchase,
accepted hits/deaths, movement holds and final assertions. Actor identity,
native Gold flags and ordinary currency are checked separately. Runtime logs
record the exact Golden charge/radius and mod artifact hashes belong in the
fixture receipt. Observe the process exit code independently; an absent result
or an unobserved exit is not a pass.

The native seed and helper hash are part of the evidence. This helper does not
prove the full campaign, every boss phase, other discs or all mod combinations.

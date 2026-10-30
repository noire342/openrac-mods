# Validation

## Weapon Experience 0.6.1

The initial release uses the already verified Windows x86-64 artifact, rather
than rebuilding an untested binary at publication time.

| Evidence | Scope |
|---|---|
| 12 automated test groups | Clean source-archive rebuild; zero reported compiler warnings. |
| Native Windows NTSC-U sessions | Weighted XP, a real kill crossing the V9→V10 boundary, native presentation, level-10 HUD and Morph-o-Ray pose toggling. |
| Public ABI checks | Provider reports 0.6.1; v1/v2/v3 tables and revisioned configuration. |
| Default-policy checks | Damage/capacity bonuses and the independent model flip are off by default. |

The original test fixtures sometimes seed inventory or XP to reach a boundary.
Seeded progress is not evidence of naturally earning that progress. The final
boundary test includes a real attributed kill after setup.

DLL SHA-256:

```text
bb98d0af780e9b948d306776363eb3a0fff3c21d662718470ec90f742192e80c
```

Windows archive SHA-256:

```text
e0b783ca6d12172b7bea2df43f08cf729db307d48ff548fcb1d445da909ea4bb
```

The machine-readable [release receipt](validation/weapon-xp-0.6.1.json) contains
public, sanitized evidence only. Private file paths, save data and RAM dumps are
not included.

## Limits

- Native PAL gameplay has not been validated; current PAL evidence is host-side.
- No complete campaign playthrough is claimed.
- Enabling optional bonuses with another damage/capacity mod needs separate
  composition testing.
- API configuration edits are session-only; there is no sandboxed Luau bridge.
- An attribution to original concept art for the optional Morph-o-Ray pose has
  not been independently confirmed.

## Revolverator source preview

Version 0.1.0 is published as source, local asset recipes and developer tools.
There is no public Revolverator binary or game-derived asset bundle. The
[sanitized preview receipt](validation/revolverator-0.1.0-source-preview.json)
records host checks and native observations without including saves, RAM,
original art or personal paths.
The [detailed native record](../revolverator/docs/NATIVE_VALIDATION.md) preserves
the observed setup and separates assigned API XP from earned combat XP.

| Evidence | Verified scope |
| --- | --- |
| Ten host test groups on Windows and Linux | Controller, contact/pose, ammo, vendor, motor, guest arena, localization, native rotor, class alias and HD icon. |
| C API and ELF checks | C client builds; Linux exposes the three loader entry points and `openrac_revolverator_query`; service is unavailable before session load. |
| Native Windows NTSC-U input | A quick tap produces zero captures/throws; continuous capture hold requires at least 0.18 seconds. |
| Pose and motor | Upward shaft alignment approximately 0.99999; idle tip speed 1 rad/s; capture synchronization follows the enemy at 8..18 rad/s; empty-ammo jerk returns to its starting angle. |
| Native ammunition/crates | Capacity 10, one charge per activation, pickup 2 capped at capacity, unit price 25; native crate breaking observed. |
| Vendor | Not sold before visiting Hoven; normal Hoven introduction at 15,000 bolts. |
| Explicit cancellation | Cancelling a held enemy returns to idle with zero throws/impacts in that check. |
| Weapon XP coexistence | A native attributed kill earned 39 real item-18 XP using the locally installed development provider 0.6.2. |
| Versioned name | A separate API-seeded 3,500-XP test displayed `Revolverator v4`; this is not counted as earned progression. |
| Level smoke tests | All gameplay levels 0..18 load; Orxon/Clank mode blocks ordinary weapons as expected. These are load checks, not full level playthroughs. |

Native observations used a private Windows build with SHA-256
`ff3b5b109a65ef9ad786f4f6bb819b130c7e77887589cf4ea55d3296db40cb29`.
That DLL and its locally derived art are not published here. Source builds in CI
use the native 32px glyph and no game assets.

The Weapon XP provider used in the composition check is newer than this
repository's published 0.6.1 artifact. This preview publication does not copy or
release that local 0.6.2 source; the existing public XP source and release remain
unchanged. Do not infer every settings combination from that one composition
check.

Known preview limitations are explicit: Rilgar uses the native 32px fallback in
private HD builds when presentation memory is unavailable, and Blarg Station
can lack custom cartridge pickup notifications when its text bank cannot fit.
Gameplay and native ammunition handling remain functional in those checks.

PAL, full campaign completion, boss-fight coverage, every capturable enemy,
all save/reload paths and all combinations of other mods are not claimed. The
[regression checklist](../revolverator/docs/QA_CHECKLIST.md) remains the broader
set of cases for future previews.

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

## Revolverator

Development tests and native test instances exist, but a finished release gate
has not passed. Do not infer complete gameplay, vendor, ammunition or save
coverage from a host test or a screenshot. Its release receipt will be added
only when the exact DLL and asset setup have been verified together.

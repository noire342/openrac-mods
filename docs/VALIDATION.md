# Validation

## Current preview source: Revolverator 0.2 and XP 0.6.3-arsenal

This branch contains the additive implementation. The original Walloper remains
independent. The [detailed native record](../revolverator/docs/NATIVE_VALIDATION.md)
keeps fixture setup, exact private binary hashes and limits separate from host tests.
The [current source receipt](validation/revolverator-0.2.0-preview-source.json)
records 30/30 passing groups on both Windows and Linux, plus the ELF service check.

| Evidence | Scope |
|---|---|
|17 Revolverator host groups | Controller/pose, ammo/vendor, logical inventory, CRC/fingerprint sidecars, menus, aliases, optional XP and presentation/fallback contracts |
|13 Weapon Experience host groups | Rules, persistence, hit ownership, native adapters, acquisition lifecycle and DLL/service contracts |
| Native combat XP | Revolverator earns 0→39 from an attributed Birdbot kill while Walloper stays 0; a later native Walloper kill earns its own 0→39 |
| Native commerce/pickup capacity 20 | Hoven 15,000→20 rounds; refill 2→20 costs 450; Walloper 7,500 independent; pickups 4→6 and 19→20, then remain 20 |
| Native Novalis menus |16 entries in native 4×5 grids, distinct favourites, assembled rotating previews and cartridge HUD |
| Limited-memory native HUD | Rilgar 5/Nebula 6/Quartu 15 complete 2,400-frame standalone runs; exact authored 32px cartridge texels/CLUT, displayed 19/20; compact/native glyph fallback on 5/6 |
| Native saves | Independent custom sidecars, successful save/load and duplicate autosave behavior in the isolated fixtures described by the detailed record |

The limited-memory runs used DLL SHA256
`62cbd609ec86b2088888eef4c55b901e17b3107fa90f89e615b1816047b312cd`
and runtime SHA256
`fc717cd0526ca1977895f3baf8e86186adcc5bfd41d0d75b5622148f0e689f9b`.
The weapon glyph was 32px on Rilgar/Nebula and 128px on Quartu; remaining HUD heap
was 3,456/624/12,304 bytes. These three runs did not open pause/vendor menus or
load Weapon Experience. They are not whole-level or campaign completion.

Native graphics observations use locally generated original drill art. Public
source builds use the player's native glyph and included authored cartridge
data; original art and private HD headers are excluded from Git and CI output.
Source build checks are not native Linux/PAL gameplay proof. Preview release
artifacts need their own matching validation receipt.

## Published stable Weapon Experience 0.6.1 — historical release

The published 0.6.1 Windows artifact and archives remain unchanged. Its
[release receipt](validation/weapon-xp-0.6.1.json) records 12 automated groups,
native weighted XP and V9→V10 progression, acquisition/HUD behavior and the
optional Morph-o-Ray pose. Its APIs are v1/v2/v3, not the development companion's
required v4 ownership contract.

DLL SHA256: `bb98d0af780e9b948d306776363eb3a0fff3c21d662718470ec90f742192e80c`.
Windows ZIP SHA256: `e0b783ca6d12172b7bea2df43f08cf729db307d48ff548fcb1d445da909ea4bb`.
The existing release is not re-certified by changing development source.

## Revolverator 0.1.0 — historical source preview

The [sanitized 0.1 receipt](validation/revolverator-0.1.0-source-preview.json)
records that earlier implementation's tests and limits. It replaced Walloper 18,
used capacity 10 and shared item 18 XP. Those statements describe 0.1 history only;
they are not the additive 0.2 design. Historical level 0..18 load checks establish
loading, not completed gameplay or 0.2 coverage.

## Remaining limits

No complete campaign, every boss/capture target, every save interruption, every
video format or all-mod combination is certified. Native PAL/macOS/Linux gameplay
has not been verified by host tests. Keep API-seeded inventory/XP separate from
real earned progression, and consult the [regression checklist](../revolverator/docs/QA_CHECKLIST.md).

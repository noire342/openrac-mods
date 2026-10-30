# Validation

## Standalone Skill Points and Cinematic Journeys

- [Skill Points 0.1.2 receipt](validation/skill-points-ra-0.1.2.json): five host
  groups; Windows NTSC-U native load, normal initial pause route, controlled
  Pyrocitor unlock, optional Kerwan challenge both on/off and XP coexistence.
  The 19-level UI smoke and pause-reopening records are explicitly from 0.1.1;
  no claim that all 178 achievements were manually completed.
- [Cinematic Journeys 0.1.0 receipt](validation/cinematic-journeys-0.1.0.json):
  two host groups, all 65,536 button combinations, PAL/NTSC adapter tests, native
  NTSC-U protected movie Start, full-chord skip and default five-second flight.
  The ten-second fixture preceded the last ownership refinement. Flights were
  requested through a native transition fixture, not manual map navigation;
  PAL and every campaign scene were not tested in-game.

These are the original binary-specific native receipts. Publication adds clean
source build/CI checks; it does not turn host tests into additional gameplay
proof or certify every combination with the Revolverator preview.
The combined clean Windows source build passes **41/41 host groups**; see the
[publication source receipt](validation/skill-points-cinematics-publication.json).

## Current self-contained native package

The [native receipt](validation/revolverator-0.2.0-native-package.json) records
six new scenarios on the unchanged official Windows NTSC-U client. The weapon
package discovers/stages its own assets and loads utility DLLs privately.
Original levels are used; no custom `--levels`, external asset pack, replacement
client, utility mod cards or preparation launcher script is required.

| Current native scenario | Observed scope |
|---|---|
| Smoke, 900 frames | Only Revolverator and optional XP as production mod entries; no QA helper, 20 starting rounds, control at 521 |
| Vendor/pickups, 2×2,400 frames | 24 commerce checks; Hoven price/refill, independent Walloper, ordinary pickup cap 20 |
| Earned XP, 1,800 frames | Actual capture/throw/impact/death; Revo 0→39 while Walloper unchanged, then native Walloper 0→39 separately; no XP seeding |
| Save/reload, 2,200 frames | Real isolated card: snapshot 7, RAM 3 while pending, reload 7; sidecar CRC/native fingerprint validated, original Walloper remains unowned |
| Menus, 2,400 frames | 16 weapons in native 4×5 grid, quick keys 18/65554 distinct; drill/cartridge preview and HUD inspected, phase 18 completed |

The smoke has no instrumentation helper. Other targeted fixtures list their QA
probe modules in the receipt; those probes are not production dependencies.
Novalis loads 345 models, and 61 original inputs including the client remain
unchanged. The current exact hashes are:

- Revolverator: `ebe140838d24ebb4707b74fc06ad94a935c9ab135c885ad87a78d9f2081fdef5`.
- Private hook utility: `077a7306b71ac5f35776b74c261cceb1ff0c1144961a847696eabc961c71d55a`.
- Private file alias utility: `aec87cbfd6eb3133dbb51e62edbce7fae46081355dcd4b5b5aa458876860ca14`.
- Optional XP: `68205f76fea997336d4ee01fcb7c2812a2fdd32b6d79d3b67cc51a8b162cb67c`.
- Official EXE: `b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.

The previously published Revolverator/XP source subset passes 34/34 groups on Windows and Linux:
19 Revolverator, 13 XP, 1 hook provider, 1 file-overlay group. The new staging group
checks serialization, argument precedence, source immutability and owned cache
cleanup; file-overlay policy/parser tests add 1,865 checks. The separate hook
provider has 5,158 checks. [Source evidence](validation/revolverator-0.2.0-native-package-source.json)
and native scenarios are distinct. Neither establishes complete campaigns,
all bosses/mods, NG+, every save slot, PAL or native Linux/macOS gameplay.

---

## Previous stock-client manual/helper setup — historical

The current path uses the unchanged official Windows x64 NTSC-U client,
commit `77a2f052`, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`,
with Arsenal Compatibility 0.1.0. The [current source receipt](validation/revolverator-0.2.0-stock-preview-source.json)
records **32/32 host groups passing on Windows and Linux**, and the ELF service check.

| Current evidence | Scope |
|---|---|
|18 Revolverator host groups | Existing weapon contracts plus the NativeRuntimeHooks consumer adapter, SDK delegation and rollback |
|13 Weapon Experience host groups | Rules, persistence, hit ownership, native adapters and acquisition/service contracts |
|1 compatibility provider host group |5,158 checks covering 30 targets, exact-build rejection, signatures, transaction failures, rollback, reentry and foreign-hook preservation |
| Stock native pass-through | Provider and test consumer prepare/activate 30 detours, complete 1,800 frames; observed NewGame 1, Restore 2, RestoreData 42, UpdateState 1,056 |

The [exact native preview receipt](validation/revolverator-0.2.0-stock-native.json)
records six additional stock-client scenarios with the real Revolverator:

| Native scenario | Observed scope |
|---|---|
| Trial, 720 frames | Included launcher, no helper plugins, isolated Novalis; 20 starting rounds |
| Vendor and pickups, 2×2,400 frames | 24 commerce checks, Hoven purchase/refill, original Walloper ownership independent, normal pickup cap 20 |
| Earned XP, 1,800 frames | Actual capture/throw/impact/death; Revo earns 39 while Walloper stays unchanged, then native Walloper earns its own 39; no XP seeding |
| Save/reload, 2,200 frames | Real isolated card copy; snapshot 7 rounds, changed to 3 while pending, reload restores 7; sidecar CRC/native fingerprint pass; original Walloper remains unowned |
| Menus, 2,400 frames | 16 weapons in native 4×5 grid, distinct quick keys 18/65554; assembled drill preview and cartridge HUD inspected |

The six scenarios use Revo DLL SHA256
`ca5d978e86bcd06e0d253b5c5f01a324bda7192871f72e489626f55e8a88f28c`,
provider SHA256
`077a7306b71ac5f35776b74c261cceb1ff0c1144961a847696eabc961c71d55a`
and matching XP SHA256
`68205f76fea997336d4ee01fcb7c2812a2fdd32b6d79d3b67cc51a8b162cb67c`
where XP is loaded. 61 original input files, including the installed stock EXE,
remain unchanged. The receipt contains relative fixture identifiers and log
hashes, with no private absolute paths. Earlier pass-through coverage remains
provider-only; its zero menu/vendor counts do not limit these later scenarios.

This is not a complete campaign, all-boss/all-mod test, new-game-plus test or
every-save-slot certification. PAL, other operating systems and other executable
hashes remain unsupported by this provider. Host builds do not prove native
Linux/PAL gameplay. See the [detailed record](../revolverator/docs/NATIVE_VALIDATION.md).

## Earlier additive 0.2 results on a modified runtime — historical

The following observations used the earlier `fc717...` development executable.
They remain useful historical evidence, but do not certify the current
stock-client wrappers or newly compiled plugins. The [previous source receipt](validation/revolverator-0.2.0-preview-source.json)
records 30/30 host groups for that source state.

| Historical evidence | Scope |
|---|---|
| Native combat XP | Revolverator earns 0→39 from an attributed Birdbot kill while Walloper stays 0; a later native Walloper kill earns its own 0→39 |
| Native commerce/pickup capacity 20 | Hoven 15,000→20 rounds; refill 2→20 costs 450; Walloper 7,500 independent; pickups 4→6 and 19→20, then remain 20 |
| Native Novalis menus | 16 entries in native 4×5 grids, distinct favourites, assembled rotating previews and cartridge HUD |
| Limited-memory native HUD | Rilgar 5/Nebula 6/Quartu 15 complete 2,400-frame standalone runs; exact authored 32px cartridge texels/CLUT, displayed 19/20; compact/native glyph fallback on 5/6 |
| Native saves | Independent custom sidecars, successful save/load and duplicate autosave behavior in the isolated fixtures described by the detailed record |

The limited-memory runs used DLL SHA256
`62cbd609ec86b2088888eef4c55b901e17b3107fa90f89e 615b1816047b312cd`
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

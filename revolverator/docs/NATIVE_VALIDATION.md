# Native validation

Release validation identifies the exact weapon DLL, asset adapter, official
client, source snapshot, disc region and native scenarios. Use the validation
attached to the release being installed. A historical pass does not certify a
newly rebuilt DLL or every later client.

Version 0.3.3 adds contact sparks, automatic player-disc asset derivation and
minimum-interface compatibility. Its host, native renderer, clean-install and
publication checks are recorded separately from the receipts below. See
[contact sparks](CONTACT_SPARKS.md), [runtime assets](RUNTIME_ASSETS.md) and
[compatibility](COMPATIBILITY.md).

The current binary's authoritative [release validation](https://github.com/noire342/openrac-mods/releases/download/revolverator-v0.3.3/revolverator-0.3.3-validation.json)
is attached to the release and embedded as RELEASE-VALIDATION.json in both
archives. It includes final native evidence and its source/binary pins.
The [metadata-only query receipt](validation/version-metadata-0.3.3.json)
separately verifies the manifest's single source of truth for version 0.3.3
and required interface 1.12; a metadata query is not a gameplay test.

### Earlier 0.3.3 candidate evidence

The preserved [pre-publication receipt](validation/release-0.3.3.json) records the accepted
PAL Base and Golden runs on the official client: **136 native checks passed**,
both processes exited normally, and all 131 original inputs stayed unchanged
in each run. Each case produced 37 actual native Type19 particles, matching
the 37 emission requests, within 0.01202 units of the drill tip. No new
contact particles appeared during pause, idle, ordinary attack, equipment
changes or flight. Four unmodified native screenshots were reviewed.

That earlier candidate's weapon DLL is
425cb7ee886ef6ee868ee009d78b1e3fecd037fdb7e2a2fefae7b74c5af139f3;
the native-tested asset adapter is
8a69d4411696c76e6ea7229ad66999bbddd107bac76a7338771acd67a20b14c4.
These hashes identify evidence, not future client-version requirements.
The [asset preparation](validation/runtime-asset-preparation-0.3.3.json) and
[actual C++ constructor](validation/runtime-asset-constructor-0.3.3.json)
receipts separately cover cold/warm cache behavior, corruption repair,
unchanged source assets and long Windows paths. Failed exploratory native
runs remain excluded in the contact-sparks receipt.

## Version 0.3.2: PAL gameplay and presentation

The accepted 0.3.2 DLL is
9d9ba8704588b5838589d6d0042e7a8cc8ee972b4e0fee24cf3c26672fce7a52.
Its [native regression receipt](validation/pal-032-regression.json) covers
264 checks in six scenarios; [enemy eligibility](validation/pal-032-heavy-enemies.json)
adds 33 checks in three scenarios. All nine processes exit normally, with
107 original inputs unchanged per run. Windows host tests pass 33 groups.

These receipts establish their recorded equipment-menu, actual Bomb shot,
construction, pause, attack, Base/Gold carrying and original Walloper paths.
The heavy-enemy comparison records equal native per-hit damage for Alien
Snapper1048 and Bomber Tank253, plus actual Blarg Commander1371 capture,
held-update suspension and throw. Toxic Crab1202 native Orxon attempts are
explicitly excluded because a stable usable Ratchet state was not reached;
its unchanged exclusion is source/host verified. Heavy-target Golden coverage
and complete mounted-Commander behavior are not implied by that test.

Earlier PAL receipts, including carry-speed, preview/title and visual-assembly
corrections, retain their original DLL hashes under docs/validation. Their
binary-specific scope is described in the corresponding documents.

## Historical version 0.3.1: OpenRAC 0.10 NTSC-U

The official Windows NTSC-U 0.10 executable matched the installer-extracted
file, SHA256 f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b.
It used format-4 levels. [OPENRAC_0_10.md](OPENRAC_0_10.md) retains that SDK and
client provenance; it is not the current minimum-interface requirement.

| Historical component | SHA256 |
| --- | --- |
| Revolverator 0.3.1 | c9294347cacad667bd9436feb6ac7c22196ddfff151c745dec8437d3554739f7 |
| Arsenal Compatibility | 36a52a6501dd2737a7f5957151e29ad961726a66eca5670727715d277cf2414d |
| File overlay | ba99b27d68168630d206020cc75b8888760958d3110b855e8598833335a7dc12 |
| Weapon XP, when enabled | 68205f76fea997336d4ee01fcb7c2812a2fdd32b6d79d3b67cc51a8b162cb67c |

The local final-pulse receipts were recorded under
work/revolverator-openrac-010-review. Fixtures seed base ownership, challenge
access, ordinary bolts and positions, but not Golden ownership, enemy health,
rewards or XP. The level-up fixture temporarily lowers session thresholds and
restores the original rules.

| Receipt | Observed native result |
| --- | --- |
| final-shop-no-xpfinalpulse.json | 23 checks: actual purchase at 3.35 m, eleven naturally lit cases, stationary centered display and complete acquisition without XP |
| final-combatshortfinalpulse.json | 40 checks: one throw/explosion, three accepted hits/deaths, Revo117 XP and Walloper0; activation pulse and normal carrying scale |
| final-combatlongfinalpulse.json | 40 checks: automatic release at 3 s, one explosion, five accepted hits/deaths, Revo156 XP and Walloper0 |
| final-switch-flightfinalpulse.json | 39 checks: switch to wrench during flight; explosion and attribution preserved; Revo117 XP and Walloper0 |
| final-levelingfinalpulse.json | 44 checks: earned enemy XP, one V2 acquisition, Gold model, one visible Ratchet and blocked input |
| final-savefinalpulse.json | 27 checks: purchase saved with 7 rounds and 10,000 bolts, codec2 sidecar bound to native save |
| final-reloadfinalpulse.json | 4 checks: new process restores Golden, exactly7 rounds and 10,000 bolts |

These receipts report 84 unchanged original inputs and no native errors.
Release recovery holds Ratchet for nine ticks, about150 ms, without horizontal
drift. The activation pulse reaches2x radial/1.2x axial size and55 rad/s; size
returns to1 while carrying.

The separate base-tap log records BASE_SINGLE_TAP_PASS with those same values
and one-round cost20 to19. REVO_EARNED_PASS records an actual capture, throw,
impact and death: Revo39 XP, Walloper0, with damage/capacity bonuses disabled.
It does not include a subsequent Walloper kill.

The first final-rapidfinalpulse.json is excluded: physical input interfered
with fixture purchase. Earlier rapid-reuse results do not independently
certify that final DLL. The 25-group host suite includes4027 rotor,
493 native-adapter and95 Gold-shop checks. Those host results remain separate
from Novalis native observations and do not establish PAL, complete campaigns,
every boss phase or native Linux/macOS play.

## Historical version 0.2: launcher-installable package

The then-tested package included private utility DLLs and graphics companions
on the unchanged official NTSC-U client. That packaging predates the current
player-disc derivation boundary. The retained receipt is
[revolverator-0.2.0-native-package.json](validation/revolverator-0.2.0-native-package.json).

| Scenario | Evidence |
| --- | --- |
| 900-frame smoke | Production Revo/XP only; no helper;20 rounds and control at frame521 |
| 2400-frame menu | 16 weapons, native4x5 grid, favorites18/65554 and actual previews/HUD |
| 1800-frame XP | Actual capture/throw/impact/death; separate Revo39 then Walloper39, without seeded XP |
| 2200-frame save | Isolated real card;7-round snapshot,3 while pending,7 on reload; valid CRC/fingerprint and original Walloper unowned |
| Two2400-frame vendor/pickup runs | 24 native checks, capacity20 and independent inventories |

The smoke has no QA helper; targeted runs name their helper. Helpers are not
installable dependencies. Novalis loads345 models and61 original inputs,
including the executable, stay unchanged. Historical DLL hashes:

- Revo: ebe140838d24ebb4707b74fc06ad94a935c9ab135c885ad87a78d9f2081fdef5
- File utility: aec87cbfd6eb3133dbb51e62edbce7fae46081355dcd4b5b5aa458876860ca14
- Hook utility: 077a7306b71ac5f35776b74c261cceb1ff0c1144961a847696eabc961c71d55a
- Official client: b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081

The34 Windows/Linux host groups are separate from gameplay proof.

## Historical version 0.2: manually configured stock-client utilities

The original pass-through boundary used official NTSC-U commit77a2f052,
client b7f3762a...df94081 and Arsenal Compatibility0.1.0,
provider077a7306...c71d55a. The1800-frame pass-through fixture resolved and
committed30 detours. Observed calls included NewGame1, Restore2,
RestoreData42 and UpdateState1056. Registering30 hooks does not mean30 game
paths were exercised; vendor, pickup and menu were not tested by that fixture.

The [stock native receipt](validation/revolverator-0.2.0-stock-native.json)
records six scenarios using Revo
ca5d978e86bcd06e0d253b5c5f01a324bda7192871f72e489626f55e8a88f28c,
the same stock provider/client, and Weapon XP68205f76...2cb67c when enabled.
It covers a720-frame no-helper startup, vendor/pickup24 checks, earned Revo
and Walloper XP, pending-save snapshot restoration and the16-weapon menu.
The original61 files stay unchanged;32 host groups and5158 fake-backend
provider checks do not enlarge the native scenario scope.

## Historical modified-runtime development

Early0.2 fixtures used NTSC-U Windows runtime77a2f052 with30 added hooks and
optional Weapon XP0.6.3-arsenal. These are isolated development results, not
the current installation requirement. Early XP tests used capacity10; later
commerce/pickup/HUD tests explicitly repeat with capacity20.

Retained observations include30 matching hook signatures,2400 menu frames,
separate Revo39 and Walloper39 earned XP,24 commerce checks, sixteen weapons
in the native4x5 panel, cartridge HUD, assembled previews, pending-save
snapshot handling and658 host XP-consumer checks including ASan/UBSan.

The Rilgar5, Nebula6 and Quartu15 standalone HUD fixtures each run2400 frames.
They show a held weapon and cartridge HUD19/20 with exact frame3 texels/CLUT.
Rilgar/Nebula use32px drill glyphs and Quartu128px; remaining HUD heaps are
3456/624/12304 bytes. They do not exercise menus or XP. The binary is
62cbd609ec86b2088888eef4c55b901e17b3107fa90f89e615b1816047b312cd;
the local receipt is packed-hud-native-62cbd-03.json.

The reproducible earned-XP fixture runs1800 frames with actual Birdbots.
It assigns weapon ownership and positions but not enemy health, rewards or XP.
Accepted origins2017 and180 reach native death handling separately. Its Revo
hash is dd3ae8723a7eb5d0071e7bf0c1321a9a8b29aee0e714c952ead7074a442d537e,
with runtime fc717cd0526ca1977895f3baf8e86186adcc5bfd41d0d75b5622148f0e689f9b.

The menu/preview fixture runs2400 frames with Revo0.2.0 and XP0.6.3-arsenal.
Frames960/990/1020 and1500/1560/1620 show assembled previews;2340 shows the
cartridge HUD after closing quick select. Its Revo hash is
402c5e92c215130529a9d3f80c08a232857c33b58f593f258fd671e85814a054.
Held and neutral-preview rotors stay separate. The dedicated cartridge frame3
leaves inventory/quick-select drill frames intact;63 HUD-contract checks and
the then-complete17-group Windows suite pass.

## Scope

Every historical section retains the limits of its own receipt. Neither
source tests nor targeted Novalis observations prove a complete campaign,
every boss, every mod combination, every save slot, every language/aspect ratio
or native gameplay on an untested operating system. The operational plan is
[QA_CHECKLIST.md](QA_CHECKLIST.md).

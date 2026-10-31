# PAL reference region and validation

Revolverator0.3.1 uses the official Windows x64 OpenRAC0.10 client and
the R&C1 Europe v2.00 disc, serial `SCES_509.16`. NTSC-U remains a separate
supported binding profile. The Italian weapon name is **Rotatore**, or
**Rotatore d'oro** with its Gold upgrade. The stable item identity remains
`noire342.revolverator` / `65554`; localized names never identify save records.

## Regional boundary

`RegionBindings` maps explicit canonical keys to the selected region before
SDK relocation. Executable data, overlay data, function names and guest callback
entries have separate lookup paths. Live guest pointers are never converted.
Unknown PAL keys fail with a contextual error; there is no universal offset.

The compatibility utility validates the region and the exact official EXE hash
before creating hooks. The asset loader accepts manifests of the same region
and mounts session-owned additive copies, leaving the extracted levels intact.
The private file overlay accepts only the documented official client hashes.
No patched client is distributed.

Both regions use the same controller, logical inventory, ammunition rules,
Gold purchase rules, progression codec and public API. PAL menu cooldown and
layout details follow its native functions; frame barriers and elapsed-time
durations are kept distinct.

## Saves

PAL uses the native card profile `SCES_509.16/BESCES-50916RATCHET`;
NTSC-U uses `SCUS_971.99/BASCUS-97199RATCHET`. An explicit `--cards` directory
continues to take precedence. Region profiles do not automatically import or
overwrite another region's card. Codec2 reads codec1 sidecars, and each sidecar
still requires the matching native save hash.

The old reserved `WalloperNativeState.unlocked` codec byte is retained for
format compatibility but is neither read from nor written to game memory.
Its former address belongs to skill point18, not weapon unlock state. Native
skill-point changes must survive equipment projection and save scopes.

The PAL memory-card driver has its own layout, including a larger card record;
changing only the global address is insufficient. Both layouts have explicit
bindings and regression tests. Native saves retain ordinary Walloper state and
sanitize custom favourite/equipment transport values. Matching sidecars restore
the logical weapon, Gold upgrade, ammunition, earned XP and eight favourites.
A restored custom selection requests its native carrier once Ratchet is ready;
this also handles a restore into the already loaded map. It never grants or
refills the weapon.

## Verification scope

The sanitized [PAL0.3.1 receipt](validation/pal-0.3.1.json) binds every native
fixture to its DLL, companion, utility and runtime hashes. Its current
Revolverator DLL is `bd880b9a...1622a5aa`; the official PAL runtime is
`e720a59d...205fb0e`. Full hashes and relative private-archive receipt references
are recorded in the JSON. The raw user paths, saves and RAM dumps are excluded.

Native checks on that current DLL completed with normal exits and107 original
input files unchanged per fixture:

| Fixture | Checks | Native evidence |
| --- | ---: | --- |
| `pal-combat-release-03` |41/41| Gold purchase/acquisition, accepted explosive hits and deaths, earned custom XP with independent Walloper XP, one-round cost, bounded recoil and resumed movement |
| `pal-switch-final-01` |40/40| The launched enemy completes its Gold explosion after switching to the native wrench |
| `pal-reload-02` |10/10| Gold, ammo7, XP117, bolts10058, actual carrier equip and all eight logical/native favourites restore |

The reload used a save produced by the earlier `3d9ca983...75c61a91` DLL and
the earlier XP companion recorded in its receipt. Current combat/switch checks
used Weapon XP0.6.4. That companion has a separate
[XP/save supplement](../../weapon-xp/docs/validation/pal-0.10.0-0.6.4.json);
its tests are not attributed to this weapon's DLL.

Earlier native fixtures remain explicitly historical: native PAL card creation
and full save50/50; two overlapping flights with two separately accepted
collision bursts37/37; full-charge combat41/41; and real-XP V2 acquisition45/45.
The V2 fixture temporarily lowered session thresholds, without seeding XP.
They were not rerun on the current DLL and do not expand its native test scope.

The historical icon fixture passed24 Gold-shop checks and observed60 frames of
the native weapon wheel through unforced input. Its recorded manual review of
frame1055 confirmed the clean Gold drill tile and separate cartridge HUD icon.
This is evidence for that screenshot/build, not every UI state. Failed early
fixtures remain excluded with their reasons in the receipt.

The official extractor validated the PAL executable. All21 extracted levels
passed the client's `--check-levels` importer. Static staging/skeleton checks
cover all19 gameplay levels, including Walloper skin bindings and Ratchet
joints53/54/56; these are not gameplay runs on19 planets. Both supported port
builds tick at60Hz, confirmed by native `DeltaSeconds` measurements; the PAL
disc's original50Hz identity does not set elapsed-time policy.

Host validation covers31 Windows native-adapter groups and52 groups in the
public Windows source tree. The final public Linux run passed **52/52**, with
zero failures, in14.58seconds. The complete retained stdout is
`work/publication-tools/pal031-linux-tests-final.log` (SHA256
`c5ebe4b701a11b339ad9747c5e8451eb739533b4d44832e166b8c521e67c4938`). An earlier51/52 run exposed CRLF in a Windows-checkout TSV fixture;
the fixture was normalized to its pinned LF format without changing runtime code.
These checks exercise regional bindings, build mismatch rejection, isolation,
save/restore and existing contracts. Native Linux/macOS gameplay and a
current-binary NTSC playthrough are not established.

Full-campaign coverage, every boss/weapon/planet, all ten levels in a complete
playthrough, and automatic cross-region save migration remain unverified.

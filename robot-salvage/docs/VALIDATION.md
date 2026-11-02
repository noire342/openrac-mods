# Development validation scope

This source snapshot is published for development and review. It is not a
finished release or a whole-campaign certification. No installable archive is
added to the launcher's release catalog by this source publication.

## Reference and evidence boundaries

Native work targets the unchanged official OpenRAC 0.10.0 Windows x64 PAL
executable, SHA256
`e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e`,
with the owner's Europe v2.00 / SCES_509.16 extraction. No ISO, WAD, private
memory snapshot or personal save is included here. Linux tests exercise host
contracts only; the native adapter is PAL-only.

The source-specific automated results are in [host-validation.json](host-validation.json).
CI builds this module and its shared utility on Windows and tests portable
contracts on Linux. Host tests cover catalog identity, atomic transactions,
leases, stale generations, collection input/LOS/precedence, physics and
materialization limits, sidecar integrity/errors, cannon custody, UI layout
and native menu transitions. They do not replace gameplay evidence.

Historical native checks have separate binary scopes:

| Scope | Observed result | Limit |
|---|---|---|
| Birdbot real Blaster death, original fragments, settling, pickup | Original head/cannon/hand/leg retained beyond native TTL; held Triangle consumes one part; inventory conservation | Novalis path, not every robot family |
| Native Suck Cannon, `pal-salvage-cannon-native-12` | 21 checks passed, exit 0; same original actors/models stored and launched; one native terminal explosion, no extra enemy death reward; 107 original inputs unchanged | Salvage DLL `e661de06545f63824642ceb5931e805b46688d97cfb1b76d986db3d74b116a76`; separate artifact, not the new source build |
| Native collection prompt, fixture 20 | 21 checks passed; actual Triangle glyph and descriptive Birdbot name, held-input behavior and equipped cannon | Separate earlier prompt fixture |
| Root/directory transition, `pal-menu-transition-fixed-30` | 20 checks passed, exit 0; clean forward/back panels inspected; 107 inputs unchanged | Standalone menu helper, not a full combined-package gameplay run |

The current source incorporates the menu transition regression and a correction
for long off-center fragment bounds. A historical passing binary does not
qualify the entire current source snapshot. Current build hashes are recorded
separately from the historical native evidence.

## Known limitation

Some native factories produce different physical sizes for the same model.
The current catalog stores the first encountered shape for that component ID.
For example, bomber source 688 / fragment 1510 varies with its native factory
scale. A per-instance physical-property override is still being developed;
these variable-size cases are not accepted as correct in this snapshot.
The inventory's stable identity and stacking must remain independent of size.

## Acceptance work still open

- Complete native physics, contact, player push and pickup checks for every
  supported robot/fragment family, including large and variable-size parts.
- Verify overlapped fragments, large accumulations, native actor-pool pressure,
  suspension/reactivation and collider reuse with the final build.
- Complete real nearby vendor/NPC/vehicle precedence and obstructed pickup cases.
- Complete Cannon cancellation, full capacity and eligible ordinary-enemy
  coexistence on the final coordinated build.
- Complete both companion load orders across death, planet changes, native save,
  restart and loads of different slots using the final coordinated packages.
- Finish long-label/maximum-quantity visual checks and final UI integration.

The source adds scoped loading guards to the companions to avoid writing to
level memory reused for incoming compressed resources. The old v1 broker ABI
remains present. These changes are tested as source contracts; existing public
release binaries are not silently replaced or certified by this commit.

Architecture separation and passing tests support maintainability. They do not
guarantee the absence of gameplay bugs or make incomplete acceptance checks pass.

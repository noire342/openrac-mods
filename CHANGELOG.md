# Changelog

## Skill Points 0.1.3 and Cinematic Journeys 0.1.1

- Verify the exact official OpenRAC 0.10.0 client and all 19 Skill Points callback maps.
- Separate the Skill Points runtime, bounded game memory, C service and DLL lifecycle.
- Validate the API prefix and required services before registering hooks/callbacks.
- Validate cinematic reports in both PS2 RAM and the mapped port arena; preserve normal pause.
- Add lifecycle/failure regressions and SDK 1.10.1 Windows/Linux standalone build/package CI.
- Make packaging independent of private QA folders and versions; preserve profile format/API v1.
- Document reproducible checks, exact binary identities and native coverage limits.

## Skill Points: RetroAchievements 0.1.2 / Cinematic Journeys 0.1.0

- Publish both standalone modules, Windows installable ZIPs, corresponding
  source archives, retained third-party notices and native validation receipts.
- Extend repository discovery to four mods and add independent Windows/Linux
  CI jobs for the two new modules.
- Skill Points integrates 178 official rules into the original menu, accessible
  below Options from a new campaign; optional Kerwan timed challenge defaults off.
  Progress is offline and separate from memory cards and Weapon Experience.
- Cinematic Journeys preserves the deliberate four-shoulder-plus-Start skip
  chord and adds a configurable 5-second minimum interplanetary animation.
- Preserve the native proof boundaries: Skill Points is NTSC-U; Cinematic
  Journeys has a PAL adapter but its in-game evidence is NTSC-U only.

## Revolverator 0.2.0 preview.1 / Weapon Experience 0.6.3-arsenal

- Install Revolverator as one normal native launcher package. Bundle utility
  DLLs privately, stage 19 original manifests read-only and load packaged class
  assets automatically; no external launcher or extra utility cards.
- Limit repository discovery to Weapon Experience/Revolverator through the root
  index, and document the explicit preview release URL for prerelease selection.
- Extend the combined host matrix to 34 groups with asset staging and file-open
  policy/parser tests. Keep earlier manual/helper proofs historical.

- Record six stock-client native scenarios: real trial, 24 commerce checks,
  earned independent 39/39 XP, 7→3-pending→7 reload and native 16-entry menus.
  The 61 original inputs, including the official client, remain unchanged.
- Run the additive preview on the unchanged official Windows client using the
  new Arsenal Compatibility 0.1.0 mod. No client EXE is shipped or replaced;
  the previous engine patch remains historical source reference only.
- Keep24 stock SDK hooks separate from30 exact-build compatibility boundaries.
  Add versioned claim/bind/commit/rollback/diagnostic APIs with owned teardown.
- Expand the combined Windows/Linux host matrix to 32 groups including the
  runtime consumer adapter and portable compatibility provider contracts.

- Revolverator 0.2 is additive: original Walloper preserved, logical
  key 65554, independent 20-round inventory/XP/favourites and fingerprinted sidecars.
- Include matching Weapon Experience 0.6.3-arsenal source with API v4 hit ownership
  and acquisition helpers, plus the GPL engine bridge source patch.
- Expose Revolverator API v2 and declarative discovery metadata for future tools.
- Follow the upstream GPL-3.0-or-later mod convention; preserve SDK/interop MIT
  notices and prior Git history. Original game art is excluded from this source.
- Add an isolated Novalis trial launcher and an optional `start_unlocked` setting
  (off by default); the normal path retains the Hoven purchase.
- Enable position-independent core objects so the additive library also links
  on Linux.

- Make the native memory helper templates available to every translation unit,
  fixing GNU linker errors in the Linux source build. The published Windows
  0.6.1 artifact and its source ZIP remain unchanged.
- Build/test on Windows and Ubuntu 24.04 using current GitHub Actions runtimes.

## Weapon Experience 0.6.1

- Ten persistent levels for twelve weapons, with progression-only defaults.
- Native HUD and acquisition presentation; configurable per-weapon rules.
- Public C APIs v1, v2 and v3, with existing layouts preserved.
- Independent opt-in Morph-o-Ray model-pose toggle; native targeting and
  transformation behavior remain in control.

See [the full 0.6.1 changes](weapon-xp/docs/CHANGES-0.6.1.md).

## Revolverator 0.1.0 — source preview

- Publish source for capture/spin/throw, native ammunition/HUD, Hoven vendor,
  contact-based attachment and the separately controlled original drill tip.
- Add a session guest arena, five-language text, a copy-only C API and ten
  host test groups, plus local asset/bundle builders.
- Record native Windows NTSC-U checks and 19-level load smoke evidence, with
  explicit Rilgar HD fallback and Blarg Station pickup-text limitations.
- Publish no Revolverator binaries or game-derived art. Weapon Experience 0.6.1
  source and release archives remain unchanged by this preview publication.

See [current scope](docs/REVOLVERATOR.md).

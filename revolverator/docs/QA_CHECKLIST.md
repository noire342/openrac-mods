# Release verification checklist

This is a verification plan, not a claim that every box has passed. The release
receipt records completed observations against exact artifacts. Historical
results are indexed in [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).

## Installation and compatibility

- Install the public ZIP through the official launcher with only Revolverator
  enabled, then test the optional XP combination separately.
- Confirm the minimum interface and required capabilities; supported later
  clients must not be rejected solely by version number.
- Start with an empty local asset cache. Confirm automatic derivation, normal
  gameplay startup and unchanged native files; repeat with a warm cache.
- Confirm no copied game art, patched client, separate Python dependency or
  manual utility-card step is required.
- Check notices, version, source archive, hashes and public downloads.

## Contact sparks and presentation

- Observe native sparks at the actual contact during a captured spin.
- Confirm rate follows speed, remains bounded and does not accrue in pause.
- Confirm no contact emission at idle, on a miss, during equip construction,
  while empty, after release, after target loss or during menu previews.
- Exercise target replacement, cancellation, death, equipment changes and
  level transitions without stale entity or particle pointers.
- Keep Base and Golden, native Walloper, attack enlargement, normal carrying
  geometry, acquisition scenes, menus and the cartridge HUD distinct.

## Gameplay and persistence

- Test tap versus minimum uninterrupted hold, capture, spin, throw, impact and
  rapid reuse. Verify one round per activation and capacity 20.
- Test Hoven purchase, normal ammo pickup/refill and the Novalis Golden purchase.
- Verify excluded heavy enemies receive native melee damage; Blarg Commanders
  remain capturable. Check eligible light targets near excluded targets.
- Confirm the 50% carry-speed limit and release recovery without altering
  vertical motion, platform movement or camera control.
- Exercise weapon-menu confirmation, scrolling and eight quick-select slots.
- Save and reload independent ownership/ammo/Gold/favorites; reject corrupt,
  absent or mismatched sidecars and preserve native Walloper state.
- With XP enabled, confirm earned custom XP, separate Walloper XP, level-up
  presentation and no credit for suppressed hits or duplicate death records.

## Evidence boundary

Use isolated cards/settings for automated fixtures and record seeded setup.
Observe exit codes, original-file hashes and renderer output. Host suites and
regional bindings do not establish full-campaign, every-boss, every-language
or arbitrary mod-combination compatibility.

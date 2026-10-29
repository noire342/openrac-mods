# 0.6.0

- Extracted validated per-weapon progression/reward/bonus rules from native adapters.
- Added editable bounded INI rules, launcher controls for native animation/vN names/kill cap.
- Added versioned C ABI v3 for copied configuration, atomic batch patches, revision checks,
  reload, explicit save and diagnostics. Preserved v1/v2 prefixes and co-op provenance.
- Added storage format 4 for custom XP curves; exact migration and .v3.bak for 0.5.0 profiles.
- Preserve actual last-written capacity during live rule changes. Default behavior remains
  ten levels, native weapons and both bonuses disabled.

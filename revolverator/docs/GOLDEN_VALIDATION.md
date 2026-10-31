# Golden Rotatore0.3 native validation

All native scenarios use the unchanged official Windows OpenRAC0.9.0 NTSC-U
executable, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
They use the original installed levels, the native mod package and private
compatibility DLLs. No custom executable or external graphics stack is used.
Helpers seed base ownership, ordinary bolts, NG+ access and actor positions;
they do not seed Golden ownership, enemy health/rewards or experience.

Verified paths:

- Actual Novalis shop, with all ten native cases and the additional case.
  The static Gold display stays joined and the prompt uses the native HUD.
  One Triangle press deducts exactly50,000 normal bolts and no Gold Bolts;
  native Gold flags, Walloper ownership, ammunition and XP remain unchanged.
- A short spin gives blast charge0.111, radius2.88 and maximum damage8.88;
  automatic release at3seconds gives charge1, radius6 and maximum damage16.
  Accepted native packets hit multiple Birdbots; native deaths and bolt-based
  XP are observed. The original Walloper receives none of this custom XP.
- Real motion input cannot move Ratchet horizontally for9ticks, about150ms,
  at release. Movement resumes afterward. The projectile is not delayed.
- Switching to the wrench after release preserves the flying enemy's Golden
  explosion and XP attribution and cancels the locomotion hold.
- An actual purchase is written by the native save flow on a copied card.
  A fresh process starts without custom ownership, loads that native save
  and its matching codec2 sidecar, and restores Golden,7rounds and10,000bolts.
- The base weapon and original Walloper still earn separate native XP.

The24 Windows CTest groups pass. Focused Golden impact, recoil and native
adapter tests also passed Linux GCC with ASan/UBSan; those are host tests,
not a native Linux compatibility claim. Tests cover line of sight, damage
eligibility including boss classes, actor generations, duplicate impacts,
the bounded48-hit plan, cancellation, timeout, sidecar migration and native
memory ownership. All19 Golden held models retain the exact original GLB
binary buffers: geometry, UVs, topology, skeleton and rotor weights.

Limits: native combat proof is on Novalis Birdbots; it does not establish a
full-campaign/all-boss-phase run, PAL, native Linux/macOS or arbitrary mod
combinations. This local0.3 package has not been published to GitHub.

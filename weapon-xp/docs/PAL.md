# PAL validation

Weapon Experience **`0.6.4`** passed the scoped native Novalis checks below on
**official OpenRAC 0.10.0, Windows x64, PAL v2.00 / SCES_509.16**. This is native
smoke evidence for the tested build, not a complete campaign compatibility claim.

Validated `weapon_xp.dll` SHA-256:
`29406690A04910737CC14B02D1EF1CDEA39D3C26650BF51B72CA71A1FBF758EA`.

## Current 0.6.4 evidence

The [0.6.4 machine-readable receipt](validation/pal-0.10.0-0.6.4.json) records every runtime,
companion and probe hash, log markers, screenshots and relative QA receipt paths.
The QA runs used isolated fixture directories and no user saves. QA helpers are
not part of the public game ZIP.

Native combat and profile reload were repeated against the actual new 0.6.4 DLL
in `pal-xp-064-native-01` and `pal-xp-064-reload-01`; both processes exited with zero.
The current host run again passed 13/13 groups. Frame1200 from the new native run
was visually reviewed for the vertical Suck Cannon HUD and LV1 label. All 107
hashed original inputs remained unchanged, and the native/reloaded XP profiles
were byte-identical.

The earlier [0.6.3-arsenal receipt](validation/pal-0.10.0.json), for DLL hash
`6074BAE6770EF3647C531C2906D4248C2E309A07CB8B49B94434024A5A95514B`, is retained
unchanged as historical evidence. The current receipt identifies the newly tested
binary separately.

## What passed

| Check | Observed result |
|---|---|
| Build and host tests | Release x64, 13/13 groups passed; PAL/NTSC adapters and all four optional damage/capacity combinations exercised. |
| Native Walloper / Colpitore | Native Circle attacks killed an existing Birdbot; XP increased **0 → 39**. |
| Native Blaster / Blastatore | Native projectiles killed another Birdbot; XP increased **0 → 39**, ammunition **100 → 97**. |
| Independent Revolverator | Its XP and ammunition remained **0 → 0** during both native weapon kills. |
| Native level-up | One level event, native localized **Blastatore v2**, 199/199 acquisition samples with one scene Ratchet and the original body hidden. |
| Input ownership | All 198 sampled native pad reports remained neutral while movement/fire were deliberately offered during the presentation. |
| Suck Cannon / Cannone a risucchio | Original vertical gauge, **42 × 192**, native callback, integrated **LV1** and XP channel visually reviewed; 235 native draw calls, XP unchanged at zero. |
| Persistence | Format-4 profile contained 39 XP for each tested weapon. A separate process reread exactly **39 / 39** after 60 alive gameplay frames. |

The combat fixture never awarded or seeded XP, and never changed enemy health or
bolt rewards. It granted the original weapons through native Item_Give and moved
existing enemies and their matching bounds. To exercise a real level-up after
one earned kill, it temporarily set the Blaster's next session threshold to 20
through the public API. The original rules were restored after the native scene;
the earned XP was retained. Default progression thresholds were covered by host
tests, rather than accelerated by artificial awards in the game.

The reload fixture registered only a frame observer: no native hooks, item grants,
combat, pad injection, enemy movement, rule edits or XP writes. Its copied profile
remained byte-identical after the run. XP profiles are regional sidecars such as
`pal-campaign-1.wxp`, independent of the native game's save slots.

## Ratchet actor review

The second combat run counted the resident object pool, original body and separate
acquisition actors. Three stable Blaster samples and six samples after acquisition
retirement each had exactly one render-enabled/drawn class-0 Ratchet and no other
object sharing its body model.

During a Walloper punch there can legitimately be three translucent Ratchet
afterimages. The official PAL held-item attack code creates these copies with
alpha 48/23/12 and delays 2/4/6; native transform-history code uses flags `0x080A`,
fades them and deletes them. The observed alpha 38/13/2 copies matched that native
effect and disappeared before the stable Blaster samples. They are distinct from
an unwanted player duplicate surviving an acquisition scene.

The small background silhouette in frame720 preceded the level-up. At the sampled
Blaster stage, the pool contained one rendered Ratchet and two airborne Birdbots
(class459), consistent with the background figures. A precise pixel-to-object
identity is not claimed. Unallocated records with zero class pointer and zero
alpha were excluded; the diagnostic's raw `class0Live` total is not a count of
rendered Ratchets.

## PAL adapter source checks

The source audit used OpenRAC commit
`d015ea1c6925de90b4be91ddb349af83361d9d4b`. All 76 HUD callback addresses across
19 levels and both regions, six stable hook signatures and eleven PAL globals
were cross-checked. The validated DLL includes these exact PAL level-0 corrections:

| Native datum | PAL canonical address |
|---|---|
| Texture-group header / gauge depth | `0x0017E5D8` |
| HUD DMA packet cursor | `0x00161280` |
| Gameplay camera / acquisition alignment | `0x00166D80` |

XP service ABI v4 and its unchanged v1/v2/v3 prefixes match the Revolverator
consumer. Loader API `1.1` is a separate version number. The native tests used the
actual companion and Revolverator DLLs together.

## Coverage limits

The native checks cover Novalis, Italian localized names, earned XP for Walloper
and Blaster, one acquisition presentation, the empty Suck Cannon HUD, and profile
reload. They do not establish every weapon's native XP path, every camera,
language, planet or campaign transition. PAL v1.00 and Japanese discs are outside
this receipt. A complete campaign playthrough remains unvalidated.

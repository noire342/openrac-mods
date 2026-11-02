# Equipped preview and level-up title correction (unreleased)

Two PAL regressions reported after 0.3.1 are corrected in this local build:

- The inventory Ratchet preview now uses a separate root-preserving rotor pose.
  Native Walloper finger animation can no longer separate the drill head from
  its housing. Gameplay, isolated weapon preview and acquisition actors retain
  their independent pose owners.
- A successfully completed custom acquisition scene now uses the game's native
  notification channel. Each queued level retains its own version, displaying
  `Rotatore v2` or `Rotatore d'oro v2` in Italian. Interrupted scenes do not emit
  success messages, and the borrowed native text binding is restored afterward.

The [native PAL receipt](validation/pal-preview-title-hotfix.json) binds the
corrected DLL, official OpenRAC 0.10 executable, helper, logs and screenshots.
The published DLL reproduces the detached head and absent title. The corrected
base scenario passes 22/22 checks and the Gold scenario passes 49/49, with 261
inventory preview samples per variant. Both normal and Gold screenshots were
reviewed. All three accepted fixtures exit 0 and preserve their 107 original
inputs. Real enemy kills earn the XP; session-only lower thresholds make the
level transition practical without writing XP or enemy health/rewards.

The complete Windows Release suite passes 52/52. Focused pose tests cover both
PAL and NTSC plus base/Gold; title tests cover queued versions 2 through 10,
preferences and cancellation. Native proof here is PAL Novalis V2, not a new
whole-campaign or NTSC gameplay claim. This receipt does not replace the
published 0.3.1 validation receipt.
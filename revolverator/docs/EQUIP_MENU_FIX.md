# PAL equipment selection and assembly

This correction was published in version 0.3.2. Selecting Bomb Glove,
Blaster, Pyrocitor, the original Walloper or Rotatore in **Armi** now equips
that logical weapon. Custom ownership and ammunition remain independent.

PAL's native menu clears its pending-page pointer after installing a page.
The confirmation adapter incorrectly required that pointer to equal the active
page. It now accepts the native stable state (`next == 0`), retaining the
native selection cooldown, ownership, read-only and Back-transition guards.
The old DLL was reproduced using real native Cross input: all requested
native weapons stayed on Rotatore, and a requested Bomb shot consumed a custom
round. The same input route passes with the fix: Bomb ammunition goes 10 to 9,
and Rotatore ammunition stays at 20.

Equipping Rotatore starts its cosmetic construction sequence: the housing
appears from a compact scale, overshoots to 1.22x at 0.13 seconds and settles
by 0.29 seconds. The drill extends from its actual socket while making two
screw turns, reaching full length by 0.55 seconds. The sequence ends at
0.60 seconds. Its staging follows the native Blaster and Pyrocitor appearance
animations; gameplay remains available immediately.

The existing attack pulse is unchanged: 2x radial size, 1.2x axial size and
55 rad/s. The carry state keeps normal geometry and its existing movement
limit. Pause freezes an unfinished construction sequence. Opening Armi and
keeping the same Rotatore does not replay construction or its sound.

## Verification

Windows Release passes all 32 standalone test groups, including actual PAL
menu-layout regression cases, 752 assembly checks and 6488 visual lifecycle
checks. The native nine-cycle route on official OpenRAC 0.10 PAL passes
56 checks, including native confirmations, the real Bomb shot, separate
Walloper identity and repeated genuine Rotatore equips. Native audio accepts
one bank 3 / sound 127 command for each genuine equip and none for the final
same-selection menu visit. The muted fixture does not provide a listening
review.

The [native receipt](validation/pal-equip-menu-local.json) records all six
accepted scenarios, with **264/264 native checks**:

| Scenario | Passed checks |
| --- | ---: |
| Nine actual Armi selections and native Bomb shot | 56 |
| Base pause, construction and attack | 57 |
| Gold purchase, pause, construction and attack | 69 |
| Separate original Walloper | 20 |
| Base capture, movement limit and release | 25 |
| Gold capture, movement limit and release | 37 |

All six processes exit 0 and preserve the 107 original input files. Both carry
fixtures confirm the 50% native movement limit and nine recoil frames. The
receipt explicitly excludes exploratory runs whose old 30-frame observation
window ended before the new construction sequence finished; the accepted
reruns extend only that private helper wait to 45 frames, retaining all checks.

The exact tested DLL is
`fa0eb8aec7cb6c8bfe93f9e8a463da8690b827d79e114acfd958cf7094d63106`.
Both bundled compatibility utilities and all 79 existing asset files are
unchanged. Native evidence is specific to PAL Europe v2.00 and the recorded
scenarios; it is not a whole-campaign claim.

# Revolverator source preview

The [0.1.0 source preview](../revolverator/README.md) reconstructs the removed
drill weapon for the native OpenRAC port of R&C1 NTSC-U. It replaces Walloper
item 18 and uses the game's existing ownership, vendor, ammunition, quick-select
and save mechanisms. It does not add a new inventory slot or claim to recover
the removed weapon's complete original code.

Hold Circle near an eligible enemy to catch it, lift it at the actual contact
point and spin it above the drill; release to throw. A quick tap does not capture.
Only the original drill tip rotates. The native HUD retains its ammunition tile
and adds a slim charge channel inside it. Pickup text and weapon descriptions
support English, French, German, Spanish and Italian.

| Economy | Base value |
| --- | ---: |
| First normal vendor availability | After visiting Hoven |
| Weapon price | 15,000 bolts |
| Capacity | 10 charges |
| Consumption | 1 charge per activation |
| Ammunition pickup | 2 charges, capped at capacity |
| Ammunition unit price | 25 bolts |

The new pickup is one industrial energy cartridge, generated with AI assistance
from an original procedural mesh/texture recipe. It is not a set of spare drill
tips. The [builders](../revolverator/docs/BUILD_AND_ASSETS.md) read the original
Hoven drill from the player's own extraction and generate private local assets.
The custom cartridge requires a generated native level-manifest extension; it
cannot currently be added by treating a new render class as an ordinary model
replacement. No OpenRAC executable changes are required.

## Verified scope

The source includes ten host test groups. Native Windows NTSC-U checks covered
capture/throw, contact/shaft alignment, tip motor synchronization, empty-ammo
jerk/rollback, native ammunition and crate damage, Hoven pricing/unlock, API
cancellation and earned Weapon XP. All gameplay levels 0..18 were loaded in
smoke sessions. Orxon in Clank mode correctly blocks normal weapon use.

The detailed [validation record](VALIDATION.md#revolverator-source-preview) keeps
these observations separate from campaign coverage. They are evidence for a
source preview, not a full campaign or boss-fight playthrough.

## Known limitations

- NTSC-U only. PAL is untested and is not declared compatible by this preview.
- Public builds use the original 32px glyph. Optional private Real-ESRGAN HD
  builds preserve the same 32px logical footprint; Rilgar falls back to the native
  glyph when that level cannot reserve enough presentation memory.
- Custom cartridge pickup notifications can be unavailable on Blarg Station
  when its text bank cannot be extended within the available memory. Gameplay
  and native ammunition handling still operate.
- The original drill and the optional HD icon are game-derived art. They are
  generated locally and excluded from this repository and its CI artifacts.
- No Revolverator binary or ready-to-import public gameplay bundle is provided.
  The private bundle wrapper is a local build tool, not a new launcher format.
- Full campaign, every enemy, every interruption and all combinations of other
  mods have not been exercised. An enemy class whitelist governs capture.

Developer contracts are in the [API guide](../revolverator/docs/API.md), including
the optional mechanics tail, game-thread lifetime and item 18 XP identity.

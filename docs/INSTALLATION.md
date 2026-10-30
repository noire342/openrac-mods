# Installation and development status

## Stable Weapon Experience 0.6.1

The existing Windows x86-64 ZIP remains the latest stable download. Import it
through OpenRAC's Mods page, enable the mod and restart. It retains progression-only
defaults, ten levels and independently disabled damage/ammo bonuses.

Profiles are separate from native game saves. Choose one profile per campaign;
loading an older save does not rewind native weapon XP. Windows profiles are
under `%LOCALAPPDATA%/OpenRAC/weapon-xp/`. Migration, checksums, atomic replacement
and locking retain the behavior documented in that release. Its archives are
not changed by the development source on this branch.

## Revolverator 0.2 and XP 0.6.3-arsenal — preview

Use the matching preview asset and instructions on the release page, or build
these sources for review. This preview is separate from the final stable release.
Revolverator is an additional weapon; the original Walloper remains
available with independent ownership, favourites, ammunition and XP.

For a local test setup:

1. Build the mod using the pinned SDK and the [source instructions](BUILDING.md).
2. Build a separate NTSC-U runtime with the [arsenal bridge patch](arsenal-bridge/README.md).
3. Generate the drill/art and three extended render classes from your own
   extraction using the [asset workflow](../revolverator/docs/BUILD_AND_ASSETS.md).
4. If XP is enabled, use the matching 0.6.3-arsenal/API v4 source build; otherwise
   leave Weapon Experience disabled. The older stable 0.6.1 provider is not compatible
   with this custom-hit ownership requirement.

A DLL alone does not supply the runtime hooks or generated level manifests.
The preview launcher helper prepares separate sessions and defaults to its
bundled local runtime. Its first preparation copies the user's card/settings
and XP profiles into `.profile`; later sessions reuse that persistent copy.
It does not overwrite the official installation or original user card.

Custom state is stored in `saveN.bin.arsenal`, fingerprinted against the native
save. Original IDs and Walloper state stay in the native file. The normal vendor
starts offering Revolverator after Hoven for 15,000 bolts, with 20 rounds and
2-round pickups. The normal launcher does not grant it automatically.

NTSC-U Windows native checks and remaining limits are documented in
[Validation](VALIDATION.md). PAL gameplay and full campaigns are not certified.

The preview setting `start_unlocked` defaults to false. Enabling it grants the
Revolverator for an immediate test; the ordinary path retains Hoven purchase
conditions. `Avvia-Prova.cmd` uses `-Trial`: a separate empty Novalis session, the Revolverator
unlocked with 20 rounds, and isolated XP/card data. `Avvia-Revolverator.cmd` uses
the persistent `.profile`. For a custom persistent setup, `-StartUnlocked` is
optional.

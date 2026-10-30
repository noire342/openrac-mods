# Additive weapon inventory and native menu

Version 0.2 keeps the original Walloper and adds a logical Revolverator. Native
item arrays and save formats are not extended with an invented free ID.

| Identity | Logical key | Native transport |
| --- | ---: | ---: |
| Walloper | 18 | 18 |
| Revolverator | 65554 (`0x10012`) | 18, only inside the required context |

There is no safe spare native item ID in the audited range. Item 35 belongs to
the Persuader, and 36 has hoverboard/minigame behavior. Array sizes also differ:
unlocked 32, ammo 37, available 35 as declared, text variants 40. Native serialization
includes additional original flags. An ammo-array entry does not make an item
ID safe for ownership, equip, vendor, menus or persistence.

## Existing screens

`NativeWeaponMenu` extends the original weapon grid and quick-select editor,
using the game's panels, focus, text and preview functions. The view has five
columns and up to four visible rows:20 cells. Catalogues beyond 20 scroll through
the same grid. Navigation and its backing cells change together; merely drawing
smaller icons would not create an inventory entry.

The eight quick slots remain the existing eight slots. Their logical bindings
can distinguish Walloper 18 from Revolverator 65554; gameplay icons and equipment
project the selected key into the native operation. Preview and draw scopes do
not change the persistent logical ownership or silently equip another weapon.

The registry supports further descriptors internally. The public v2 service
currently grants/inspects the registered Revolverator; it does not expose a
cross-mod arbitrary-registration endpoint yet.

## Boundaries

- `arsenal::Registry`: stable IDs, ownership, XP, ammunition, equipment and logical favourites.
- `NativeArsenal`: scoped metadata/counter projection and original Walloper preservation.
- `NativeWeaponMenu`: cells, paging, navigation and preview contexts.
- `NativeArsenalSave`: native serialization/lifecycle boundaries and per-slot sidecar selection.
- `arsenal::Store`: validated atomic sidecar codec tied to the native save fingerprint.
- `NativeArsenalXP`: optional v4 ownership filtering and independent progression.

The native file stores original IDs and Walloper state. Custom favourite
bindings are sanitized while serializing, then RAM is restored. The sidecar
retains custom records and logical bindings. No global item count, descriptor
array bound or native save packet format is rewritten.

These contexts require the dedicated 30-hook OpenRAC bridge. Menu functionality
has native fixture evidence; final icon/model rendering remains under graphical
QA. See [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).

# Compatibility with OpenRAC

The public Windows x64 package requires **OpenRAC mod interface 1.12 or newer**
and the hooks, function bindings and asset-mount capabilities declared by the
mod. PAL Europe v2.00 is the native reference disc. NTSC-U uses its separate
binding profile.

The manifest declares a minimum interface, not a fixed client release.
A later OpenRAC version is accepted when it continues to expose the required
compatible capabilities. An exact executable hash in a validation receipt
identifies what was tested; it does not mean every later client must have
that same hash.

## Hooks and assets

NativeRuntimeHooks uses the official typed hook chain for the vendor icon strip
on interface 1.20 and newer. Earlier supported interfaces use a narrow bridge
for that callback. The weapon-preview pose uses its own bridge. The typed
void() and void(u32) bridges resolve named game-function
exports and validate that they belong to executable pages of the main client
image before activation. They use no fixed RVA or executable hash. The mod
owns activation and rollback.

The vendor icon strip delegates to the game when the list contains only stock
items. Revolverator draws the strip when the list includes a Revolverator row.
The manifest's required hook list still describes interface 1.12. On interface
1.20 the loader may log a warning for the additional icon-strip hook. The
runtime checks the interface before registering that hook.

The public package does not need a separate compatibility-provider mod card.
The original provider route is retained only for historical source compatibility.

The bundled asset adapter's v2 entry point validates the API prefix, major1 /
minor12-or-newer boundary, game, supported disc, memory and function/hook
capabilities. It mounts session-owned manifest aliases through the operating
system's typed CreateFileW export. This modern route accepts compatible later
minor interfaces without an executable-hash allowlist. The legacy entry point
retains its historical exact-build checks.

Capability checks must succeed before a mount becomes active. Unsupported
signatures or capabilities produce a load error instead of being guessed
from a client version number. The executable and installed game files are
not replaced or rewritten.

The player's extracted levels must match the selected disc and a supported
manifest format. The official launcher's **Check and play** action prepares
those levels. The mod then derives its models in a separate local cache;
see [runtime assets](RUNTIME_ASSETS.md).

## Other mods and persistence

Revolverator keeps the original Walloper as a separate logical weapon and
scopes native carrier projection to operations that need it. The stable
custom key remains 0x10012 / noire342.revolverator; changing the display author
or localized weapon name does not change save identity.

The optional Weapon Experience integration requires its API v4 contract.
Unsupported XP providers are reported rather than silently crediting the
native carrier. No other custom-weapon or shared-inventory integration is
claimed solely because it shares a carrier or an API header.

## Optional Expanded Quick Select integration

On **Windows x64**, Revolverator can detect the optional, read-only
`openrac_expanded_quick_select_query(1)` API provided by glawks'
[Expanded Quick Select v1.0.3](https://github.com/glawks/expanded-quick-select/releases/tag/v1.0.3).
This API is now available in the public EQS release and does not make EQS
a dependency of Revolverator. Without it, normal single-wheel behaviour
is unchanged.

With EQS enabled, EQS owns the editor, the native favourites in the game
save and the extra pages in its sidecar. Revolverator reads the selected
page through the API and leaves the wheel slots and icons to EQS. The
weapon remains selectable from the pause menu. Custom weapon bindings in
an EQS wheel require a future API that can store custom weapon identities.
Existing Revolverator favourites remain in its own data for use when EQS
is disabled. Detection uses the already loaded Windows module.

Compatibility checks: verify distinct native weapon layouts across pages 1–4
with Revolverator equipped and unequipped; edit assignments on pages 2–4,
save and reload, and verify all EQS pages retain their contents. Confirm
that assigning the custom weapon cannot insert a native item in an EQS
slot. Repeat with EQS disabled to confirm the original single-wheel
behaviour.

## Evidence boundary

The release receipt records its actual official client, source, mod DLL,
asset adapter, region and scenarios. Later clients are allowed by the minimum
interface/capability checks; native validation on an earlier client does not
itself certify every future change. Historical OpenRAC 0.9/0.10 tests remain
available in [native validation](NATIVE_VALIDATION.md) and are labeled as such.

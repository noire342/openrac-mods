# Release packaging and provenance

The Windows archive is revolverator-0.3.3-windows-x86_64.zip with
openrac-mod.json at its root. The manifest names author **lorenzogarrone**,
version 0.3.3, GPL-3.0-or-later and minimum mod interface 1.12. The release
tag is revolverator-v0.3.3.

## Installable files

Include the weapon DLL, its asset-mount adapter and license notices, original
authored ammunition model/texture, and the runtime preparation scripts in
[BUILD_AND_ASSETS.md](BUILD_AND_ASSETS.md). Dependency metadata must match the
DLLs actually included. Players install one weapon card, without manual
support-card or graphics-pack installation.

Exclude game files and derived copies: the Hoven drill, native textures,
recoloured native atlases, native HUD pixels, extracted levels, game code/data,
ISOs and private generated HD headers. Reconstruction happens locally from
the player's own disc. Also exclude toolchains, SDK files, RAM dumps, cards,
sidecars, profiles, credentials and local QA helpers.

The ammunition mesh and painted texture are original AI-assisted artwork.
The release and official listing disclose AI assistance. Native game artwork
is not relicensed by the mod's authored-code license.

## Corresponding source

Publish the exact corresponding source for every bundled native library,
including third-party notices and required build/asset recipes.
tools/package_source.py creates an allowlisted source snapshot and inventory;
it does not install or publish anything. A companion built from a different
frozen commit must have that source and provenance identified separately.
Never substitute newer shared-utility source for an older DLL.

Original copyright and attribution notices remain with third-party files.
Stable identifiers such as noire342.revolverator are API/save compatibility
keys and are not changed when the author's display name changes.

## Integrity and validation

Publish SHA256SUMS, the validation receipt and source archive beside the
installable ZIP. Check every ZIP member, manifest version, DLL hash and
required script before upload. After publication, download the public asset
and compare it with the exact file used for native validation.

Use tools/package_runtime_release.py with the SDK-generated module directory,
the exact tested adapter and their recorded hashes. The source manifest has
no generated platform plugin map and is deliberately rejected as a package
manifest. For example:

~~~powershell
python revolverator/tools/package_runtime_release.py `
  --module build-revolverator/package/revolverator `
  --overlay verified/arsenal_file_overlay.dll `
  --dll-sha256 <tested-weapon-sha256> `
  --overlay-sha256 <tested-adapter-sha256> `
  --validation verified/release-validation.json `
  --commit <full-corresponding-source-commit> `
  --output dist-revolverator
~~~

The validation receipt must report a successful native release gate and the
same weapon DLL hash. The tool checks the generated manifest, Windows x64 DLL
headers, both explicit binary hashes and authored-art hashes, then creates
the native archive, complete source archive, validation JSON and SHA256SUMS.
On Windows it also calls only the inert openrac_mod_query export and checks
its id, version and required interface against the generated manifest.
It verifies every ZIP member after writing and refuses to overwrite artifacts.
Both archives contain the final release-validation receipt; source receipts
from earlier candidates retain their original binary-specific identity.
The bundled adapter source is under support/file-overlay and builds through
the scoped revolverator_file_overlay target. Rebuilding that target does not
silently replace the adapter chosen for a validated release.

Receipts distinguish host tests, native input/renderer observations, exact
runtime hashes and publication checks. A build, installation or impact counter
alone is not gameplay evidence. Historical receipts retain their original
hashes and limits. See [PUBLICATION.md](PUBLICATION.md) for submission and the
official index's external review delay.

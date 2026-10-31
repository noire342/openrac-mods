# Catalog and native release contract

The normal launcher entry point is the repository-root address. Keep
`openrac-mods.json` and its four stable IDs intact. Utility folders are private
components, not extra installable cards. A localized Rotatore name does not
change the `revolverator` ID, logical key 65554 or sidecar identity.

Official OpenRAC 0.10 reads manifests at the first eligible release tag and
requires a native ZIP named `<id>-<manifest.version>-windows-x86_64.zip`, listed
in that release's `SHA256SUMS`. Repository-root discovery excludes drafts and
prereleases. A source-only main update does not make a prerelease discoverable.

The 0.3.1 release uses `prerelease=false` and `make_latest=true`, with four native
ZIPs: Revolverator 0.3.1, Weapon Experience 0.6.4, Skill Points 0.1.3 and Cinematic
Journeys 0.1.1. The latter two are copied byte-for-byte from their verified
releases; they are not rebuilt or relabeled. The release also contains complete
corresponding source for the new components, a binary-specific validation
receipt and checksums. Their existing releases/source/receipts remain available.

Before publication verify the tag's four manifests, asset names, checksums and
region declarations together. Skill Points remains NTSC-U-only. API/package
recognition does not expand a module's native validation scope. Preserve older
release artifacts and tags without replacing their assets.

Native version updates must change the numeric version core. The official
launcher compares major/minor/patch and the presence of a suffix; it does not
order different suffix text. Weapon Experience 0.6.4 therefore supersedes
0.6.3-arsenal instead of silently replacing that artifact at the same version.

Relevant primary source:
[remote release selection](https://github.com/OpenRAC/OpenRAC/blob/launcher-v0.10.0/launcher/core/src/remote.rs),
[version comparison](https://github.com/OpenRAC/OpenRAC/blob/launcher-v0.10.0/launcher/core/src/plugins.rs).

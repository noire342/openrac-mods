# Publication checklist

The release is prepared for the official [OpenRAC mod publication workflow](https://openrac.dev/mods#publish).

## Release

- Set the manifest version and author; use **lorenzogarrone** for display credit.
- Declare minimum mod interface 1.12, supported regions/platforms and real
  dependencies. Do not pin compatibility to one client release number.
- Build, run host checks and validate the exact installable package in the
  official client, including first-start asset derivation and contact sparks.
- Keep game files out of the ZIP and source archive; retain authored recipes,
  AI attribution, corresponding native source and third-party notices.
- Publish a stable revolverator-v0.3.3 tag with the Windows ZIP, corresponding
  source, observed validation and SHA256SUMS.
- Download every public asset and verify hashes, archive contents, required
  scripts and the exact native-validated DLL before announcing availability.

## Official index submission

The upstream README supports a pull request adding index/revolverator.json.
Set repo to https://github.com/noire342/openrac-mods and path to revolverator,
include the metadata below, and run tools/build_index.py --check-entry on the
entry in a checkout of the official index repository. Submit one listing PR
after the release and archive audit pass. This route needs no issue-label
permissions or computer UI automation.

Alternatively submit the [GitHub form](https://github.com/OpenRAC/openrac-mods/issues/new?template=submit-mod.yml)
once for mod id revolverator. Use one route, to avoid duplicate submissions.
The form asks for tags, impact, AI assistance,
documentation and bug links. Mark AI assistance **Yes**. The ammunition model
and texture are original AI-assisted artwork; the disc-derived weapon artwork
is prepared only on the player's machine.

A suitable short description is:

> Restores the cut Revolverator as a separate weapon: catch, spin and throw enemies, with native menus, ammo, saves and a Golden upgrade. Sparks mark the drill contact while holding a target. Assets are prepared automatically from your own disc.

Suggested tags: gameplay, cosmetic, ui. Suggested impact: gameplay,
touches-saves. Link the repository's issue tracker and Revolverator README.
Screenshots are optional; if included, upload them to the GitHub form rather
than linking images hosted elsewhere. A YouTube link is the only supported
optional video host.

Confirm the [content rules](https://github.com/OpenRAC/openrac-mods/blob/master/CONTENT.md)
only after auditing the actual files and listing. The submission bot reports
problems and opens the listing pull request; editing the issue reruns checks.

## Release versus listing availability

A GitHub release can be installed directly through the launcher by repository
link. A listing in the official index has a separate external review step.
The current index workflow holds a new external native version until a
maintainer approves it and it is at least 24 hours old. Each new external native
version goes through those gates before normal index refreshes publish it. A published release or submitted
issue must not be described as an approved listing before that happens.

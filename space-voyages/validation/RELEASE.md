# Release evidence: Space Voyages 0.8.0

The shipped Windows DLL is the unchanged PAL flight baseline:
`a891d74dd9ff066b4630c8b4842efaab7ecad183c6c4396640b9fe88ac574bc8`.
The tested SDK 1.14-02 PAL runtime was
`fa0c4eacc9a0a8ee9ecf2619597a2c915334b68e94ea185d6105ce09a1682e5f`.
That executable and the copyrighted local `pal-flightfx-07-final` assets are
not included. The cumulative engine source patch and frozen SDK are supplied.

## Source and CPU checks

All 91 source/test entries match the hashes in
`original-source-build.json`, which recorded this DLL and 40 passing CPU suites.
The source was restored from the frozen pre-audio-distance tree and verified
against every original hash; `source-restoration.json` describes that process.
Later audio-distance and campaign-v2 changes are absent.

A clean build of this public source passes 33/33 C++ suites without game data.
The original 40-suite configuration also exercised seven private asset-pack
cases. Those are distinct from the redistributable, data-free CI suite.
Public Windows/Linux CI additionally runs the Python asset-generator tests.
CI status is linked in the GitHub release; a configured workflow is not a pass.

## Native observations and limits

The user confirmed space flight and shooting on the isolated PAL baseline.
An earlier automated `p084a8` scene recorded 2,925 steady present intervals,
p99 18.194287 ms, maximum 23.017999 ms, zero intervals over 25 ms and zero catch-up
presents. This is a scoped present-timing result, not physical scanout timing,
universal performance or manual controller certification.

The automated scene used an isolated test seed and a separately pinned
diagnostic Garage provider. Publication does not turn that fixture into a
verified ordinary campaign path. No new game was launched for publication.

No complete campaign, current NTSC route, arbitrary mod combination or cold
Home -> space -> campaign -> Home Garage round trip is certified. Garage and
Original-ship consumers/APIs are included; the external provider, its world
content and private cockpit capture are not bundled. Official unmodified
OpenRAC 0.10.0 lacks the required transient scene/audio/destination services.

## Distributed material

- Windows plugin, its manifest, documentation and license notices.
- Corresponding mod/source tools, frozen MIT SDK, GPL engine patch and receipts.
- SHA-256 checksums and a machine-readable release validation record.

No ISO, extracted game art/audio, full engine binary, memory card, save, RAM
dump, SDK 1.15/1.16 candidate, atmospheric-flight code or experimental DualSense
mod is included in the Space Voyages packages. Other existing catalog packages
are reattached byte-for-byte so ordinary discovery keeps offering them.

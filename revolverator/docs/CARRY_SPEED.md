# Carry movement balance

While a live enemy is captured by the Revolverator, Ratchet's horizontal speed
is capped at 50% of the current native ground-speed request. Base and Gold use
the same rule. Normal empty attacks and already launched enemies impose no
carry cost. Launch retains its existing 150 ms recoil; changing equipment or
cancelling the capture removes the carry limit immediately.

The native adapter caps only velocity X/Y after both native movement callbacks.
It does not repeatedly multiply velocity or replace shared speed constants.
Native target/ground-speed requests, vertical physics, platform carry, camera,
progression and save data retain their original ownership.

The [PAL native receipt](validation/pal-carry-speed-hotfix.json) records the exact
DLL/runtime/helper hashes, independent raw-native movement observations and
matched movement windows. Base passes 25/25 checks, Gold 37/37 and equipment
cancellation 26/26. The previous DLL reproduces the missing carry slowdown.
All four accepted fixtures exit 0 with 107 original inputs unchanged each.

Seventy-one carrying samples per corrected scenario have speed ratio 0.5 and
zero cap violations. Matched actual displacement falls from 4.2648311 to
2.1326302 world units/second. After launch the existing nine recoil frames
remain, followed by full native movement; equipment cancellation resumes native
movement in the first tick with no throw recoil.

The carry-only validated DLL is
`6364d5d1e604664eea5a4e6c03ca4bb26258ecc626e2b0c6ec5b87f7883b86fc`.
Its Windows Release suite passes 52/52, including 1672 native-world assertions.
Native proof here covers the named PAL Novalis scenarios. Earlier preview/title
screenshots remain tied to their own preceding DLL receipt.

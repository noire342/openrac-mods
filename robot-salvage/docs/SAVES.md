# Component save ownership

The inventory uses a `.salvage` sidecar next to the native `saveN.bin`.
The mod never rewrites the native packet. The binary codec preserves the full
64 bits of part identities and quantities, including unknown IDs from optional
catalogs. Catalog descriptors, localized provenance, physical dimensions and
stable model keys travel with the snapshot, so reopening a campaign on another
planet does not require killing the original robot again to recover its name.

Each sidecar includes the native campaign packet-prefix length and FNV64
fingerprint, a codec version and a CRC32 over the header/payload. Native
options outside the campaign packet prefix do not invalidate component data.
Catalog and inventory load into temporary objects; neither output changes
until the entire file, catalog and native fingerprint have passed validation.
Unknown IDs are retained. Corrupt, truncated and stale sidecars are rejected.

Writes use an exclusively created temporary in the target directory, flush it,
and atomically replace only the `.salvage` target. An error preserves the prior
sidecar and removes only the writer's own temporary. The codec currently limits
an input file to 64 MiB as an allocation/format safety bound; this is not an
in-game inventory capacity or a quantity limit.

`NativeStore` uses the PAL `SCES_509.16 / BESCES-50916RATCHET` layout. Explicit
`--cards` takes precedence; physical port 1 uses the `.slot2` root suffix.
The PAL active-card/operation/pending/error offsets are independently bound.
`--no-card` suppresses all disk paths.

The parent calls `begin_save` before a native save request and `after_prepare`
after the request. The exact pre-serialization snapshot stays attached to that
pending native operation. A later collection or overlapping declined save does
not replace it. `after_io` commits only after successful native completion;
native I/O failure does not advance the sidecar.

`after_restore_data` accepts only the verified PAL campaign descriptors during
native read operations. The inventory then follows the loaded native file:
missing/rejected sidecars yield an empty inventory for that loaded campaign,
with rejection logged and the source files preserved. The explicit-restore
callback lets the parent end the current world visit. New game clears inventory;
the native new-game-plus path preserves it. Neither operation serializes loose
world pieces. A same-planet respawn does not itself call the explicit-restore
callback.

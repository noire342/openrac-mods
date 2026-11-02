#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit native Clank controller tables in the player's NTSC-U disc, read-only.

Only addresses, sizes, digests and validation results are emitted. No game bytes
are extracted or distributed. The disc/container/overlay layouts follow OpenRAC
d015ea1c6925de90b4be91ddb349af83361d9d4b, port/assets/disc/{toc,level,overlay}.
"""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys


OPENRAC_COMMIT = "d015ea1c6925de90b4be91ddb349af83361d9d4b"
SECTOR_BYTES = 2048
LEVEL_COUNT = 19
MAX_OVERLAY_BYTES = 16 * 1024 * 1024

# Physical addresses from the original SCUS-971.99 overlay sections, ordered 00..18.
ATTACK_ADDRESSES = (
    0x17BC28, 0x17C0A8, 0x17C228, 0x17BD28, 0x17BDA8, 0x17C088,
    0x17C3A8, 0x17BCA8, 0x17C428, 0x17BDA8, 0x17C0A8, 0x17C628,
    0x17C0A8, 0x17BF28, 0x17C328, 0x17C228, 0x17C088, 0x17C528,
    0x17C628,
)
LEDGE_ADDRESSES = (
    0x1C3CF0, 0x1C4170, 0x1C42F0, 0x1C3DF0, 0x1C3E70, 0x1C4170,
    0x1C4470, 0x1C3D70, 0x1C44F0, 0x1C3E70, 0x1C4170, 0x1C46F0,
    0x1C4170, 0x1C3FF0, 0x1C43F0, 0x1C42F0, 0x1C4170, 0x1C45F0,
    0x1C46F0,
)


@dataclass(frozen=True)
class Table:
    name: str
    canonical_address: int
    size: int
    sha256: str
    addresses: tuple
    extent_basis: str


TABLES = (
    Table("attack_records", 0x17BC28, 9 * 44,
          "8bd1e1dffba7bfe8a7098e97ab8ba270e5bdb9b2f6cf7add2e0e4cf618b1c8c7",
          ATTACK_ADDRESSES,
          "Nine 0x2c-byte records; native Clank melee indexes records 7 and 8."),
    Table("speed_bands", 0x17BDB8, 32,
          "60fff9cd48cf76598e50af66d8489891508bbadc39d49a9e942fe6274fe00d26",
          tuple(a + 0x190 for a in ATTACK_ADDRESSES),
          "Two 16-byte SpeedStepThreshold records; PlayerSpeedTuning reads through +0x18."),
    Table("run_jump_animations", 0x17BDD8, 24,
          "9de4a64f9f543bf776b38e5e87fa4bc899b33e105d416e685185a9db0b677ff5",
          tuple(a + 0x1B0 for a in ATTACK_ADDRESSES),
          "24-byte signature from the named byte table up to the next blend table."),
    Table("blend_curves", 0x17BDF0, 3 * 25 * 4,
          "af77ecca1a65ec3553d645cbadd63f2f3dbe2d6d94539cccdac0c8a8e8fe5d0b",
          tuple(a + 0x1C8 for a in ATTACK_ADDRESSES),
          "Three 25-float curves consumed by native blend selectors -1, -2 and -3."),
    Table("ledge_shimmy", 0x1C3CF0, 23 * 4,
          "a5678994d2b8b85712aed84157b7972587a645102dacad8f44f8279ecc1b5117",
          LEDGE_ADDRESSES,
          "Verified consumed range: Clank sequences 11 and 12 each contain 23 frames. "
          "The extern array declaration does not specify its complete extent."),
)


class AuditError(ValueError):
    pass


def word(data, offset):
    if offset < 0 or offset > len(data) - 4:
        raise AuditError("Truncated disc header")
    return struct.unpack_from("<I", data, offset)[0]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def address(value):
    return f"0x{value:08X}"


@dataclass(frozen=True)
class Section:
    address: int
    kind: int
    data: bytes


def sections(overlay):
    """Parse only sections sharing the first entry point, as the native loader does."""
    if len(overlay) < 16:
        raise AuditError("Truncated level overlay")
    first_entry = word(overlay, 12)
    offset = 0
    result = []
    while offset + 16 <= len(overlay):
        dest, size, kind, entry = struct.unpack_from("<IIII", overlay, offset)
        if entry != first_entry:
            break
        offset += 16
        if size > len(overlay) - offset:
            raise AuditError("Overlay section exceeds its container")
        if dest >= 0x02000000 or size > 0x02000000 - dest:
            raise AuditError("Overlay section exceeds guest RAM")
        result.append(Section(dest, kind, overlay[offset:offset + size]))
        offset += size
    if not result:
        raise AuditError("Overlay has no sections")
    return result


def static_range(loaded, at, size):
    for section in loaded:
        # SHT_PROGBITS only: a matching zero run in SHT_NOBITS is not static data.
        if section.kind != 1 or at < section.address:
            continue
        offset = at - section.address
        if offset <= len(section.data) and size <= len(section.data) - offset:
            return section.data[offset:offset + size]
    raise AuditError(f"Static table {address(at)} is outside initialized sections")


def occurrences(loaded, signature):
    found = []
    for section in loaded:
        if section.kind != 1:
            continue
        start = 0
        while True:
            offset = section.data.find(signature, start)
            if offset < 0:
                break
            at = section.address + offset
            if at % 4 == 0:
                found.append(at)
            start = offset + 1
    return sorted(found)


class Disc:
    def __init__(self, path):
        self.path = path
        self.file = None
        self.size = 0

    def __enter__(self):
        self.file = self.path.open("rb")
        self.file.seek(0, 2)
        self.size = self.file.tell()
        return self

    def __exit__(self, *unused):
        self.file.close()

    def read(self, at, size):
        if at < 0 or size < 0 or at > self.size or size > self.size - at:
            raise AuditError("Requested range exceeds the disc image")
        if size > MAX_OVERLAY_BYTES:
            raise AuditError("Requested range exceeds the bounded audit read limit")
        self.file.seek(at)
        data = self.file.read(size)
        if len(data) != size:
            raise AuditError("Incomplete disc image read")
        return data

    def overlay(self, toc, level):
        header = self.read(word(toc, 0x28C8 + level * 8) * SECTOR_BYTES, 0x2434)
        if word(header, 4) != 0x2434:
            raise AuditError(f"Unsupported level {level} header")
        base = word(header, 8) * SECTOR_BYTES
        container_size = word(header, 12) * SECTOR_BYTES
        if base > self.size or container_size > self.size - base:
            raise AuditError("Level container exceeds the disc image")
        data_header = self.read(base, 0x58)
        offset, size = struct.unpack_from("<ii", data_header)
        if offset < 0 or size <= 0 or offset > container_size or size > container_size - offset:
            raise AuditError(f"Invalid level {level} overlay range")
        return self.read(base + offset, size)


def audit(path):
    with Disc(path) as disc:
        toc = disc.read(1500 * SECTOR_BYTES, 0x2960)
        if word(toc, 0) != 1:
            raise AuditError("Unsupported NTSC-U disc table")
        overlays = [disc.overlay(toc, level) for level in range(LEVEL_COUNT)]
        disc_size = disc.size
    loaded = [sections(overlay) for overlay in overlays]
    signatures = {}
    for table in TABLES:
        signature = static_range(loaded[0], table.canonical_address, table.size)
        if digest(signature) != table.sha256:
            raise AuditError(f"Unsupported disc revision: {table.name} digest differs")
        signatures[table.name] = signature
    levels = []
    for level in range(LEVEL_COUNT):
        matches = {}
        for table in TABLES:
            found = occurrences(loaded[level], signatures[table.name])
            expected = table.addresses[level]
            matches[table.name] = {
                "expected_address": address(expected),
                "matching_addresses": [address(at) for at in found],
                "unique_expected_match": found == [expected],
            }
        levels.append({
            "level": level,
            "overlay_size": len(overlays[level]),
            "overlay_sha256": digest(overlays[level]),
            "tables": matches,
            "passed": all(result["unique_expected_match"] for result in matches.values()),
        })
    return {
        "format": 1,
        "openrac_commit": OPENRAC_COMMIT,
        "disc_revision": "NTSC-U SCUS-971.99; verified table signatures",
        "disc_size": disc_size,
        "toc_sha256": digest(toc),
        "coverage": "All 19 original disc overlays, static PROGBITS bytes only; "
                    "this does not verify runtime relocation or gameplay.",
        "tables": [{
            "name": table.name,
            "canonical_address": address(table.canonical_address),
            "bytes": table.size,
            "sha256": table.sha256,
            "extent_basis": table.extent_basis,
        } for table in TABLES],
        "levels": levels,
        "passed": all(level["passed"] for level in levels),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path, help="Explicit path to the player's original disc image")
    parser.add_argument("--json", metavar="OUTPUT", help="Write metadata JSON to OUTPUT; '-' prints JSON")
    args = parser.parse_args()
    try:
        output = Path(args.json).resolve() if args.json and args.json != "-" else None
        if output is not None:
            if output.suffix.lower() != ".json":
                raise AuditError("Metadata output must have a .json extension")
            if output == args.iso.resolve() or (
                    output.exists() and args.iso.exists() and output.samefile(args.iso)):
                raise AuditError("Metadata output must not replace the read-only source image")
        report = audit(args.iso)
        serialized = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.json == "-":
            print(serialized, end="")
        elif output is not None:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(serialized, encoding="utf-8")
        if args.json != "-":
            passed = sum(level["passed"] for level in report["levels"])
            print(f"{passed}/{LEVEL_COUNT} overlays: five unique static table signatures verified")
            if output is not None:
                print(output)
        return 0 if report["passed"] else 1
    except (OSError, AuditError, struct.error) as error:
        print(f"Disc table audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

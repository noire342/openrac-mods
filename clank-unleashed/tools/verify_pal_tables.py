#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only PAL SCES-509.16 v2.00 Clank resource and native table audit.

Standard library only. Emits metadata, never model/animation/texture bytes.
Disc, overlay and WAD layouts follow OpenRAC d015ea1c, port/assets/disc,
Copyright (c) 2026 the OpenRAC contributors, derived from ReRAC (ISC).
See the project's NOTICE.md for the retained third-party license text.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

COMMIT = "d015ea1c6925de90b4be91ddb349af83361d9d4b"
ISO_SIZE = 4214784000
ISO_SHA256 = "0f18a6c84cd8d727ec8c21000a236ed5ce2f279cbb8d0682fa9747199ef73260"
BOOT_SIZE = 1388100
BOOT_SHA1 = "79956931bd62fafd8d20fa2eae796dbaf2e15e83"
BOOT_SHA256 = "17f8a846329fd10798cb97847e9610e5eecd1867668420ed627b2292a96b122b"
CLANK_SHA256 = "5a9c508cff1221e6fccd407d0f210c8ee3bba98a1518186056f1665e55e9e354"
CLANK_FRAMES = [59, 58, 32, 20, 15, 30, 22, 34, 20, 30, 40, 23, 23, 1, 30, 20, 20, 30, 30]
SECTOR = 2048
ATTACK = (
    0x17BD28, 0x17C128, 0x17C2A8, 0x17BDA8, 0x17BE28, 0x17C188,
    0x17C4A8, 0x17BD28, 0x17C4A8, 0x17BE28, 0x17C128, 0x17C6A8,
    0x17C128, 0x17BFA8, 0x17C428, 0x17C2A8, 0x17C108, 0x17C5A8, 0x17C6A8,
)
LEDGE = (
    0x1C3FF0, 0x1C43F0, 0x1C4570, 0x1C4070, 0x1C40F0, 0x1C4470,
    0x1C4770, 0x1C3FF0, 0x1C4770, 0x1C40F0, 0x1C43F0, 0x1C4970,
    0x1C43F0, 0x1C4270, 0x1C46F0, 0x1C4570, 0x1C43F0, 0x1C4870, 0x1C4970,
)
# name, canonical PAL address, verified consumed size, SHA-256, physical addresses.
TABLES = (
    ("attack_records", 0x17BD28, 396,
     "8bd1e1dffba7bfe8a7098e97ab8ba270e5bdb9b2f6cf7add2e0e4cf618b1c8c7", ATTACK),
    ("speed_bands", 0x17BEB8, 32,
     "60fff9cd48cf76598e50af66d8489891508bbadc39d49a9e942fe6274fe00d26",
     tuple(a + 0x190 for a in ATTACK)),
    ("run_jump_animations", 0x17BED8, 24,
     "9de4a64f9f543bf776b38e5e87fa4bc899b33e105d416e685185a9db0b677ff5",
     tuple(a + 0x1B0 for a in ATTACK)),
    ("blend_curves", 0x17BEF0, 300,
     "af77ecca1a65ec3553d645cbadd63f2f3dbe2d6d94539cccdac0c8a8e8fe5d0b",
     tuple(a + 0x1C8 for a in ATTACK)),
    ("ledge_shimmy", 0x1C3FF0, 92,
     "a5678994d2b8b85712aed84157b7972587a645102dacad8f44f8279ecc1b5117", LEDGE),
)


class AuditError(ValueError):
    pass


def word(data, at):
    if at < 0 or at > len(data) - 4:
        raise AuditError("Truncated resource/header word")
    return struct.unpack_from("<I", data, at)[0]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def addr(value):
    return f"0x{value:08X}"


class Disc:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        self.file = self.path.open("rb")
        self.file.seek(0, 2)
        self.size = self.file.tell()
        return self

    def __exit__(self, *unused):
        self.file.close()

    def read(self, at, size):
        if (at < 0 or size < 0 or size > 16 * 1024 * 1024 or
                at > self.size or size > self.size - at):
            raise AuditError("Disc range exceeds the source or bounded read limit")
        self.file.seek(at)
        data = self.file.read(size)
        if len(data) != size:
            raise AuditError("Incomplete disc read")
        return data

    def full_digest(self):
        result = hashlib.sha256()
        self.file.seek(0)
        while data := self.file.read(8 * 1024 * 1024):
            result.update(data)
        return result.hexdigest()

    def level(self, toc, level):
        header = self.read(word(toc, 0x28C8 + level * 8) * SECTOR, 0x2434)
        if word(header, 4) != 0x2434:
            raise AuditError("Unsupported level header signature")
        base, size = word(header, 8) * SECTOR, word(header, 12) * SECTOR
        if base > self.size or size > self.size - base:
            raise AuditError("Level container exceeds the disc image")
        return base, size, self.read(base, 0x58)

    def lump(self, level, field):
        base, size, header = level
        offset, count = struct.unpack_from("<ii", header, field)
        if offset < 0 or count <= 0 or offset > size or count > size - offset:
            raise AuditError("Invalid level lump range")
        return self.read(base + offset, count)

    def identity(self):
        pvd = self.read(16 * SECTOR, SECTOR)
        if pvd[:7] != b"\x01CD001\x01":
            raise AuditError("Source is not a supported ISO9660 disc image")
        directory_size = word(pvd, 166)
        if directory_size > 1024 * 1024:
            raise AuditError("Implausible ISO root directory size")
        root = self.read(word(pvd, 158) * SECTOR, directory_size)
        files, offset = {}, 0
        while offset < len(root):
            size = root[offset]
            if not size:
                offset = (offset // SECTOR + 1) * SECTOR
                continue
            if size < 34 or size > len(root) - offset:
                raise AuditError("Truncated ISO root directory entry")
            rec = root[offset:offset + size]
            name_size = rec[32]
            if name_size > len(rec) - 33:
                raise AuditError("Truncated ISO filename")
            name = rec[33:33 + name_size].decode("ascii", errors="replace").split(";")[0]
            if not rec[25] & 2:
                files[name] = (word(rec, 2) * SECTOR, word(rec, 10))
            offset += size
        if "SYSTEM.CNF" not in files or "SCES_509.16" not in files:
            raise AuditError("Expected PAL boot executable SCES_509.16 is absent")
        config = self.read(*files["SYSTEM.CNF"]).decode("ascii")
        config_values = dict(re.findall(r"^\s*(BOOT2|VER|VMODE)\s*=\s*([^\r\n]+)", config, re.M))
        boot = self.read(*files["SCES_509.16"])
        return {
            "serial": "SCES_509.16", "volume": pvd[40:72].decode("ascii").strip(),
            "config_version": config_values.get("VER", "").strip(),
            "video": config_values.get("VMODE", "").strip(),
            "boot_size": len(boot), "boot_sha1": hashlib.sha1(boot).hexdigest(),
            "boot_sha256": sha(boot),
            "matches_official_openrac_0_10_0_boot": len(boot) == BOOT_SIZE and
                hashlib.sha1(boot).hexdigest() == BOOT_SHA1 and sha(boot) == BOOT_SHA256,
        }


def sections(overlay):
    if len(overlay) < 16:
        raise AuditError("Truncated overlay")
    result, offset, first_entry = [], 0, word(overlay, 12)
    while offset + 16 <= len(overlay):
        base, size, kind, entry = struct.unpack_from("<IIII", overlay, offset)
        if entry != first_entry:
            break
        offset += 16
        if size > len(overlay) - offset or base >= 0x02000000 or size > 0x02000000 - base:
            raise AuditError("Overlay section exceeds its container or guest RAM")
        result.append((base, kind, overlay[offset:offset + size]))
        offset += size
    return result


def static_range(loaded, at, size):
    for base, kind, data in loaded:
        offset = at - base
        if kind == 1 and 0 <= offset <= len(data) and size <= len(data) - offset:
            return data[offset:offset + size]
    raise AuditError(f"Static range {addr(at)} is outside initialized PROGBITS")


def occurrences(loaded, signature):
    result = []
    for base, kind, data in loaded:
        if kind != 1:
            continue
        offset = 0
        while (found := data.find(signature, offset)) >= 0:
            if (base + found) % 4 == 0:
                result.append(base + found)
            offset = found + 1
    return sorted(result)


def wad_decompress(packed):
    if len(packed) < 16 or packed[:3] != b"WAD":
        raise AuditError("Core resource is not a WAD stream")
    size = word(packed, 3)
    if size < 16 or size > len(packed):
        raise AuditError("Truncated WAD stream")
    stream, at, out = packed[16:size], 0, bytearray()
    limit = 32 * 1024 * 1024

    def next_byte():
        nonlocal at
        if at >= len(stream):
            raise AuditError("Read past WAD stream")
        value, at = stream[at], at + 1
        return value

    def literals(count):
        nonlocal at
        if count > len(stream) - at or count > limit - len(out):
            raise AuditError("WAD literal exceeds input/output bounds")
        out.extend(stream[at:at + count])
        at += count

    while at < len(stream):
        flag = next_byte()
        if flag < 0x10:
            literals(next_byte() + 18 if flag == 0 else flag + 3)
            if at < len(stream) and stream[at] < 0x10:
                raise AuditError("Two literal WAD packets in a row")
            continue
        if flag < 0x20:
            length = flag & 7
            if not length:
                length = next_byte() + 7
            little, high = next_byte(), next_byte()
            extra, far = (flag >> 3) & 1, high * 64 + (little >> 2)
            if extra == 0 and far == 0:
                if length != 1:
                    at = (at + 4095) & ~4095
                else:
                    literals(little & 3)
                continue
            distance, length = 0x4000 * (extra + 1) + far, length + 2
        elif flag < 0x40:
            length = flag & 31
            if not length:
                length = next_byte() + 31
            little, high = next_byte(), next_byte()
            distance, length = high * 64 + (little >> 2) + 1, length + 2
        else:
            distance = next_byte() * 8 + ((flag >> 2) & 7) + 1
            length, little = (flag >> 5) + 1, flag
        if distance > len(out) or distance == 0 or length > limit - len(out):
            raise AuditError("WAD match exceeds output bounds")
        start = len(out) - distance
        if length <= distance:
            out.extend(out[start:start + length])
        else:
            repeated = out[start:]
            out.extend((repeated * ((length + distance - 1) // distance))[:length])
        literals(little & 3)
    return out


def clank_resource(disc, level):
    index = disc.lump(level, 0x10)
    core = wad_decompress(disc.lump(level, 0x50))
    bounds, clank = {len(core)}, None
    for at in (8, 12, 16, 20, 80, 84, 88):
        if value := word(index, at):
            bounds.add(value)
    for table, stride in ((0x18, 32), (0x20, 32), (0x28, 48)):
        count, start = word(index, table), word(index, table + 4)
        if count > 2048:
            raise AuditError("Invalid resource class count")
        for i in range(count):
            offset = word(index, start + i * stride)
            if offset:
                bounds.add(offset)
            if table == 0x18 and word(index, start + i * stride + 4) == 87:
                clank = offset
    if table := word(index, 0x78):
        for i in range(256):
            value = word(index, table + i * 4)
            if 0 < value < len(core):
                bounds.add(value)
    count, start = word(index, 0x80), word(index, 0x84)
    if count > 64:
        raise AuditError("Invalid gadget count")
    for i in range(count):
        if value := word(index, start + i * 16):
            bounds.add(value)
    ends = sorted(value for value in bounds if clank is not None and value > clank)
    if clank is None or not ends or ends[0] > len(core) or not 148 <= ends[0] - clank <= 1024 * 1024:
        raise AuditError("Playable Clank class range is absent/invalid")
    blob = core[clank:ends[0]]
    count = blob[0xC]
    if count != 19:
        raise AuditError("Unexpected Clank animation count")
    frames = []
    for i in range(count):
        offset = word(blob, 0x48 + i * 4)
        if offset > len(blob) - 20:
            raise AuditError("Clank animation header exceeds its class")
        frames.append(blob[offset + 16])
    return {
        "source_level": 6, "class_id": 87, "core_bytes": len(core),
        "bytes": len(blob), "sha256": sha(blob), "sequence_count": count,
        "sequence_frames": frames,
        "matches_verified_resource": len(blob) == 493440 and sha(blob) == CLANK_SHA256
            and frames == CLANK_FRAMES,
    }


def audit(path):
    with Disc(path) as disc:
        identity = disc.identity()
        identity.update({"size": disc.size, "sha256": disc.full_digest()})
        identity["matches_official_openrac_0_10_0_disc"] = (
            disc.size == ISO_SIZE and identity["sha256"] == ISO_SHA256)
        if not (identity["matches_official_openrac_0_10_0_disc"] and
                identity["matches_official_openrac_0_10_0_boot"]):
            raise AuditError("Disc/boot differs from the official PAL v2.00 target")
        toc = disc.read(1500 * SECTOR, 0x2960)
        if word(toc, 0) != 1:
            raise AuditError("Unsupported table of contents")
        levels = [disc.level(toc, level) for level in range(19)]
        overlays = [disc.lump(level, 0) for level in levels]
        resource = clank_resource(disc, levels[6])
    loaded = [sections(overlay) for overlay in overlays]
    signatures = {}
    for name, canonical, size, expected, unused in TABLES:
        value = static_range(loaded[0], canonical, size)
        if sha(value) != expected:
            raise AuditError(f"Unexpected PAL table signature: {name}")
        signatures[name] = value
    results = []
    for level in range(19):
        tables = {}
        for name, canonical, size, expected_hash, expected_addresses in TABLES:
            matches = occurrences(loaded[level], signatures[name])
            tables[name] = {
                "expected_address": addr(expected_addresses[level]),
                "matching_addresses": [addr(value) for value in matches],
                "unique_expected_match": matches == [expected_addresses[level]],
            }
        results.append({
            "level": level, "overlay_bytes": len(overlays[level]),
            "overlay_sha256": sha(overlays[level]), "tables": tables,
            "passed": all(value["unique_expected_match"] for value in tables.values()),
        })
    return {
        "format": 1, "game": "rac1", "version": "pal", "disc_revision": "2.00",
        "openrac_commit": COMMIT, "disc": identity, "toc_sha256": sha(toc),
        "clank_resource": resource,
        "tables": [{"name": name, "canonical_address": addr(canonical), "bytes": size,
                    "sha256": expected} for name, canonical, size, expected, unused in TABLES],
        "extent_note": "Ledge signature covers the 23 floats consumed by Clank sequences 11/12; "
                       "the complete extern-array extent is unspecified. All signatures are "
                       "initialized PROGBITS, not BSS or runtime-derived data.",
        "levels": results,
        "coverage_limit": "Static disc/resource/table identity only; runtime relocation and "
                          "native gameplay must be verified separately.",
        "passed": resource["matches_verified_resource"] and all(row["passed"] for row in results),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path, help="Explicit path to the player's PAL v2.00 ISO")
    parser.add_argument("--json", metavar="OUTPUT", help="Write metadata JSON; '-' prints JSON")
    args = parser.parse_args()
    try:
        output = Path(args.json).resolve() if args.json and args.json != "-" else None
        if output is not None:
            if output.suffix.lower() != ".json":
                raise AuditError("Metadata output must have a .json extension")
            if output == args.iso.resolve() or (
                    output.exists() and args.iso.exists() and output.samefile(args.iso)):
                raise AuditError("Metadata output must not replace its read-only source")
        result = audit(args.iso)
        text = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if args.json == "-":
            print(text, end="")
        elif output is not None:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(text, encoding="utf-8")
        if args.json != "-":
            print(f"PAL v2.00: {sum(row['passed'] for row in result['levels'])}/19 overlays, "
                  f"Clank resource verified={result['clank_resource']['matches_verified_resource']}")
            if output is not None:
                print(output)
        return 0 if result["passed"] else 1
    except (OSError, AuditError, struct.error, UnicodeError) as error:
        print(f"PAL audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

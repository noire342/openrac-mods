#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only audit of native PAL Clank hover/glow companion resources.

Uses the adjacent verify_pal_tables.py bounded disc/overlay/WAD readers. Emits
only metadata, addresses and digests; never extracts or writes game asset bytes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

# Importing audit helpers must not create bytecode files beside the source tools.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_pal_tables import (  # noqa: E402
    AuditError, COMMIT, Disc, ISO_SHA256, ISO_SIZE, SECTOR, addr, sections, sha,
    wad_decompress, word,
)


CLASSES = {
    1146: {
        "role": "Clank native hover/rotor attachment",
        "bytes": 11264,
        "sha256": "d778a07bd7bf2e86cab792c5e2c682cbf520e559901a16a4990d8f7a0650aeea",
        "sequence_frames": [1, 30],
        "hero_pointer_offset": "0x1620",
    },
    1204: {
        "role": "Clank native glow companion",
        "bytes": 1920,
        "sha256": "6270af291ad1a05dffeb52889dbcc65464f7bce719e3c65f803e703fefe7235f",
        "sequence_frames": [1],
        "hero_pointer_offset": "0x1624",
    },
}


def resource_boundaries(index, core_size):
    bounds, classes = {core_size}, {}
    for at in (8, 12, 16, 20, 80, 84, 88):
        if value := word(index, at):
            bounds.add(value)
    for table, stride in ((0x18, 32), (0x20, 32), (0x28, 48)):
        count, start = word(index, table), word(index, table + 4)
        if count > 2048:
            raise AuditError("Invalid class resource count")
        for i in range(count):
            offset = word(index, start + i * stride)
            if offset:
                bounds.add(offset)
            if table == 0x18:
                classes[word(index, start + i * stride + 4)] = offset
    if table := word(index, 0x78):
        for i in range(256):
            value = word(index, table + i * 4)
            if 0 < value < core_size:
                bounds.add(value)
    count, start = word(index, 0x80), word(index, 0x84)
    if count > 64:
        raise AuditError("Invalid gadget count")
    for i in range(count):
        if value := word(index, start + i * 16):
            bounds.add(value)
    return bounds, classes


def resource_metadata(core, bounds, offset):
    ends = sorted(value for value in bounds if value > offset)
    if not ends or ends[0] > len(core) or not 80 <= ends[0] - offset <= 1024 * 1024:
        raise AuditError("Companion class exceeds its core resource")
    blob = core[offset:ends[0]]
    count = blob[0xC]
    if count > 64 or 0x48 + 4 * count > len(blob):
        raise AuditError("Invalid companion sequence table")
    frames = []
    for i in range(count):
        seq = word(blob, 0x48 + 4 * i)
        if seq > len(blob) - 20:
            raise AuditError("Companion sequence exceeds its class resource")
        frames.append(blob[seq + 16])
    return {"core_offset": offset, "bytes": len(blob), "sha256": sha(blob),
            "sequence_count": count, "sequence_frames": frames}


def vtable(overlay):
    loaded = sections(overlay)
    if len(loaded) < 7 or loaded[3][1] != 1:
        raise AuditError("Expected seven PAL overlay sections with initialized vtable")
    base, unused_kind, data = loaded[3]
    entries, fallback = {}, None
    for at in range(0, len(data) - 11, 12):
        cls, update, callbacks = struct.unpack_from("<iII", data, at)
        entry = {"entry_address": addr(base + at), "update": addr(update),
                 "callbacks": addr(callbacks)}
        if cls == -1:
            fallback = entry
            break
        entries[cls] = entry
    if fallback is None:
        raise AuditError("Native class vtable lacks its end/fallback record")
    return base, entries, fallback


def glb_metadata(path):
    with path.open("rb") as stream:
        head = stream.read(20)
        if len(head) != 20:
            raise AuditError("Truncated GLB header")
        magic, version, total, size, chunk_type = struct.unpack("<5I", head)
        if (magic != 0x46546C67 or version != 2 or chunk_type != 0x4E4F534A or
                total != path.stat().st_size or size > 4 * 1024 * 1024 or size > total - 20):
            raise AuditError("Unsupported extracted GLB metadata header")
        metadata = json.loads(stream.read(size).decode("utf-8"))
        stream.seek(0)
        digest = hashlib.sha256()
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return {"bytes": total, "sha256": digest.hexdigest(),
            "animation_count": len(metadata.get("animations", [])),
            "animation_names": [value.get("name", "") for value in metadata.get("animations", [])]}


def manifest_metadata(levels, level, cls):
    if levels is None:
        return None
    base = (levels / f"level_{level:02}").resolve()
    manifest = base / "manifest.json"
    if manifest.stat().st_size > 16 * 1024 * 1024:
        raise AuditError("Extracted manifest exceeds audit bounds")
    data = json.loads(manifest.read_text(encoding="utf-8"))
    if data.get("format") != 4 or data.get("version") != "pal" or data.get("level") != level:
        raise AuditError("Expected original PAL format-4 manifest")
    model = data.get("classes", {}).get("moby", {}).get(str(cls))
    if not isinstance(model, dict) or not isinstance(model.get("mesh"), str):
        return {"present": False}
    target = (base / model["mesh"]).resolve()
    if not target.is_relative_to(base):
        raise AuditError("Original extracted mesh path leaves its level directory")
    result = {"present": True, "mesh": model["mesh"], "scale": model.get("scale"),
              "name": model.get("name"), "exists": target.is_file()}
    if result["exists"]:
        result["glb"] = glb_metadata(target)
    return result


def audit(iso, levels):
    with Disc(iso) as disc:
        identity = disc.identity()
        identity.update({"size": disc.size, "sha256": disc.full_digest()})
        if not (disc.size == ISO_SIZE and identity["sha256"] == ISO_SHA256 and
                identity["matches_official_openrac_0_10_0_boot"]):
            raise AuditError("Expected the verified official PAL SCES-509.16 v2.00 target")
        toc = disc.read(1500 * SECTOR, 0x2960)
        if word(toc, 0) != 1:
            raise AuditError("Unsupported disc table")
        results = []
        for level in range(19):
            header = disc.level(toc, level)
            index = disc.lump(header, 0x10)
            core = wad_decompress(disc.lump(header, 0x50))
            bounds, classes = resource_boundaries(index, len(core))
            table_base, entries, fallback = vtable(disc.lump(header, 0))
            companions = {}
            for cls, expected in CLASSES.items():
                offset = classes.get(cls)
                item = {"present_on_disc": offset is not None and offset != 0,
                        "native_vtable_record": entries.get(cls)}
                if item["present_on_disc"]:
                    item["resource"] = resource_metadata(core, bounds, offset)
                    item["matches_verified_resource"] = all(
                        item["resource"][key] == expected[key]
                        for key in ("bytes", "sha256", "sequence_frames"))
                else:
                    item["matches_verified_resource"] = False
                item["extracted_model"] = manifest_metadata(levels, level, cls)
                companions[str(cls)] = item
            result = {"level": level, "native_vtable_address": addr(table_base),
                      "native_vtable_fallback": fallback, "classes": companions}
            result["passed"] = all(
                item["present_on_disc"] and item["matches_verified_resource"] and
                (item["extracted_model"] is None or
                 item["extracted_model"].get("exists", False))
                for item in companions.values())
            results.append(result)
    return {
        "format": 1, "game": "rac1", "version": "pal", "openrac_commit": COMMIT,
        "disc": identity, "classes": {str(cls): value for cls, value in CLASSES.items()},
        "role_evidence": "games/rac1/src/overlays/shared/hero_update_ledge_grab.c, "
                         "update_hero_vehicle_attachments_and_effects: creates 0x47a at hero+0x1620, "
                         "attaches it to Clank joint 5 and blends to sequence 1 during state 0x4f. "
                         "It creates 0x4b4 at hero+0x1624 and tints/positions the glow near joint 7.",
        "source_model_level": 6,
        "levels": results,
        "all_companion_classes_already_present": all(
            item["present_on_disc"] for level in results for item in level["classes"].values()),
        "all_companion_vtable_records_absent": all(
            item["native_vtable_record"] is None
            for level in results for item in level["classes"].values()),
        "capacity": {
            "additional_disc_payload_required_on_verified_levels": 0,
            "hypothetical_fallback_raw_payload_bytes": sum(value["bytes"] for value in CLASSES.values()),
            "hypothetical_fallback_with_64_byte_registration_overhead_per_class":
                sum(value["bytes"] + 64 for value in CLASSES.values()),
            "note": "Original levels already register both classes. Missing native vtable "
                    "records are expected for manually positioned companion effects, not "
                    "evidence that a model is absent. This audit does not measure live heap room.",
        },
        "coverage_limit": "Static disc and optional extracted-model metadata only. Native "
                          "attachment visibility, animation, glide and lifecycle require gameplay proof.",
        "passed": all(level["passed"] for level in results),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path, help="Explicit path to the player's original PAL ISO")
    parser.add_argument("--levels", type=Path, help="Optional original PAL format-4 extraction root")
    parser.add_argument("--json", metavar="OUTPUT", help="Write metadata JSON; '-' prints JSON")
    args = parser.parse_args()
    try:
        output = Path(args.json).resolve() if args.json and args.json != "-" else None
        if output is not None:
            if output.suffix.lower() != ".json":
                raise AuditError("Metadata output must have a .json extension")
            if output == args.iso.resolve() or (
                    output.exists() and args.iso.exists() and output.samefile(args.iso)):
                raise AuditError("Metadata output must not replace its read-only ISO source")
            if args.levels is not None and output.is_relative_to(args.levels.resolve()):
                raise AuditError("Metadata output must stay outside the original level extraction")
        report = audit(args.iso, args.levels)
        text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.json == "-":
            print(text, end="")
        elif output is not None:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(text, encoding="utf-8")
        if args.json != "-":
            passed = sum(level["passed"] for level in report["levels"])
            print(f"PAL companions: {passed}/19 levels passed; no extra disc payload required")
            if output is not None:
                print(output)
        return 0 if report["passed"] else 1
    except (OSError, AuditError, ValueError, struct.error) as error:
        print(f"PAL companion audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

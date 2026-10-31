#!/usr/bin/env python3
"""Inspect finished isolated Golden fixtures; never start/stop game processes."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def analyze(folder: Path) -> dict:
    fixture = json.loads((folder / "fixture.json").read_text(encoding="utf-8-sig"))
    log_path = folder / "data/openrac-rac1-ntsc.log"
    log = log_path.read_text(encoding="utf-8", errors="replace")
    events = []
    for line in log.splitlines():
        if "GOLDEN_QA {" in line:
            events.append(json.loads(line.split("GOLDEN_QA ", 1)[1]))
    results = [e for e in events if e.get("event") == "result"]
    checks = [e for e in events if e.get("event") == "check"]
    final = next((e for e in reversed(events) if e.get("event") == "final_state"), None)
    originals = json.loads((folder / "original-inputs.json").read_text(encoding="utf-8-sig"))
    changed = [p["path"] for p in originals if sha256(Path(p["path"])) != p["sha256"]]
    burst_pattern = re.compile(r"Golden impact: charge=([\d.]+) radius=([\d.]+) damage=([\d.]+) scale=([\d.]+) targets=(\d+)")
    bursts = [dict(zip(("charge", "radius", "damage", "scale", "targets"),
                      (float(m[0]), float(m[1]), float(m[2]), float(m[3]), int(m[4]))))
              for m in burst_pattern.findall(log)]
    purchases = log.count("Golden Revolverator purchased:50000 normal bolts,0 gold bolts; ammunition and XP preserved")
    normal_exit = True if "exited normally" in log.lower() or "normal exit" in log.lower() else None
    # Runtime logs have used both 'Shutting down' and 'exited normally'. An absent
    # marker remains unknown; it is never promoted to process-exit evidence.
    process = None
    process_path = folder / "process-exit.json"
    if process_path.exists():
        process = json.loads(process_path.read_text(encoding="utf-8-sig"))
        normal_exit = process.get("exit_code") == 0
    errors = [line for line in log.splitlines()
              if any(s in line.lower() for s in ("out of memory", "failed to load mod", "access violation", "fatal error"))]
    valid_result = (len(results) == 1 and results[0].get("pass") is True and
                    results[0].get("failures") == 0 and results[0].get("checks") == len(checks) and
                    bool(checks) and all(c.get("pass") is True for c in checks))
    mode = fixture["golden_qa"]["mode"]
    native_events = [e for e in events if e.get("event") in
                     ("native_accepted_hit", "native_death", "native_purchase_boundary", "released", "switch_requested_after_release", "exploded")]
    combat_evidence = True
    if mode not in {"shop", "save", "reload"}:
        accepted = [e for e in events if e.get("event") == "native_accepted_hit"]
        combat_evidence = (len(bursts) == 1 and len(accepted) >= 2 and
                           all(e.get("origin") == 2017 and e.get("flags") == 0x830000 for e in accepted) and
                           any(e.get("event") == "native_death" for e in events) and
                           any(e.get("event") == "released" for e in events))
    passed = (valid_result and combat_evidence and final is not None and
              final.get("golden") == 1 and final.get("capacity") == 20 and
              not changed and not errors and purchases == (0 if mode == "reload" else 1) and normal_exit is True)
    return {
        "format": 1,
        "scenario": mode,
        "passed": passed,
        "native_gameplay_verified": passed,
        "full_campaign_verified": False,
        "artifacts": fixture["artifacts"],
        "result": results[0] if results else None,
        "checks": checks,
        "final_state": final,
        "native_bursts": bursts,
        "native_purchase_poll_events": purchases,
        "normal_process_exit_observed": normal_exit,
        "original_input_count": len(originals),
        "original_inputs_unchanged": not changed,
        "changed_originals": changed,
        "native_errors": errors,
        "log_sha256": sha256(log_path),
        "fixture_seeded": fixture["golden_qa"]["seeds"],
        "never_seeded": fixture["golden_qa"]["never_seeded"],
        "native_events": native_events,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.fixture.resolve())
    encoded = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

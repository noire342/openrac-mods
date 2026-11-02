#!/usr/bin/env python3
"""Package auditable standalone sources, without SDK/build/game/private files."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def sources():
    files = {}
    for name in ("CMakeLists.txt", "LICENSE", "NOTICE.md", "README.md", "openrac-mod.json",
                 "sdk.lock", ".clang-format", "compatibility.json"):
        files[name] = ROOT / name
    for directory in ("src", "tests", "tools", "docs"):
        for path in (ROOT / directory).rglob("*"):
            if path.is_file() and path.suffix in {".hpp", ".h", ".cpp", ".c", ".py", ".md", ".json", ".in"}:
                files[path.relative_to(ROOT).as_posix()] = path
    files["third_party/SNAPSHOT.json"] = ROOT / "third_party/SNAPSHOT.json"
    utility = ROOT / "third_party/file-overlay"
    if not utility.is_dir():
        utility = ROOT.parent / "arsenal-file-overlay"
    for name in ("src/overlay_core.cpp", "src/overlay_core.hpp", "src/windows_overlay.cpp",
                 "src/windows_overlay.hpp", "include/file_overlay_api.h", "LICENSE",
                 "docs/LICENSE-MIT.md", "third_party/minhook.lock.json"):
        files[f"third_party/file-overlay/{name}"] = utility / name
    for path in (utility / "third_party/minhook").rglob("*"):
        if path.is_file():
            files[f"third_party/file-overlay/{path.relative_to(utility).as_posix()}"] = path
    native = ROOT / "third_party/openrac"
    if not native.is_dir():
        native = ROOT.parent / "revolverator/third_party/openrac"
    for name in ("common/json.h", "common/json.cpp", "NOTICE.md"):
        files[f"third_party/openrac/{name}"] = native / name
    return {name: path.read_bytes() for name, path in sorted(files.items())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = sources()
    data["SOURCE-SNAPSHOT.json"] = (json.dumps({
        "format": 1, "files": {name: hashlib.sha256(blob).hexdigest() for name, blob in data.items()}
    }, indent=2) + "\n").encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as out:
        for name, blob in sorted(data.items()):
            info = zipfile.ZipInfo("clank-unleashed/" + name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            out.writestr(info, blob)
    print(f"{args.output}: {len(data)} source files")

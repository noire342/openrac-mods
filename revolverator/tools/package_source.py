#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
"""Create an audited PRIVATE source snapshot; this tool never publishes it."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT_FILES = {'CMakeLists.txt', 'openrac-mod.json', 'README.md', 'LICENSE', 'COPYING'}
ROOT_DIRS = {'src', 'include', 'docs', 'tests', 'tools', 'examples'}
SUFFIXES = {'.c', '.cpp', '.h', '.hpp', '.py', '.ps1', '.md', '.json', '.txt', '.cmake'}
PRIVATE_ASSET_NAMES = {'revolverator_hd_icon_data.hpp'}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def snapshot(source: Path) -> dict[str, bytes]:
    source = source.resolve(strict=True)
    files = {}
    for path in sorted(source.rglob('*')):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        parts = relative.parts
        selected = (len(parts) == 1 and path.name in ROOT_FILES) or (
            parts[0] in ROOT_DIRS and path.suffix.lower() in SUFFIXES) or (
            parts[:2] == ('assets', 'ammunition') and
            len(parts) == 3 and path.name in {'recipe.json', 'README.md'})
        if not selected or '__pycache__' in parts or path.name in PRIVATE_ASSET_NAMES:
            continue
        if path.is_symlink() or source not in path.resolve(strict=True).parents:
            raise ValueError(f'External or symbolic source path rejected: {relative}')
        files[relative.as_posix()] = path.read_bytes()
    required = {'CMakeLists.txt', 'openrac-mod.json', 'include/revolverator_api.h', 'src/mod.cpp'}
    if not required.issubset(files):
        raise ValueError('The source tree is missing required mod files')
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    output = args.output.resolve()
    if source in output.parents:
        raise ValueError('Write the private archive outside the source tree')
    if output.exists():
        raise ValueError('Refusing to overwrite an existing source snapshot')
    files = snapshot(source)
    inventory = {name: {'bytes': len(data), 'sha256': digest(data)}
                 for name, data in files.items()}
    receipt = {'format': 1, 'publication': 'PRIVATE; not approved for release',
        'files': inventory, 'excluded': ['SDK/toolchains', 'binaries', 'game artwork',
            'derived icon headers', 'RAM/save data', 'local QA fixtures', 'credentials']}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr('revolverator/'+name, data)
        archive.writestr('SOURCE-SNAPSHOT.json', json.dumps(receipt, indent=2)+'\n')
    # The shared working tree can change during another agent's build. Reject a
    # snapshot if any selected source changed while we were writing the archive.
    if snapshot(source) != files:
        output.unlink()
        raise RuntimeError('Source changed during packaging; no archive retained')
    print(json.dumps({'publication': 'PRIVATE', 'files': len(files),
        'archive': str(output), 'sha256': digest(output.read_bytes())}, indent=2))


if __name__ == '__main__':
    main()

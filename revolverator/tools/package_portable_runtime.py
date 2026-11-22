#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Package the SDK build, authored ammunition and local preparation scripts."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

from package_runtime_release import AUTHORED_ART, PREPARERS, archive, digest, document, inventory, read


PLATFORMS = {
    'linux-x86_64': ('.so', 62),
    'linux-aarch64': ('.so', 183),
    'android-arm64': ('.so', 183),
    'macos-x86_64': ('.dylib', 0x01000007),
    'macos-arm64': ('.dylib', 0x0100000C),
}


def android_note(blob: bytes) -> bool:
    """Read the NDK's PT_NOTE instead of assuming every AArch64 ELF uses Bionic."""
    offset = struct.unpack_from('<Q', blob, 32)[0]
    length, count = struct.unpack_from('<HH', blob, 54)
    if length != 56 or offset + count * length > len(blob):
        return False
    for header in range(offset, offset + count * length, length):
        if struct.unpack_from('<I', blob, header)[0] != 4:  # PT_NOTE
            continue
        start = struct.unpack_from('<Q', blob, header + 8)[0]
        end = start + struct.unpack_from('<Q', blob, header + 32)[0]
        if end > len(blob):
            return False
        while start + 12 <= end:
            name_size, data_size, note_type = struct.unpack_from('<III', blob, start)
            name_at = start + 12
            data_at = name_at + (name_size + 3) // 4 * 4
            start = data_at + (data_size + 3) // 4 * 4
            if start <= end and note_type == 1 and data_size >= 4:
                if blob[name_at:name_at + name_size] == b'Android\0':
                    return True
    return False


def check_library(blob: bytes, platform: str) -> None:
    suffix, machine = PLATFORMS[platform]
    if suffix == '.so':
        valid = (len(blob) >= 64 and blob[:7] == b'\x7fELF\x02\x01\x01'
                 and struct.unpack_from('<HH', blob, 16) == (3, machine))
    else:
        valid = (len(blob) >= 32 and blob[:4] == b'\xcf\xfa\xed\xfe'
                 and struct.unpack_from('<I', blob, 4)[0] == machine
                 and struct.unpack_from('<I', blob, 12)[0] == 6)
    if not valid:
        raise ValueError(f'Wrong shared library format or architecture for {platform}')
    if platform in ('android-arm64', 'linux-aarch64') and android_note(blob) != (platform == 'android-arm64'):
        raise ValueError(f'NDK ELF ABI note does not match {platform}')


def collect(module: Path, source: Path) -> tuple[str, str, dict[str, bytes]]:
    manifest_bytes = read(module, 'openrac-mod.json')
    manifest = json.loads(manifest_bytes)
    original = document(source / 'openrac-mod.json')
    for key in ('format', 'id', 'version', 'author', 'license', 'games'):
        if manifest.get(key) != original.get(key):
            raise ValueError(f'Generated/source manifest mismatch: {key}')
    if manifest['format'] != 2 or manifest['id'] != 'revolverator':
        raise ValueError('Expected a generated format-2 Revolverator manifest')
    if manifest.get('api') != '1.20':
        raise ValueError('The portable manifest must declare OpenRAC mod interface 1.20')
    arsenal = manifest['extensions']['noire342.arsenal']
    if manifest.get('assets') != 'generated-levels' or 'bundledUtilities' in arsenal:
        raise ValueError('Expected the official loader asset path in the portable manifest')
    if 'rac1.vendor.icon_strip.draw' not in arsenal.get('compatibilityHooks', []):
        raise ValueError('The portable manifest must declare the official Gadgetron icon-strip hook')

    plugins = manifest.get('plugin', {})
    if not isinstance(plugins, dict) or len(plugins) != 1 or next(iter(plugins)) not in PLATFORMS:
        raise ValueError('Expected exactly one SDK-generated portable platform')
    platform = next(iter(plugins))
    plugin = f'lib/{platform}/revolverator{PLATFORMS[platform][0]}'
    if plugins[platform] != plugin:
        raise ValueError(f'Unexpected plugin library path for {platform}')
    library = read(module, plugin)
    check_library(library, platform)
    files = {'openrac-mod.json': manifest_bytes, plugin: library}

    for name, expected in AUTHORED_ART.items():
        blob = read(module, name)
        if digest(blob) != expected:
            raise ValueError(f'Authored ammunition provenance differs: {name}')
        files[name] = blob
    for name in ('README.md', 'LICENSE', *(f'tools/{name}' for name in PREPARERS)):
        blob = read(module, name)
        if blob != read(source, name):
            raise ValueError(f'Build stage is stale: {name}')
        files[name] = blob
    return manifest['version'], platform, files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True, help='SDK-generated package/revolverator directory')
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    version, platform, files = collect(args.module.resolve(strict=True), args.source.resolve(strict=True))
    args.output.mkdir(parents=True, exist_ok=True)
    result = args.output / f'revolverator-{version}-{platform}.zip'
    archive(result, files)
    print(json.dumps({
        'archive': result.name,
        'sha256': digest(result.read_bytes()),
        'files': inventory(files),
    }, indent=2))


if __name__ == '__main__':
    main()

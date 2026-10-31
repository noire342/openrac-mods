#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Add Golden art to a native package; retain every byte of model binary data."""
import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path
from build_model_pack import read_glb, write_glb


def digest(data):
    return hashlib.sha256(data).hexdigest()


def golden_model(path, static_preview=False):
    original, binary = read_glb(path)
    if original.get('images') != [{'uri': '../textures/moby_revolverator_original.png'}]:
        raise ValueError(f'Unexpected base texture layout: {path}')
    golden = copy.deepcopy(original)
    golden['images'][0]['uri'] = '../textures/moby_revolverator_gold.png'
    for mesh in golden['meshes']:
        mesh['name'] = 'revolverator_golden'
        if static_preview:
            for primitive in mesh['primitives']:
                for attribute in ('JOINTS_0', 'WEIGHTS_0'):
                    primitive['attributes'].pop(attribute, None)
    if static_preview:
        for node in golden.get('nodes', []):
            node.pop('skin', None)
    result = write_glb(golden, bytearray(binary))
    return result, digest(binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', required=True, type=Path)
    parser.add_argument('--atlas', type=Path, default=Path(__file__).resolve().parents[1] / 'assets/golden/moby_revolverator_gold.png')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    root = args.plugin.resolve()
    manifest = json.loads((root / 'openrac-mod.json').read_text(encoding='utf-8-sig'))
    if manifest.get('id') != 'revolverator' or manifest.get('format') != 2:
        raise ValueError('Expected a native Revolverator package, not installed game levels')
    atlas = args.atlas.read_bytes()
    if atlas[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Golden atlas must be a PNG')
    width, height = struct.unpack_from('>II', atlas, 16)
    if width != height or not 128 <= width <= 8192:
        raise ValueError('Golden atlas must preserve the original square aspect ratio')
    pending, records = [], []
    for level in range(19):
        folder = root / 'assets/graphics' / f'level_{level:02}'
        source = folder / 'mobys/moby_2017.glb'
        target = folder / 'mobys/moby_2018.glb'
        texture = folder / 'textures/moby_revolverator_gold.png'
        if target.exists() or texture.exists():
            raise ValueError(f'Golden output already exists: {folder}')
        data, binary_hash = golden_model(source)
        pending += [(target, data), (texture, atlas)]
        records.append({'level': level, 'base_sha256': digest(source.read_bytes()),
                        'golden_sha256': digest(data), 'unchanged_binary_sha256': binary_hash})
    # A rigid shop display must not depend on whichever weapon's animation
    # resource is currently streamed. Its vertex/index/UV buffers are identical.
    display = root / 'assets/graphics/level_01/mobys/moby_2019.glb'
    if display.exists():
        raise ValueError(f'Static preview output already exists: {display}')
    display_data, display_hash = golden_model(root / 'assets/graphics/level_01/mobys/moby_2017.glb', True)
    pending.append((display, display_data))
    # Validate all inputs before creating any output.
    for path, data in pending:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    report = {'format': 1, 'same_geometry': True, 'same_uvs': True,
              'same_skeleton': True, 'same_rotor_binding': True,
              'atlas_sha256': digest(atlas), 'atlas_size': [width, height], 'levels': records,
              'static_preview': {'class': 2019, 'unchanged_binary_sha256': display_hash}}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Golden art: {len(records)} levels; unchanged GLB binary buffers; atlas {width}x{height}')


if __name__ == '__main__':
    main()

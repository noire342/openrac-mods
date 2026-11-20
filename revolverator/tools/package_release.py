#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Create a standard OpenRAC Revolverator ZIP and its corresponding source ZIP.

The native archive has openrac-mod.json at its root. Only the built Windows
plugin, its two private utility DLLs, base/Golden assets and notices are distributed.
No client EXE, wrapper launcher, staged manifest, card or ISO is included.
The tool neither installs nor starts the game, and never publishes artifacts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import struct
import zipfile
from pathlib import Path, PurePosixPath, PureWindowsPath

from package_source import ROOT_DIRS, ROOT_FILES, SUFFIXES, PRIVATE_ASSET_NAMES

STOCK_SHA256_BY_REGION = {
    'pal': 'e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e',
    'ntsc': 'f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b',
}
LIBRARIES = {
    'windows_dll_sha256': 'lib/windows-x86_64/revolverator.dll',
    'compat_dll_sha256': 'lib/windows-x86_64/arsenal_compat.dll',
    'overlay_dll_sha256': 'lib/windows-x86_64/arsenal_file_overlay.dll',
}
IGNORED_DIRS = {'.git', '__pycache__', 'build', '.profile', '.sessions'}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + '\n').encode('utf-8')


def member(value: str) -> str:
    value = str(value).replace('\\', '/')
    if ('\0' in value or ':' in value or PureWindowsPath(value).drive
            or PurePosixPath(value).is_absolute()
            or any(part in {'', '.', '..'} for part in value.split('/'))):
        raise ValueError(f'Unsafe archive member: {value!r}')
    return PurePosixPath(value).as_posix()


def is_link(path: Path) -> bool:
    try:
        status = path.lstat()
    except FileNotFoundError:
        return False
    return (stat.S_ISLNK(status.st_mode)
            or bool(getattr(status, 'st_file_attributes', 0) & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0)))


def reject_link_ancestors(path: Path):
    for ancestor in (path, *path.parents):
        if is_link(ancestor):
            raise ValueError(f'Symbolic/junction path rejected: {ancestor}')


def root_path(path: Path) -> Path:
    path = path.absolute()
    reject_link_ancestors(path)
    if is_link(path) or not path.is_dir():
        raise ValueError(f'Input root must be a real directory: {path}')
    return path.resolve(strict=True)


def checked_path(root: Path, relative: str) -> Path:
    relative = member(relative)
    path = root
    for part in PurePosixPath(relative).parts:
        path /= part
        if is_link(path):
            raise ValueError(f'Symbolic/junction input rejected: {relative}')
    resolved = path.resolve(strict=True)
    if not resolved.is_relative_to(root) or not resolved.is_file():
        raise ValueError(f'Input escapes its root or is not a file: {relative}')
    return resolved


class Inputs:
    def __init__(self):
        self.files = {}
        self.inventory = {}

    def read(self, root: Path, relative: str, label: str) -> bytes:
        path = checked_path(root, relative)
        data = path.read_bytes()
        record = {'bytes': len(data), 'sha256': sha(data)}
        if path in self.files and self.files[path][2] != record:
            raise RuntimeError(f'Input changed while reading: {label}')
        self.files[path] = (root, relative, record)
        self.inventory[member(label)] = record
        return data

    def verify(self):
        for path, (root, relative, expected) in self.files.items():
            if checked_path(root, relative) != path:
                raise RuntimeError(f'Input moved during packaging: {relative}')
            data = path.read_bytes()
            if {'bytes': len(data), 'sha256': sha(data)} != expected:
                raise RuntimeError(f'Input changed during packaging: {relative}')


def tree_files(root: Path, source=False):
    for current, directories, files in os.walk(root, followlinks=False):
        current = Path(current)
        kept = []
        for name in sorted(directories):
            path = current / name
            if source and name in IGNORED_DIRS:
                continue
            if is_link(path):
                raise ValueError(f'Symbolic/junction directory rejected: {path.relative_to(root)}')
            kept.append(name)
        directories[:] = kept
        for name in sorted(files):
            path = current / name
            if is_link(path):
                raise ValueError(f'Symbolic input rejected: {path.relative_to(root)}')
            if not stat.S_ISREG(path.stat().st_mode):
                raise ValueError(f'Non-regular input rejected: {path.relative_to(root)}')
            yield path.relative_to(root).as_posix()


def put(entries: dict[str, bytes], name: str, data: bytes):
    name = member(name)
    if any(existing.casefold() == name.casefold() for existing in entries):
        raise ValueError(f'Duplicate archive member: {name}')
    if PurePosixPath(name).suffix.lower() in {'.exe', '.iso', '.bin', '.arsenal', '.pyc'}:
        raise ValueError(f'Forbidden distributable file: {name}')
    entries[name] = data


def read_json(inputs: Inputs, root: Path, relative: str, label: str):
    return json.loads(inputs.read(root, relative, label).decode('utf-8-sig'))


def validate_regions(games):
    regions = games.get('rac1') if isinstance(games, dict) and set(games) == {'rac1'} else None
    if (not isinstance(regions, list) or len(regions) != 2
            or any(type(region) is not str for region in regions)
            or len(set(regions)) != len(regions)
            or set(regions) != set(STOCK_SHA256_BY_REGION)):
        raise ValueError('Expected exactly PAL and NTSC in games.rac1, without duplicates or other games')


def validate_manifest(manifest):
    if not isinstance(manifest, dict):
        raise ValueError('Expected a format2 manifest object')
    validate_regions(manifest.get('games'))
    if (manifest.get('format') != 2 or manifest.get('id') != 'revolverator'
            or manifest.get('version') != '0.3.1' or manifest.get('license') != 'GPL-3.0-or-later'
            or manifest.get('plugin') != {'windows-x86_64': LIBRARIES['windows_dll_sha256']}
            or manifest.get('requires')):
        raise ValueError('Expected standalone Windows PAL/NTSC Revolverator 0.3.1 format2 package')
    hooks = manifest.get('hooks', [])
    extension = manifest.get('extensions', {}).get('noire342.arsenal', {})
    if (len(hooks) != 27 or len(set(hooks)) != 27
            or set(hooks).intersection(extension.get('compatibilityHooks', []))
            or set(extension.get('bundledUtilities', [])) != {'arsenal_compat.dll', 'arsenal_file_overlay.dll'}):
        raise ValueError('Expected27 stock SDK hooks and two bundled utilities, without external utility dependencies')


def validate_dll(data: bytes, relative: str):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'Not a Windows DLL: {relative}')
    offset = struct.unpack_from('<I', data, 60)[0]
    if offset + 26 > len(data) or data[offset:offset+4] != b'PE\0\0':
        raise ValueError(f'Invalid PE image: {relative}')
    machine = struct.unpack_from('<H', data, offset+4)[0]
    characteristics = struct.unpack_from('<H', data, offset+22)[0]
    magic = struct.unpack_from('<H', data, offset+24)[0]
    if machine != 0x8664 or magic != 0x20b or not characteristics & 0x2000:
        raise ValueError(f'Expected a Windows x64 DLL, not a client EXE: {relative}')


def inventory(entries):
    return {name: {'bytes': len(data), 'sha256': sha(data)} for name, data in sorted(entries.items())}


def collect_source(inputs, root, identifier, expected_version):
    info = read_json(inputs, root, 'openrac-mod.json', f'source/{identifier}/openrac-mod.json')
    if (info.get('id') != identifier or info.get('version') != expected_version
            or info.get('license') != 'GPL-3.0-or-later'):
        raise ValueError(f'Corresponding source identity/version/license mismatch: {identifier}')
    if identifier == 'revolverator':
        validate_regions(info.get('games'))
    selected = {}
    for relative in tree_files(root, source=True):
        path = PurePosixPath(relative)
        parts = path.parts
        keep = (len(parts) == 1 and path.name in ROOT_FILES) or (
            parts[0] in ROOT_DIRS and (path.suffix.lower() in SUFFIXES or path.name in {'LICENSE', 'COPYING', 'NOTICE', 'AUTHORS'})) or (
            parts[:2] == ('assets', 'ammunition') and len(parts) == 3 and path.name in {'recipe.json', 'README.md'})
        keep = keep or (parts[:2] == ('assets', 'golden') and len(parts) == 3
                         and path.name in {'moby_revolverator_gold.png', 'README.md'})
        if not keep or path.name in PRIVATE_ASSET_NAMES or '__pycache__' in parts:
            continue
        put(selected, f'{identifier}/{relative}', inputs.read(root, relative, f'source/{identifier}/{relative}'))
    entry_source = 'src/weapon_xp.cpp' if identifier == 'weapon-xp' else 'src/mod.cpp'
    required = {f'{identifier}/{name}' for name in ('CMakeLists.txt', 'openrac-mod.json', 'LICENSE', 'README.md', entry_source)}
    if not required.issubset(selected):
        raise ValueError(f'Incomplete corresponding source for {identifier}')
    if identifier == 'revolverator':
        required_json = {f'{identifier}/third_party/openrac/{name}' for name in ('common/json.cpp', 'common/json.h', 'NOTICE.md')}
        if not required_json.issubset(selected):
            raise ValueError('The GPL JSON reader and its provenance must be included in Revolverator source')
    return selected


def validate_minhook(inputs, root, identifier):
    lock = read_json(inputs, root, 'third_party/minhook.lock.json', f'source/{identifier}/third_party/minhook.lock.json')
    if lock.get('version') != '1.3.4' or set(lock.get('files', [])) != set(lock.get('file_sha256', {})):
        raise ValueError(f'Incomplete pinned MinHook source inventory: {identifier}')
    for relative, expected in lock['file_sha256'].items():
        relative = 'third_party/minhook/' + member(relative)
        if sha(inputs.read(root, relative, f'source/{identifier}/{relative}')) != expected:
            raise ValueError(f'Pinned MinHook source checksum mismatch: {identifier}/{relative}')


def write_archive(path, entries, owned):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as handle:
        owned.append(path)
        with zipfile.ZipFile(handle, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in sorted(entries.items()):
                info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                archive.writestr(info, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--compat-source', type=Path, required=True)
    parser.add_argument('--overlay-source', type=Path, required=True)
    parser.add_argument('--xp-source', '--weapon-xp-source', dest='xp_source', type=Path, help='Optional corresponding Weapon XP companion source; no XP binary is bundled')
    icon = parser.add_mutually_exclusive_group(required=True)
    icon.add_argument('--hd-icon-data', type=Path, help='Exact generated header used by the HD-enabled DLL; included in corresponding source')
    icon.add_argument('--native-icon-fallback', action='store_true', help='Explicit assertion that the DLL was built without an HD header')
    parser.add_argument('--validation', type=Path, help='Optional native aggregate binding all three DLL hashes and the stock client')
    parser.add_argument('--reference-region', choices=tuple(STOCK_SHA256_BY_REGION), default='pal',
                        help='Region whose official OpenRAC0.10 client and native proof are certified; default PAL')
    parser.add_argument('--source-commit', default='', help='Optional source revision; no publication is performed')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-out', type=Path, help='Separate source ZIP; defaults to <out stem>-sources.zip')
    args = parser.parse_args()
    reference_region = args.reference_region
    runtime_sha256 = STOCK_SHA256_BY_REGION[reference_region]
    plugin, source = root_path(args.plugin), root_path(args.source)
    compat, overlay = root_path(args.compat_source), root_path(args.overlay_source)
    xp = root_path(args.xp_source) if args.xp_source else None
    output = args.out.absolute()
    source_out = (args.source_out or output.with_name(output.stem+'-sources.zip')).absolute()
    receipt_out = output.with_name(output.name+'.receipt.json')
    roots = [plugin, source, compat, overlay] + ([xp] if xp else [])
    destinations = [output, source_out, receipt_out]
    if len({path.resolve() for path in destinations}) != len(destinations):
        raise ValueError('Binary, source and receipt destinations must be distinct')
    for path in destinations:
        reject_link_ancestors(path)
        if path.exists() or is_link(path) or any(path.resolve().is_relative_to(root) for root in roots):
            raise ValueError(f'Refusing existing/output-inside-input destination: {path}')
    inputs = Inputs()
    manifest = read_json(inputs, plugin, 'openrac-mod.json', 'plugin/openrac-mod.json')
    validate_manifest(manifest)
    expected = {'openrac-mod.json', 'README.md', 'LICENSE', *LIBRARIES.values(),
                'assets/ammunition/model.glb', 'assets/ammunition/texture.png'}
    for level in range(19):
        expected |= {f'assets/graphics/level_{level:02}/mobys/moby_2017.glb',
                     f'assets/graphics/level_{level:02}/textures/moby_revolverator_original.png',
                     f'assets/graphics/level_{level:02}/mobys/moby_2018.glb',
                     f'assets/graphics/level_{level:02}/textures/moby_revolverator_gold.png'}
    expected.add('assets/graphics/level_01/mobys/moby_2019.glb')
    actual = set(tree_files(plugin))
    if actual != expected:
        raise ValueError(f'Built package allowlist mismatch: missing={sorted(expected-actual)}, unexpected={sorted(actual-expected)}')
    binary = {}
    for relative in sorted(expected):
        data = inputs.read(plugin, relative, 'plugin/'+relative)
        if relative in LIBRARIES.values():
            validate_dll(data, relative)
        elif relative.endswith('.glb'):
            if len(data) < 12 or data[:4] != b'glTF' or struct.unpack_from('<II', data, 4) != (2, len(data)):
                raise ValueError(f'Invalid packaged GLB: {relative}')
        elif relative.endswith('.png') and not data.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError(f'Invalid packaged PNG: {relative}')
        put(binary, relative, data)
    sources = collect_source(inputs, source, 'revolverator', '0.3.1')
    if binary['LICENSE'] != sources['revolverator/LICENSE']:
        raise ValueError('The built license does not match its corresponding source')
    for identifier, root in [('arsenal-compat', compat), ('arsenal-file-overlay', overlay)]:
        sources.update(collect_source(inputs, root, identifier, '0.1.0'))
        validate_minhook(inputs, root, identifier)
        notices = {'README.md': 'README.md', 'LICENSE': 'LICENSE', 'docs/LICENSE-MIT.md': 'LICENSE-MIT.md',
                   'third_party/minhook/LICENSE.txt': 'MinHook-LICENSE.txt', 'third_party/minhook/AUTHORS.txt': 'MinHook-AUTHORS.txt'}
        header = 'arsenal_hooks_api.h' if identifier == 'arsenal-compat' else 'file_overlay_api.h'
        notices['include/'+header] = header
        for relative, name in notices.items():
            put(binary, f'THIRD_PARTY_NOTICES/{identifier}/{name}', inputs.read(root, relative, f'source/{identifier}/{relative}'))
    if xp:
        sources.update(collect_source(inputs, xp, 'weapon-xp', '0.6.4'))
    put(binary, 'THIRD_PARTY_NOTICES/OpenRAC-JSON/NOTICE.md', inputs.read(source, 'third_party/openrac/NOTICE.md', 'source/revolverator/third_party/openrac/NOTICE.md'))
    put(binary, 'THIRD_PARTY_NOTICES/README.md', (
        '# Bundled private utility libraries\n\n'
        'Revolverator loads arsenal_compat.dll and arsenal_file_overlay.dll from its own DLL directory.\n'
        'No separate compatibility-plugin installation, replacement client or external launcher is needed.\n'
        'Both implementations are GPL-3.0-or-later; their complete corresponding sources accompany this release.\n'
        'Each utility embeds pinned MinHook1.3.4; BSD/HDE notices and author lists are retained here.\n'
        'Only the interoperability headers marked MIT use the accompanying MIT permission notice.\n'
        'The OpenRAC JSON reader retains its original GPL copyright and provenance notice.\n\n'
        'Game-derived drill artwork/glyphs remain the property of their original rights holders.\n'
        'The ammunition model/texture were created with AI assistance. Software licenses do not relicense game art.\n'
        'Reference region: '+reference_region+'; reference OpenRAC0.10 client SHA256: '+runtime_sha256+'\n'
        'Supported OpenRAC0.10 installed clients: '+', '.join(region+' '+digest for region, digest in STOCK_SHA256_BY_REGION.items())+'\n'
        'Native gameplay verification in the release receipt applies only to its selected reference region.\n'
    ).encode())
    if args.hd_icon_data:
        hd = args.hd_icon_data.absolute()
        if hd.name != 'revolverator_hd_icon_data.hpp':
            raise ValueError('Expected the exact generated revolverator_hd_icon_data.hpp')
        hd_root = root_path(hd.parent)
        put(sources, 'revolverator/generated/'+hd.name, inputs.read(hd_root, hd.name, 'hd-icon/'+hd.name))
        put(sources, 'revolverator/generated/NOTICE.txt', b'Corresponding generated artwork input for the HD-enabled DLL. Derived native glyph upscaled with Real-ESRGAN. Original game artwork is not relicensed by the software GPL.\n')
    put(sources, 'revolverator/BUILD-CONFIG.txt', (
        'Build with C++20, CMake Release x64 and a separately obtained OpenRAC Mod SDK1.3.0.\n'
        + ('Set -DREVOLVERATOR_HD_ICON_DIR=<revolverator source>/generated.\n' if args.hd_icon_data else 'Explicitly declared native glyph fallback; no HD header input.\n')
        + 'Build arsenal-compat and arsenal-file-overlay separately with their included CMakeLists and vendored MinHook.\n'
        'The two utility DLLs belong beside revolverator.dll in lib/windows-x86_64.\n'
        'The binary ZIP carries the separately licensed artwork; the source builders/recipe explain its reconstruction.\n'
    ).encode())
    hashes = {key: sha(binary[relative]) for key, relative in LIBRARIES.items()}
    verified = False
    validation_summary = None
    if args.validation:
        validation_path = args.validation.absolute()
        validation = read_json(inputs, root_path(validation_path.parent), validation_path.name, 'validation/native-aggregate.json')
        if not isinstance(validation, dict):
            raise ValueError('Native aggregate must be an object')
        for key in ('reference_region', 'region'):
            if key in validation and validation[key] != reference_region:
                raise ValueError(f'Native aggregate {key} does not match reference region {reference_region}')
        for key, value in {**hashes, 'runtime_sha256': runtime_sha256}.items():
            actual = validation.get(key)
            if not isinstance(actual, str) or actual.lower() != value:
                raise ValueError(f'Native aggregate does not match this package: {key}')
        if type(validation.get('native_gameplay_verified')) is not bool:
            raise ValueError('Native aggregate must explicitly state native_gameplay_verified as a JSON boolean')
        verified = validation['native_gameplay_verified'] is True
        validation_summary = {**hashes, 'reference_region': reference_region, 'runtime_sha256': runtime_sha256,
                              'native_gameplay_verified': verified, 'scope': 'reference-region-only'}
    source_inventory = inventory(sources)
    put(sources, 'SOURCE-INVENTORY.json', json_bytes({'format': 1, 'components': ['revolverator', 'arsenal-compat', 'arsenal-file-overlay']+(['weapon-xp'] if xp else []),
        'source_commit': args.source_commit, 'reference_region': reference_region,
        'native_verified_regions': [reference_region] if verified else [],
        'files': source_inventory, 'inventory_self_excluded': True,
        'excluded': ['client EXEs', 'ISOs', 'memory cards', 'staged absolute-path manifests', 'SDK/toolchains', 'build outputs']}))
    inputs.verify()
    owned = []
    try:
        write_archive(output, binary, owned)
        write_archive(source_out, sources, owned)
        inputs.verify()
        receipt = {'format': 1, 'kind': 'openrac-native-revolverator-release', 'id': 'revolverator', 'version': manifest['version'],
            'reference_region': reference_region, 'runtime_included': False, 'runtime_sha256': runtime_sha256, **hashes,
            'native_verified_regions': [reference_region] if verified else [],
            'native_gameplay_verified': verified, 'validation': validation_summary, 'inputs_stable': True,
            'source_commit': args.source_commit, 'hd_icon_input_included': args.hd_icon_data is not None,
            'binary_archive': {'name': output.name, 'sha256': sha(output.read_bytes()), 'files': inventory(binary)},
            'source_archive': {'name': source_out.name, 'sha256': sha(source_out.read_bytes()), 'files': inventory(sources)},
            'inputs_before_after': dict(sorted(inputs.inventory.items())), 'published': False}
        with receipt_out.open('xb') as handle:
            owned.append(receipt_out);handle.write(json_bytes(receipt))
    except Exception:
        for path in reversed(owned):
            path.unlink(missing_ok=True)
        raise
    print(json.dumps({'archive': str(output), 'sha256': receipt['binary_archive']['sha256'], 'native_files': len(binary),
        'source_archive': str(source_out), 'source_sha256': receipt['source_archive']['sha256'], 'source_files': len(sources),
        'receipt': str(receipt_out), 'reference_region': reference_region, 'runtime_sha256': runtime_sha256,
        'native_gameplay_verified': verified, 'runtime_included': False, 'published': False}, indent=2))


if __name__ == '__main__':
    main()

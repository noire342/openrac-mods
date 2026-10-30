#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package this standalone mod from a clean build, without local QA state.

python tools/package_delivery.py --build build --output dist [--validation receipt.json]
No game data, credentials, QA binaries or personal profiles are package inputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path, help='CMake build root, also for a combined build')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--validation', type=Path, help='optional factual validation receipt, never invented by packaging')
    args = parser.parse_args()
    source = json.loads((ROOT/'openrac-mod.json').read_text(encoding='utf-8'))
    mod, version = source['id'], source['version']
    target = mod.replace('-', '_')
    package = args.build.resolve()/'package'/target
    manifest = json.loads((package/'openrac-mod.json').read_text(encoding='utf-8'))
    if (manifest['id'], manifest['version']) != (mod, version):
        raise SystemExit('Built manifest differs from source; rebuild before packaging.')
    plugins = manifest['plugin']
    if len(plugins) != 1:
        raise SystemExit('Package exactly one built platform at a time.')
    platform, binary_name = next(iter(plugins.items()))
    binary = (package/binary_name).resolve()
    if not binary.is_relative_to(package) or not binary.is_file():
        raise SystemExit('Invalid native library path.')
    args.output.mkdir(parents=True, exist_ok=True)
    native = args.output/f'{mod}-{version}-{platform}.zip'
    sources = args.output/f'{mod}-{version}-source.zip'
    if native.exists() or sources.exists():
        raise SystemExit('Output archives already exist; choose a fresh output directory.')
    with zipfile.ZipFile(native, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.write(package/'openrac-mod.json', 'openrac-mod.json')
        archive.write(binary, binary_name)
        for name in ['README.md','LICENSE','LICENSE.rcheevos']:
            if (ROOT/name).is_file(): archive.write(ROOT/name, name)
        for folder in ['docs','include','examples']:
            if (ROOT/folder).is_dir():
                for path in sorted((ROOT/folder).rglob('*')):
                    if path.is_file() and not path.is_symlink() and '__pycache__' not in path.parts:
                        archive.write(path, path.relative_to(ROOT).as_posix())
    paths = [ROOT/name for name in ['CMakeLists.txt','README.md','LICENSE','LICENSE.rcheevos','openrac-mod.json','sdk.lock','dependencies.lock.json','.clang-format','.gitignore'] if (ROOT/name).is_file()]
    for folder in ['src','include','docs','data','examples','tools','tests','vendor']:
        if (ROOT/folder).is_dir():
            paths.extend(path for path in (ROOT/folder).rglob('*') if path.is_file() and not path.is_symlink() and '__pycache__' not in path.parts)
    with zipfile.ZipFile(sources, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(paths):
            archive.write(path, mod+'/'+path.relative_to(ROOT).as_posix())
    assets = [native, sources]
    if args.validation:
        receipt = args.output/f'{mod}-{version}-validation.json'
        # Require JSON; retain its content instead of claiming new native proof.
        json.loads(args.validation.read_text(encoding='utf-8-sig'))
        receipt.write_bytes(args.validation.read_bytes())
        assets.append(receipt)
    for path in [native, sources]:
        with zipfile.ZipFile(path) as archive:
            if archive.testzip() is not None: raise SystemExit('Corrupt archive: '+path.name)
            if any(name.endswith(('.spra','.pdb','.iso')) or any(part.startswith('qa-') for part in Path(name).parts[:-1]) for name in archive.namelist()):
                raise SystemExit('Private or QA input in archive: '+path.name)
    (args.output/'SHA256SUMS').write_text(''.join(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name+'\n' for path in assets))
    print(json.dumps({'id':mod,'version':version,'platform':platform,'native':str(native),'source':str(sources),'nativeDllSha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2))

if __name__ == '__main__':
    main()

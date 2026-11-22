#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Fetch the pinned OpenRAC SDK for the Windows or portable build.

Only the MIT SDK directory is unpacked; no game code is copied into the mod.
This development tool is not run by players or by runtime asset preparation.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import stat
import tempfile
import tarfile
import urllib.request
from zipfile import ZipFile

WINDOWS_COMMIT = '9fc860d5ce34b6524e928ca1b6ed2a94ff03e5f2'


@dataclass(frozen=True)
class SdkArchive:
    url: str
    sha256: str
    prefix: str
    directory: str
    commit: str | None = None


SOURCES = {
    '1.12': SdkArchive(
        f'https://codeload.github.com/OpenRAC/OpenRAC/tar.gz/{WINDOWS_COMMIT}',
        '5b6d2696a2303b6cee627d80dfeeb71edf58da2f2bc54084523577f1e527b0d5',
        f'OpenRAC-{WINDOWS_COMMIT}/sdk/', 'rotatore-build-sdk', WINDOWS_COMMIT,
    ),
    '1.20': SdkArchive(
        'https://github.com/OpenRAC/OpenRAC/releases/download/sdk-v1.20.0/openrac-mod-sdk-1.20.0.zip',
        '4a9f042597d895802d00eca0571a5d6d378ca6e995b2f3a55d59bb90919e0eba',
        'openrac-mod-sdk-1.20.0/', 'rotatore-portable-sdk',
    ),
}


def unpack(blob: bytes, source: SdkArchive) -> dict[str, bytes]:
    files: dict[str, bytes] = {}

    def store(name: str, regular: bool, read) -> None:
        relative = PurePosixPath(name.removeprefix(source.prefix))
        if (not regular or relative.is_absolute() or '..' in relative.parts
                or '\\' in name or not relative.parts):
            raise ValueError('Unsafe SDK archive member')
        path = f'sdk/{relative.as_posix()}'
        if path in files:
            raise ValueError(f'Duplicate SDK archive member: {path}')
        files[path] = read()

    if source.commit is None:
        with ZipFile(io.BytesIO(blob)) as archive:
            for item in archive.infolist():
                if item.filename.startswith(source.prefix) and not item.is_dir():
                    mode = item.external_attr >> 16
                    store(item.filename, not stat.S_ISLNK(mode), lambda: archive.read(item))
    else:
        with tarfile.open(fileobj=io.BytesIO(blob), mode='r:gz') as archive:
            for item in archive:
                if item.name.startswith(source.prefix) and not item.isdir():
                    store(item.name, item.isfile(), lambda: archive.extractfile(item).read())
    required = {'sdk/cmake/OpenRACModConfig.cmake', 'sdk/include/openrac/openrac_mod.h'}
    if not required <= files.keys():
        raise ValueError('Pinned archive does not contain the required SDK')
    return files

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interface', choices=SOURCES, default='1.12')
    parser.add_argument('--from', dest='archive', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    source = SOURCES[args.interface]
    dest = (args.output or Path(__file__).resolve().parents[2] / '.sdk' / source.directory).resolve()
    stamp = dest / 'SOURCE.json'
    identity = {'sha256': source.sha256, 'url': source.url, 'interface': args.interface}
    if source.commit:
        identity['commit'] = source.commit
    if stamp.is_file() and (dest/'sdk/cmake/OpenRACModConfig.cmake').is_file():
        old = json.loads(stamp.read_text(encoding='utf-8'))
        if old == identity:
            print(dest / 'sdk')
            return
    if dest.exists():
        raise ValueError('Refusing to overwrite an existing unverified SDK directory')
    if args.archive:
        blob = args.archive.read_bytes()
    else:
        with urllib.request.urlopen(source.url, timeout=90) as response:
            blob = response.read()
    if hashlib.sha256(blob).hexdigest() != source.sha256:
        raise ValueError('Pinned upstream SDK archive SHA256 mismatch')
    files = unpack(blob, source)
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rotatore-sdk-', dir=dest.parent) as temp:
        stage = Path(temp) / 'stage'
        stage.mkdir()
        for relative, data in files.items():
            path = stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (stage / 'SOURCE.json').write_text(json.dumps(identity, indent=2) + '\n', encoding='utf-8')
        stage.rename(dest)
    print(dest / 'sdk')

if __name__ == '__main__':
    main()

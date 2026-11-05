#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 lorenzogarrone
"""Fetch the pinned upstream mod-interface 1.12 SDK for this module's CI job.

Only the MIT SDK directory is unpacked; no game code is copied into the mod.
This development tool is not run by players or by runtime asset preparation.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tempfile
import tarfile
import urllib.request

COMMIT='9fc860d5ce34b6524e928ca1b6ed2a94ff03e5f2'
SHA256='5b6d2696a2303b6cee627d80dfeeb71edf58da2f2bc54084523577f1e527b0d5'
URL=f'https://codeload.github.com/OpenRAC/OpenRAC/tar.gz/{COMMIT}'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--from',dest='source',type=Path)
    p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[2]/'.sdk/rotatore-build-sdk')
    args=p.parse_args();dest=args.output.resolve();stamp=dest/'SOURCE.json'
    if stamp.is_file() and (dest/'sdk/cmake/OpenRACModConfig.cmake').is_file():
        old=json.loads(stamp.read_text())
        if old.get('commit')==COMMIT and old.get('sha256')==SHA256:
            print(dest/'sdk');return
    if dest.exists():raise ValueError('Refusing to overwrite an existing unverified SDK directory')
    blob=args.source.read_bytes() if args.source else urllib.request.urlopen(URL,timeout=90).read()
    if hashlib.sha256(blob).hexdigest()!=SHA256:raise ValueError('Pinned upstream source archive SHA256 mismatch')
    prefix=f'OpenRAC-{COMMIT}/sdk/'
    files={}
    with tarfile.open(fileobj=io.BytesIO(blob),mode='r:gz') as archive:
        for item in archive:
            if not item.name.startswith(prefix):continue
            if item.isdir():continue
            if not item.isfile():raise ValueError('Unexpected SDK link or special file')
            relative=PurePosixPath(item.name[len(prefix):])
            if relative.is_absolute() or '..' in relative.parts:raise ValueError('Unsafe SDK archive member')
            files['sdk/'+relative.as_posix()]=archive.extractfile(item).read()
    if 'sdk/cmake/OpenRACModConfig.cmake' not in files or 'sdk/include/openrac/openrac_mod.h' not in files:
        raise ValueError('Pinned source does not contain the required SDK')
    dest.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rotatore-sdk-',dir=dest.parent) as temp:
        stage=Path(temp)/'stage';stage.mkdir()
        for relative,data in files.items():
            path=stage/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        (stage/'SOURCE.json').write_text(json.dumps({'commit':COMMIT,'sha256':SHA256,'url':URL,'interface':'1.12'},indent=2)+'\n')
        stage.rename(dest)
    print(dest/'sdk')

if __name__=='__main__':main()

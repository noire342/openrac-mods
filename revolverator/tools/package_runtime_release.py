#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 lorenzogarrone
"""Package the tested Windows weapon and its complete corresponding source.

The SDK-generated package manifest is used, never the raw source manifest.
Only authored ammunition art ships; native drill and HUD art is derived on the
player's machine. This tool writes artifacts and hashes but never publishes.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import zipfile
from package_source import snapshot

AUTHORED_ART={
    'assets/ammunition/model.glb':'1fa448c38e20beb3440005d4c854ecef88e2fb2e956f42674d7086ef57d78c99',
    'assets/ammunition/texture.png':'648821f6b402885c23fd6b47a219a48a078aa194fecdfd58583d66d94d1918a9',
}
PREPARERS=('prepare_runtime_assets.py','build_model_pack.py','build_golden_assets.py')

def digest(data: bytes) -> str:return hashlib.sha256(data).hexdigest()
def document(path: Path):return json.loads(path.read_text(encoding='utf-8-sig'))
def read(root: Path,relative: str) -> bytes:
    member=PurePosixPath(relative)
    if member.is_absolute() or '..' in member.parts:raise ValueError('Unsafe package member')
    path=root.joinpath(*member.parts)
    if path.is_symlink() or not path.resolve(strict=True).is_relative_to(root):
        raise ValueError(f'External or symbolic input: {relative}')
    return path.read_bytes()
def inventory(files):return {n:{'bytes':len(b),'sha256':digest(b)} for n,b in sorted(files.items())}
def pe_library(data: bytes,name: str):
    if len(data)<128 or data[:2]!=b'MZ':raise ValueError(f'Not a PE library: {name}')
    offset=struct.unpack_from('<I',data,60)[0]
    if offset+26>len(data) or data[offset:offset+4]!=b'PE\0\0':raise ValueError(f'Invalid PE: {name}')
    machine=struct.unpack_from('<H',data,offset+4)[0]
    characteristics=struct.unpack_from('<H',data,offset+22)[0]
    if machine!=0x8664 or not characteristics&0x2000:raise ValueError(f'Not a Windows x64 DLL: {name}')
def archive(path: Path,files: dict[str,bytes]):
    if path.exists():raise ValueError(f'Refusing to overwrite {path.name}')
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            item=zipfile.ZipInfo(name,(2026,1,1,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED
            item.external_attr=0o100644<<16;z.writestr(item,data,compresslevel=9)
    with zipfile.ZipFile(path) as z:
        if z.testzip() is not None:raise ValueError('ZIP CRC verification failed')
        if {n:digest(z.read(n)) for n in z.namelist()}!={n:digest(b) for n,b in files.items()}:
            raise ValueError('ZIP member verification failed')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module',type=Path,required=True,help='SDK-generated package/revolverator directory')
    p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--overlay',type=Path,required=True,help='Exact native-validated asset adapter DLL')
    p.add_argument('--dll-sha256',required=True);p.add_argument('--overlay-sha256',required=True)
    p.add_argument('--validation',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--commit',default='',help='Corresponding publication commit, when assigned')
    a=p.parse_args();module=a.module.resolve(strict=True);source=a.source.resolve(strict=True)
    manifest_bytes=read(module,'openrac-mod.json');manifest=json.loads(manifest_bytes)
    original=document(source/'openrac-mod.json');validation=document(a.validation)
    for key in ('id','version','author','api','games','license'):
        if manifest.get(key)!=original.get(key):raise ValueError(f'Generated/source manifest mismatch: {key}')
    if manifest.get('format')!=2 or manifest.get('id')!='revolverator' or manifest.get('author')!='lorenzogarrone':
        raise ValueError('Expected the authored format-2 Revolverator package')
    major,minor=map(int,manifest['api'].split('.'))
    if major!=1 or minor<12:raise ValueError('The public package requires mod interface 1.12 or newer')
    plugin='lib/windows-x86_64/revolverator.dll'
    if manifest.get('plugin')!={'windows-x86_64':plugin}:raise ValueError('Use the SDK-generated Windows manifest')
    if set(manifest.get('extensions',{}).get('noire342.arsenal',{}).get('bundledUtilities',[]))!={'arsenal_file_overlay.dll'}:
        raise ValueError('Bundled utility metadata must match the modern asset adapter only')
    weapon=read(module,plugin);overlay=a.overlay.resolve(strict=True).read_bytes()
    pe_library(weapon,plugin);pe_library(overlay,'arsenal_file_overlay.dll')
    if digest(weapon)!=a.dll_sha256.lower() or digest(overlay)!=a.overlay_sha256.lower():
        raise ValueError('A native library differs from the explicit tested hash')
    if validation.get('pass') is not True and validation.get('qa_passed') is not True:
        raise ValueError('An accepted release validation receipt is required')
    if validation.get('revolverator_sha256')!=digest(weapon):
        raise ValueError('Validation receipt belongs to another weapon DLL')
    if validation.get('overlay_sha256')!=digest(overlay):
        raise ValueError('Validation receipt belongs to another asset adapter DLL')
    files={'openrac-mod.json':manifest_bytes,plugin:weapon,
           'lib/windows-x86_64/arsenal_file_overlay.dll':overlay,
           'README.md':read(source,'README.md'),'LICENSE':read(source,'LICENSE')}
    for name,wanted in AUTHORED_ART.items():
        blob=read(source,name)
        if digest(blob)!=wanted:raise ValueError(f'Authored ammunition provenance differs: {name}')
        files[name]=blob
    for name in PREPARERS:files['tools/'+name]=read(source,'tools/'+name)
    for path in sorted((source/'docs').rglob('*')):
        if path.is_file() and path.suffix in ('.md','.json'):
            relative=path.relative_to(source).as_posix();files[relative]=read(source,relative)
    notices={
        'licenses/minhook/LICENSE.txt':'third_party/minhook/LICENSE.txt',
        'licenses/minhook/AUTHORS.txt':'third_party/minhook/AUTHORS.txt',
        'licenses/file-overlay/LICENSE':'support/file-overlay/LICENSE',
        'licenses/file-overlay/README.md':'support/file-overlay/README.md',
        'licenses/file-overlay/LICENSE-MIT.md':'support/file-overlay/docs/LICENSE-MIT.md',
        'licenses/file-overlay/minhook.lock.json':'support/file-overlay/third_party/minhook.lock.json',
    }
    for name,relative in notices.items():files[name]=read(source,relative)
    forbidden=('assets/graphics/','assets/golden/','revolverator_hd_icon_data.hpp')
    if any(any(part in name for part in forbidden) for name in files):raise ValueError('Game-derived artwork is not distributable')
    version=manifest['version'];out=a.output.resolve();out.mkdir(exist_ok=True,parents=True)
    if a.commit and (len(a.commit)!=40 or any(c not in '0123456789abcdef' for c in a.commit)):
        raise ValueError('Publication commit must be a full lowercase SHA')
    meta={'format':1,'id':'revolverator','version':version,'author':'lorenzogarrone',
          'minimum_mod_interface':manifest['api'],'source_commit':a.commit,
          'revolverator_sha256':digest(weapon),'overlay_sha256':digest(overlay),
          'game_derived_artwork_included':False,'automatic_player_disc_preparation':True,
          'ai_assisted':True,'files':inventory(files),'native_validation':validation}
    files['RELEASE-VALIDATION.json']=(json.dumps(meta,indent=2)+'\n').encode()
    native=out/f'revolverator-{version}-windows-x86_64.zip';archive(native,files)
    source_files={'revolverator/'+name:data for name,data in snapshot(source).items()}
    source_files['SOURCE-SNAPSHOT.json']=(json.dumps({'format':1,'source_commit':a.commit,
        'files':inventory(source_files),'game_derived_artwork_included':False},indent=2)+'\n').encode()
    source_zip=out/f'revolverator-{version}-source.zip';archive(source_zip,source_files)
    receipt=out/f'revolverator-{version}-validation.json'
    if receipt.exists():raise ValueError('Validation artifact already exists')
    receipt.write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
    artifacts=(native,source_zip,receipt)
    sums=out/'SHA256SUMS'
    if sums.exists():raise ValueError('SHA256SUMS already exists')
    sums.write_text(''.join(f'{digest(path.read_bytes())}  {path.name}\n' for path in artifacts),encoding='ascii')
    print(json.dumps({'version':version,'source_commit':a.commit,'artifacts':inventory({path.name:path.read_bytes() for path in (*artifacts,sums)})},indent=2))

if __name__=='__main__':main()

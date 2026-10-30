#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
"""Create one PRIVATE test ZIP. It never installs, launches or publishes it.

The outer ZIP is an extract-and-run private helper, not an invented standard
launcher bundle format. Its inner plugin and replacement pack retain their
standard OpenRAC manifests. Original level manifests are never packaged.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile


def sha(data):return hashlib.sha256(data).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin',type=Path,required=True)
    parser.add_argument('--models',type=Path,required=True)
    parser.add_argument('--ammunition-overlay',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    output=args.out.resolve()
    if output.exists():raise ValueError('Refusing to overwrite an existing private bundle')
    source=Path(__file__).resolve().parents[1]
    plugin=args.plugin.resolve(strict=True);models=args.models.resolve(strict=True)
    ammo=args.ammunition_overlay.resolve(strict=True)
    entries={};inputs={}

    def take(path,relative):
        path=path.resolve(strict=True)
        if not path.is_file():raise ValueError(f'Not a file: {path}')
        data=path.read_bytes();entries[relative]=data;inputs[path]=sha(data)

    manifest=json.loads((plugin/'openrac-mod.json').read_text())
    if manifest.get('format')!=2 or manifest.get('id')!='revolverator':raise ValueError('Expected a built native Revolverator package')
    if 'ntsc' not in manifest.get('games',{}).get('rac1',[]):raise ValueError('The plugin does not declare NTSC-U support')
    if 'windows-x86_64' not in manifest.get('plugin',{}):raise ValueError('No Windows x86-64 library in the package')
    take(plugin/'openrac-mod.json','plugin/revolverator/openrac-mod.json')
    if (plugin/'LICENSE').is_file():take(plugin/'LICENSE','LICENSE')
    for relative in manifest['plugin'].values():
        rel=PurePosixPath(relative)
        if rel.is_absolute() or '..' in rel.parts:raise ValueError('Unsafe plugin library path')
        take(plugin/relative,'plugin/revolverator/'+relative)
    pack=json.loads((models/'openrac-mod.json').read_text())
    if pack.get('format')!=1 or pack.get('game')!='rac1' or pack.get('version')!='ntsc':raise ValueError('Expected the native NTSC-U replacement pack')
    take(models/'openrac-mod.json','graphics/openrac-mod.json')
    take(models/'rotor.json','graphics/rotor.json')
    for level in range(19):
        name=f'level_{level:02}'
        for relative in ('mobys/moby_180.glb','textures/moby_revolverator_original.png'):
            take(models/name/relative,f'graphics/{name}/{relative}')
    receipt=json.loads((ammo/'build-receipt.json').read_text())
    recipe=json.loads((ammo/'recipe.json').read_text())
    if recipe.get('model_kind')!='industrial_energy_cartridge' or 'bit_centres' in recipe:
        raise ValueError('Expected the single industrial energy cartridge recipe')
    if receipt.get('visual_class')!=32000 or receipt.get('vendor_visual_class')!=2016 or len(receipt.get('levels_added',[]))!=19:
        raise ValueError('Expected the single-cell world/vendor ammunition extension')
    scales={float(info['scale']) for info in receipt['levels_added']}
    if len(scales)!=1:raise ValueError('A portable prebuilt cell currently requires one native pickup scale across all levels')
    ammo_model=ammo/'levels/level_01/revolverator_ammo/model.glb'
    take(ammo_model,'ammunition/model.glb')
    take(ammo_model.parent/'texture.png','ammunition/texture.png')
    asset={'format':1,'game':'rac1','disc':'ntsc','native_pickup_class':225,'world_class':32000,
           'vendor_class':2016,'native_scale':next(iter(scales)),'kind':'industrial_energy_cartridge'}
    entries['ammunition/asset.json']=(json.dumps(asset,indent=2)+'\n').encode()
    take(source/'tools/Start-Revolverator.ps1','Start-Revolverator.ps1')
    for name in ('README.md','docs/BUILD_AND_ASSETS.md','docs/QA_CHECKLIST.md','docs/API.md'):
        if (source/name).is_file():take(source/name,name)
    entries['README-FIRST.txt']=(
        'PRIVATE Revolverator development delivery; native gameplay QA is not certified.\n'
        'Extract this ZIP, then run Start-Revolverator.ps1 -PrepareOnly to inspect preparation.\n'
        'Run Start-Revolverator.ps1 to launch an isolated, no-card session with the installed NTSC-U runtime.\n'
        'The session does not grant the weapon automatically; normal purchase uses Hoven stock.\n'
        'Original level manifests are read locally, cloned and extended; installed data and saves are unchanged.\n'
        'The outer ZIP is not a format supported by the standard launcher importer.\n'
        'It contains private game-derived replacement artwork from the player own disc: do not publish it.\n'
    ).encode()
    delivery={'format':1,'kind':'revolverator-private-test-bundle','game':'rac1','disc':'ntsc',
        'version':manifest['version'],'publication':'PRIVATE; not approved for public release',
        'native_gameplay_verified':False,'files':{name:{'bytes':len(data),'sha256':sha(data)} for name,data in sorted(entries.items())}}
    entries['delivery.json']=(json.dumps(delivery,indent=2)+'\n').encode()
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for name,data in sorted(entries.items()):
            info=zipfile.ZipInfo(name,date_time=(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
            archive.writestr(info,data)
    if any(sha(path.read_bytes())!=digest for path,digest in inputs.items()):
        output.unlink();raise RuntimeError('An input changed during packaging; unstable private bundle discarded')
    print(json.dumps({'publication':'PRIVATE','native_gameplay_verified':False,'archive':str(output),
        'files':len(entries),'sha256':sha(output.read_bytes())},indent=2))


if __name__=='__main__':main()

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
"""Verify a single energy-cell asset and matching world/vendor visual aliases."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def checksum(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlay',type=Path,required=True)
    parser.add_argument('--previous-snapshot',type=Path)
    args=parser.parse_args();root=args.overlay
    receipt=json.loads((root/'build-receipt.json').read_text())
    assert receipt['visual_class']==32000 and receipt['vendor_visual_class']==2016
    recipe=json.loads((root/'recipe.json').read_text())
    assert recipe['model_kind']=='industrial_energy_cartridge' and 'bit_centres' not in recipe
    assert len(receipt['levels_added'])==19
    assert len(list((root/'levels').glob('level_*/manifest.json')))==21
    for entry in receipt['levels_added']:
        folder=root/'levels'/f"level_{entry['level']:02}"
        manifest=json.loads((folder/'manifest.json').read_text())
        world,vendor,native=(manifest['classes']['moby'][str(i)] for i in (32000,2016,225))
        assert world['mesh']==vendor['mesh']
        assert world['scale']==vendor['scale']==native['scale']==entry['scale']
        model=folder/world['mesh'];data=model.read_bytes()
        magic,version,length=struct.unpack_from('<III',data)
        assert (magic,version,length)==(0x46546c67,2,len(data))
        n,kind=struct.unpack_from('<II',data,12);document=json.loads(data[20:20+n])
        assert kind==0x4e4f534a and not document.get('skins')
        assert len(document['meshes'])==1 and sum('mesh' in node for node in document['nodes'])==1
        assert document['nodes'][0]['name']=='revolverator_energy_cartridge'
        assert (model.parent/document['images'][0]['uri']).is_file()
        positions=document['accessors'][0]
        height=(positions['max'][2]-positions['min'][2])*entry['scale']
        assert abs(height-.389)<.00001
    for original,expected in receipt['original_manifests_sha256'].items():
        assert checksum(Path(original))==expected
    previous=0
    if args.previous_snapshot:
        old=json.loads(args.previous_snapshot.read_text())
        for path,expected in old.items():assert checksum(Path(path))==expected
        previous=len(old)
    result={'single_energy_cell':True,'world_class':32000,'vendor_class':2016,'models_verified':19,
        'manifests':21,'native_scale_matches':True,'height_metres':height,
        'original_manifests_unchanged':True,'previous_output_files_unchanged':previous}
    (root/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Validate head-only binding while preserving authentic source art and importer compatibility."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--levels',type=Path,required=True)
    parser.add_argument('--pack',type=Path,required=True)
    parser.add_argument('--editor',type=Path,required=True)
    args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('rotor_builder',Path(__file__).parents[1]/'tools/build_model_pack.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    sys.path.insert(0,str(args.editor.resolve()));import overrides
    source_level=args.levels/'level_12'
    source_info=json.loads((source_level/'manifest.json').read_text())['classes']['moby']['164']
    source_path=source_level/source_info['mesh'];source,raw=builder.read_glb(source_path)
    original_primitive=source['meshes'][0]['primitives'][0];attributes=original_primitive['attributes']
    original={name:builder.accessor_values(source,raw,index) for name,index in attributes.items()}
    triangles=builder.accessor_values(source,raw,original_primitive['indices'])
    texture=(source_path.parent/source['images'][0]['uri']).read_bytes()
    metadata=json.loads((args.pack/'rotor.json').read_text())
    target_class=int(metadata.get('target_class',180))
    assert metadata['source_sha256']==hashlib.sha256(source_path.read_bytes()).hexdigest()
    assert metadata['texture_sha256']==hashlib.sha256(texture).hexdigest()
    total=0
    for level in range(19):
        name=f'level_{level:02}'
        native_info=json.loads((args.levels/name/'manifest.json').read_text())['classes']['moby']['180']
        native_path=args.levels/name/native_info['mesh']
        model_path=args.pack/name/f'mobys/moby_{target_class}.glb'
        document,data=builder.read_glb(model_path)
        primitive=document['meshes'][0]['primitives'][0]
        output={key:builder.accessor_values(document,data,index) for key,index in primitive['attributes'].items()}
        ratio=float(source_info['scale'])/float(native_info['scale'])
        expected_positions=[tuple(struct.unpack('<f',struct.pack('<f',value*ratio))[0] for value in row)
                            for row in original['POSITION']]
        assert output['POSITION']==expected_positions, f'{name}: authentic geometry moved beyond the native scale conversion'
        for attribute in ('NORMAL','TEXCOORD_0'):
            assert output[attribute]==original[attribute],f'{name}: source {attribute} changed'
        assert builder.accessor_values(document,data,primitive['indices'])==triangles,f'{name}: source topology changed'
        head_flags=[]
        for before_joints,before_weights,after_joints,after_weights in zip(
                original['JOINTS_0'],original['WEIGHTS_0'],output['JOINTS_0'],output['WEIGHTS_0']):
            original_bones={joint for joint,weight in zip(before_joints,before_weights) if weight>0}
            head=original_bones=={35};head_flags.append(head)
            assert after_joints==(4 if head else 0,0,0,0),f'{name}: static housing or corkscrew bound to wrong joint'
            assert after_weights[0]>0 and sum(after_weights[1:])==0,f'{name}: a rotating vertex has blended weights'
        assert sum(head_flags)==208 and len(head_flags)-sum(head_flags)==1338
        for start in range(0,len(triangles),3):
            assert len({head_flags[triangle[0]] for triangle in triangles[start:start+3]})==1, f'{name}: mixed head/body triangle'
        assert sum('mesh' in node for node in document['nodes'])==1,f'{name}: duplicate mesh instance'
        assert overrides.same_skeleton(overrides.skeleton(model_path),overrides.skeleton(native_path)),f'{name}: launcher skeleton compatibility changed'
        assert (args.pack/name/'textures/moby_revolverator_original.png').read_bytes()==texture,f'{name}: authentic texture changed'
        assert metadata['levels'][name]['head_vertices']==208
        if target_class!=180:
            assert not (args.pack/name/'mobys/moby_180.glb').exists(),f'{name}: independent pack must not replace native Walloper geometry'
        total+=1
    print(json.dumps({'models':total,'target_class':target_class,'body_vertices_each':1338,'head_vertices_each':208,
        'mixed_triangles':0,'geometry_uv_normals_topology_preserved':True,
        'all_native_inverse_binds_compatible':True,'original_texture_preserved':True},indent=2))


if __name__=='__main__':main()

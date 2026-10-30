#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 noire342
"""Build the private graphics companion from the player's extracted R&C1 assets.

Preserves the authentic drill geometry/UV/material. Its rigid housing binds to
Walloper root0; the original separate corkscrew component binds to joint4. The
native rotor supplies the shaft pivot and cancels native jab deformation. Never
edits game data.
"""
import argparse,copy,hashlib,json,struct,sys
from pathlib import Path

def read_glb(path):
 data=path.read_bytes()
 magic,version,length=struct.unpack_from('<III',data)
 if magic!=0x46546c67 or version!=2 or length!=len(data):raise ValueError(f'Invalid GLB: {path}')
 size,kind=struct.unpack_from('<II',data,12)
 if kind!=0x4e4f534a:raise ValueError('GLB has no JSON')
 document=json.loads(data[20:20+size]);offset=20+size
 size,kind=struct.unpack_from('<II',data,offset)
 if kind!=0x004e4942:raise ValueError('GLB has no binary buffer')
 return document,bytearray(data[offset+8:offset+8+size])

def write_glb(document,buffer):
 document['buffers']=[{'byteLength':len(buffer)}]
 encoded=json.dumps(document,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
 buffer+=b'\0'*(-len(buffer)%4)
 return struct.pack('<III',0x46546c67,2,28+len(encoded)+len(buffer))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(buffer),0x004e4942)+buffer

def array_positions(document,buffer,index):
 accessor=document['accessors'][index];view=document['bufferViews'][accessor['bufferView']]
 base=view.get('byteOffset',0)+accessor.get('byteOffset',0)
 sizes={5121:1,5123:2,5126:4};components={'VEC3':3,'VEC4':4}
 stride=view.get('byteStride',sizes[accessor['componentType']]*components[accessor['type']])
 return accessor,[base+i*stride for i in range(accessor['count'])]

def accessor_values(document,buffer,index):
 accessor=document['accessors'][index];view=document['bufferViews'][accessor['bufferView']]
 base=view.get('byteOffset',0)+accessor.get('byteOffset',0)
 fmt={5121:'B',5123:'H',5125:'I',5126:'f'}[accessor['componentType']]
 components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[accessor['type']]
 stride=view.get('byteStride',struct.calcsize(fmt)*components)
 return [struct.unpack_from('<'+fmt*components,buffer,base+i*stride) for i in range(accessor['count'])]

def original_head_pivot(source):
 document,raw=source
 matrices=accessor_values(document,raw,document['skins'][0]['inverseBindMatrices'])
 head=matrices[35]
 for r in range(3):
  for c in range(3):
   if abs(head[r*4+c]-(1 if r==c else 0))>1e-5:raise ValueError('The original drill head bind rotation changed')
 return [-head[12],-head[13],-head[14]]

def retarget(source,target,ratio,report=None):
 original,raw=source;document=copy.deepcopy(original);buffer=bytearray(raw)
 target_doc,target_raw=target
 head_count=body_count=head_triangles=body_triangles=0
 for mesh in document['meshes']:
  for primitive in mesh['primitives']:
   attrs=primitive['attributes'];accessor,offsets=array_positions(document,buffer,attrs['POSITION'])
   if accessor['componentType']!=5126:raise ValueError('Positions must be float32')
   joints=accessor_values(original,raw,attrs['JOINTS_0']);weights=accessor_values(original,raw,attrs['WEIGHTS_0'])
   flags=[]
   for joint,weight in zip(joints,weights):
    influences={j for j,w in zip(joint,weight) if w>0}
    if 35 in influences and influences!={35}:raise ValueError('The corkscrew is no longer a rigid original component')
    flags.append(influences=={35})
   positions=accessor_values(original,raw,attrs['POSITION'])
   for position,head in zip(positions,flags):
    if head and position[1]>-4.70:raise ValueError('The original head component includes housing geometry')
   indices=[row[0] for row in accessor_values(original,raw,primitive['indices'])]
   for start in range(0,len(indices),3):
    classification={flags[i] for i in indices[start:start+3]}
    if len(classification)!=1:raise ValueError('A triangle would span the rotating head and static housing')
    if True in classification:head_triangles+=1
    else:body_triangles+=1
   head_count+=sum(flags);body_count+=len(flags)-sum(flags)
   for offset in offsets:
    values=struct.unpack_from('<3f',raw,offset);struct.pack_into('<3f',buffer,offset,*(v*ratio for v in values))
   accessor['min']=[v*ratio for v in accessor['min']];accessor['max']=[v*ratio for v in accessor['max']]
   for attribute,first in [('JOINTS_0',0),('WEIGHTS_0',1)]:
    accessor,offsets=array_positions(document,buffer,attrs[attribute]);kind=accessor['componentType']
    fmt={5121:'4B',5123:'4H',5126:'4f'}[kind]
    value=first*(255 if kind==5121 and accessor.get('normalized') else 65535 if kind==5123 and accessor.get('normalized') else 1)
    for index,offset in enumerate(offsets):
     component=(4 if flags[index] else 0) if attribute=='JOINTS_0' else value
     struct.pack_into('<'+fmt,buffer,offset,component,0,0,0)
    accessor.pop('min',None);accessor.pop('max',None)
 document.pop('animations',None)
 document['nodes']=copy.deepcopy(target_doc['nodes']);document['scenes']=copy.deepcopy(target_doc['scenes']);document['scene']=target_doc.get('scene',0)
 mesh_bound=False
 for node in document['nodes']:
  if 'mesh' in node:
   if not mesh_bound:node['mesh']=0;mesh_bound=True
   else:node.pop('mesh');node.pop('skin',None)
 skin=copy.deepcopy(target_doc['skins'][0]);old_accessor=target_doc['accessors'][skin['inverseBindMatrices']]
 old_view=target_doc['bufferViews'][old_accessor['bufferView']]
 padding=-len(buffer)%4;buffer.extend(b'\0'*padding);start=len(buffer)
 view_start=old_view.get('byteOffset',0);buffer.extend(target_raw[view_start:view_start+old_view['byteLength']])
 new_view=copy.deepcopy(old_view);new_view['byteOffset']=start;new_view['buffer']=0
 document['bufferViews'].append(new_view);new_accessor=copy.deepcopy(old_accessor);new_accessor['bufferView']=len(document['bufferViews'])-1
 document['accessors'].append(new_accessor);skin['inverseBindMatrices']=len(document['accessors'])-1;document['skins']=[skin]
 document['images'][0]['uri']='../textures/moby_revolverator_original.png'
 for mesh in document['meshes']:mesh['name']='revolverator_original'
 if head_count!=208 or body_count!=1338:raise ValueError('Unexpected original Hoven drill vertex components')
 if report is not None:
  report.update({'body_joint':0,'head_joint':4,'original_head_joint':35,
   'head_vertices':head_count,'body_vertices':body_count,'head_triangles':head_triangles,
   'body_triangles':body_triangles,'mixed_triangles':0,'head_pivot_source':original_head_pivot(source),
   'head_pivot_model':[v*ratio for v in original_head_pivot(source)],
   'importer_inverse_bind_matrices_preserved':True,'mesh_instances':1})
 return write_glb(document,buffer)

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--levels',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--editor',type=Path,required=True)
 args=parser.parse_args();levels=args.levels.resolve();out=args.out.resolve()
 if out.exists():raise ValueError('Output exists; choose a new folder')
 if out==levels or out.is_relative_to(levels):raise ValueError('Output must be separate from the installed game levels')
 source_level=levels/'level_12';source_manifest=json.loads((source_level/'manifest.json').read_text());source_info=source_manifest['classes']['moby']['164']
 if source_manifest.get('format')!=3 or source_manifest.get('game')!='rac1' or source_manifest.get('version')!='ntsc':raise ValueError('The original drill must come from extracted R&C1 NTSC-U format 3')
 source=read_glb(source_level/source_info['mesh'])
 source_image=(source_level/'mobys'/source[0]['images'][0]['uri']).resolve();texture=source_image.read_bytes()
 files={};replaces={};rotors={}
 for index in range(19):
  folder=levels/f'level_{index:02}';manifest=json.loads((folder/'manifest.json').read_text());info=manifest['classes']['moby'].get('180')
  if not info:raise ValueError(f'Native Walloper class 180 is missing in level {index}')
  if manifest.get('format')!=3 or manifest['game']!='rac1' or manifest['version']!='ntsc':raise ValueError('This pack targets extracted R&C1 NTSC-U format 3')
  target=read_glb(folder/info['mesh']);report={};model=retarget(source,target,float(source_info['scale'])/float(info['scale']),report)
  rotors[f'level_{index:02}']={**report,'native_class_scale':float(info['scale'])}
  files[f'level_{index:02}/mobys/moby_180.glb']=model;files[f'level_{index:02}/textures/moby_revolverator_original.png']=texture
  replaces[f'level_{index:02}']={'mobys':[180],'textures':['moby_revolverator_original']}
 sys.path.insert(0,str(args.editor.resolve()));import overrides
 originals=overrides.originals(files,levels)
 manifest={'format':1,'id':'revolverator-original-model','game':'rac1','version':'ntsc','name':'Revolverator original model','author':'Insomniac Games; community restoration adapter','description':'Original Hoven drill, with a rigid housing and independently rotating corkscrew for the Revolverator gameplay mod. Replaces Walloper appearance.','levels_format':3,'replaces':replaces,'originals':originals}
 out.mkdir(parents=True)
 for name,data in files.items():path=out/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
 (out/'openrac-mod.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (out/'rotor.json').write_text(json.dumps({'format':1,'source_class_scale':float(source_info['scale']),
  'source_sha256':hashlib.sha256((source_level/source_info['mesh']).read_bytes()).hexdigest(),
  'texture_sha256':hashlib.sha256(texture).hexdigest(),'levels':rotors},indent=2)+'\n')
 (out/'README.txt').write_text('Private local asset pack generated from the installed R&C1 disc. Original art belongs to Insomniac Games / Sony. Enable together with the Revolverator native mod; disable both to restore Walloper.\n')
 print(json.dumps({'levels':len(replaces),'files':len(files),'source':str(source_level/source_info['mesh']),'source_sha256':hashlib.sha256((source_level/source_info['mesh']).read_bytes()).hexdigest(),'output':str(out)},indent=2))
if __name__=='__main__':main()

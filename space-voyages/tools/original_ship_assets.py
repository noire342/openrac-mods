#!/usr/bin/env python3
"""Append the private PAL Original2047 visual to a NEW Space asset pack.

The garage identity is0, physical class2047, source530, native controller531.
They are not interchangeable. Previous packs, source files and PCM are read
only. No commercial assets are included with this tool. The PAL visual can be
imported into an NTSC asset pack; that does not provide an NTSC garage adapter.
"""
from __future__ import annotations
import argparse, copy, json, math, shutil, struct, zlib
from pathlib import Path
from PIL import Image
from asset_formats import glb_read, values, mul, transform, trs, normalize, digest
from flight_effects_assets import nozzle_mount, file_inventory, check_inventory

TARGET_RADIUS=1.45
ROUNDING_SKIN=.000001
POSE_NAME='initial_cruise_v1'
IDENTITY={'garage_id':0,'physical_class':2047,'source_class':530,
          'controller_class':531,'travel_variant':0,'joints':31}

def inverse(m):
    a=[[m[c*4+r] for c in range(4)]+[float(c==r) for c in range(4)] for r in range(4)]
    for c in range(4):
        p=max(range(c,4),key=lambda r:abs(a[r][c]));a[c],a[p]=a[p],a[c]
        if abs(a[c][c])<1e-12:raise ValueError('Singular Original joint transform')
        q=a[c][c];a[c]=[x/q for x in a[c]]
        for r in range(4):
            if r!=c:
                q=a[r][c];a[r]=[x-q*y for x,y in zip(a[r],a[c])]
    return [a[r][4+c] for c in range(4) for r in range(4)]

def hinge(pivot,angle):
    c,s=math.cos(angle),math.sin(angle)
    m=[c,0,-s,0,0,1,0,0,s,0,c,0,0,0,0,1]
    q=transform(m,pivot,0);m[12:15]=[pivot[k]-q[k] for k in range(3)]
    return m

def cruise_pose(doc,raw):
    """InitialShipV1::rig: seq00 reference, mechanics(state4)={0,0}.

    Evaluate parents first. The braces are separate body children; they rotate
    about the corresponding leg hinge, not their own origins. Carrier motion
    is identity here because the Space controller supplies the final model.
    """
    if len(doc.get('skins',[]))!=1 or doc['skins'][0]['joints']!=list(range(31)):
        raise ValueError('Original requires its independent31-joint source rig')
    nodes=copy.deepcopy(doc['nodes'])
    animations=[a for a in doc.get('animations',[]) if a.get('name')=='seq_00']
    if len(animations)!=1:raise ValueError('Missing unambiguous Original seq00 reference')
    for c in animations[0]['channels']:
        target=c['target'];sampler=animations[0]['samplers'][c['sampler']]
        out=values(doc,raw,sampler['output'])
        if not out:raise ValueError('Empty Original reference sampler')
        nodes[target['node']][target['path']]=list(out[0])
    parents={c:i for i,n in enumerate(nodes) for c in n.get('children',[])}
    reference=[]
    for j,node in enumerate(nodes):
        p=parents.get(j)
        if p is not None and p>=j:raise ValueError('Original skeleton must be topological')
        reference.append(mul(reference[p],trs(node)) if p is not None else trs(node))
    desired=[];braces={16:9,20:9,17:12,18:12,21:25,22:25}
    for j,base in enumerate(reference):
        p=parents.get(j)
        world=mul(mul(desired[p],inverse(reference[p])),base) if p is not None else base
        if j in (9,12,25):world=mul(hinge(base[12:15],-1.3),base)
        if j in braces:world=mul(hinge(reference[braces[j]][12:15],-1.3),base)
        desired.append(world)
    return desired

def checked_source(root):
    meta=json.loads((root/'initial-ship-provenance.json').read_text(encoding='utf-8'))
    if (meta.get('abi'),meta.get('classId'),meta.get('sourceClass'),meta.get('nativeJoints'),meta.get('nativeSequences'),meta.get('region'))!=(1,2047,530,31,3,'pal'):
        raise ValueError('Expected private PAL Original2047/source530/31-joint ABI1')
    model=root/'graphics/mobys/moby_2047.glb'
    if digest(model.read_bytes())!=meta.get('renderMeshSha256'):raise ValueError('Original GLB provenance hash mismatch')
    resource=(root/'resources/class-2047.bin').read_bytes()
    if len(resource)<32 or resource[:8]!=b'ORSHIP\0\0':raise ValueError('Invalid Original CPU envelope')
    abi,oclass,size,crc,joints,seqs=struct.unpack_from('<6I',resource,8);cpu=resource[32:]
    if (abi,oclass,joints,seqs)!=(1,2047,31,3) or size!=len(cpu) or zlib.crc32(cpu)&0xffffffff!=crc or digest(cpu)!=meta.get('cpuNativeSha256'):
        raise ValueError('Original CPU identity, rig or integrity mismatch')
    doc,raw=glb_read(model)
    if len(doc.get('animations',[]))!=3:raise ValueError('Original source animation inventory changed')
    textures=[]
    for image in doc['images']:
        if 'uri' not in image:raise ValueError('Original source needs explicit external texture')
        texture=(model.parent/image['uri']).resolve()
        if not texture.is_relative_to(root.resolve()) or not texture.is_file():raise ValueError('Original texture escapes its source pack')
        textures.append(texture)
    if len(textures)!=2:raise ValueError('Original body/glass texture inventory changed')
    return meta,model,doc,raw,textures

def bake_original(root):
    meta,model,doc,raw,textures=checked_source(root);worlds=cruise_pose(doc,raw)
    palette=[mul(worlds[j],ib) for j,ib in zip(doc['skins'][0]['joints'],values(doc,raw,doc['skins'][0]['inverseBindMatrices']))]
    normal_palette=[inverse(m) for m in palette]
    parts=[]
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            if primitive.get('mode',4)!=4:raise ValueError('Original source must contain triangles')
            attrs=primitive['attributes'];material=doc['materials'][primitive['material']]
            pbr=material['pbrMetallicRoughness'];factor=pbr.get('baseColorFactor',[1,1,1,1])
            positions=values(doc,raw,attrs['POSITION']);normals=values(doc,raw,attrs['NORMAL'])
            uvs=values(doc,raw,attrs['TEXCOORD_0']);colors=values(doc,raw,attrs['COLOR_0'])
            joints=values(doc,raw,attrs['JOINTS_0']);weights=values(doc,raw,attrs['WEIGHTS_0'])
            if not all(len(rows)==len(positions) for rows in (normals,uvs,colors,joints,weights)):raise ValueError('Original vertex stream count mismatch')
            vertices=[]
            for p,n,uv,color,js,ws in zip(positions,normals,uvs,colors,joints,weights):
                if abs(sum(ws)-1)>1e-4 or any(w<0 for w in ws) or any(j>=31 for j in js):raise ValueError('Invalid Original skin weights')
                pos=tuple(sum(transform(palette[j],p)[k]*w for j,w in zip(js,ws)) for k in range(3))
                # Inverse transpose, including the extracted bind's tiny scale.
                norm=normalize(tuple(sum(sum(normal_palette[j][k*4+c]*n[c] for c in range(3))*w for j,w in zip(js,ws)) for k in range(3)))
                vertices.append((*pos,*norm,*uv,*(color[k]*factor[k] for k in range(4))))
            indices=[int(i[0]) for i in values(doc,raw,primitive['indices'])]
            if len(indices)%3 or any(i>=len(vertices) for i in indices):raise ValueError('Invalid Original triangle indices')
            texture=doc['textures'][pbr['baseColorTexture']['index']]
            sampler=doc.get('samplers',[{}])[texture.get('sampler',0)] if doc.get('samplers') else {}
            parts.append({'vertices':vertices,'indices':indices,'joints':joints,'weights':weights,
                          'texture':textures[texture['source']],'wrap_s':sampler.get('wrapS',10497),'wrap_t':sampler.get('wrapT',10497),
                          'alpha_mode':material.get('alphaMode','OPAQUE'),'double_sided':material.get('doubleSided',False),
                          'sphere_uv':material.get('name','').startswith('metal_'),'material_name':material.get('name','')})
    if len(parts)!=2 or parts[0]['alpha_mode']!='OPAQUE' or parts[1]['alpha_mode']!='BLEND' or not parts[1]['sphere_uv']:
        raise ValueError('Original body/native-glass material contract changed')
    positions=[v[:3] for part in parts for v in part['vertices']]
    lo=[min(v[k] for v in positions) for k in range(3)];hi=[max(v[k] for v in positions) for k in range(3)]
    center=[(lo[k]+hi[k])/2 for k in range(3)]
    source_radius=max(math.sqrt(sum((v[k]-center[k])**2 for k in range(3))) for v in positions)
    uniform_scale=(TARGET_RADIUS-ROUNDING_SKIN)/source_radius
    for part in parts:
        # Quantize before measuring/fitting: payload and metadata agree exactly.
        part['vertices']=[struct.unpack('<12f',struct.pack('<12f',*((v[k]-center[k])*uniform_scale for k in range(3)),*v[3:])) for v in part['vertices']]
    mounts=[]
    for joint,attachment in ((15,29),(24,30)):
        points=[];triangles=[]
        for part in parts:
            base=len(points);points.extend(v[:3] for v in part['vertices'])
            selected=[sum(w for j,w in zip(js,ws) if j==joint)>.9999 for js,ws in zip(part['joints'],part['weights'])]
            for i in range(0,len(part['indices']),3):
                tri=part['indices'][i:i+3]
                if all(selected[k] for k in tri):triangles.append(tuple(base+k for k in tri))
        mount=nozzle_mount(points,triangles);mount.update(mesh_joint=joint,attachment_joint=attachment,
            source_attachment=list(worlds[attachment][12:15]));mounts.append(mount)
    radius=max(math.sqrt(sum(c*c for c in v[:3])) for p in parts for v in p['vertices'])
    if radius>TARGET_RADIUS:raise ValueError('Float32 Original escaped the conservative hull envelope')
    info={'identity':IDENTITY,'source_region':'pal','source_disc':meta['disc'],'model_sha256':digest(model.read_bytes()),
          'cpu_sha256':meta['cpuNativeSha256'],'texture_sha256':[digest(p.read_bytes()) for p in textures],
          'pose':POSE_NAME,'pose_source':'InitialShipV1::rig + mechanics(state4), seq00 reference; canopy0, gear0; leg hinges9/12/25 -1.3 radians; braces16/20,17/18,21/22 follow legs',
          'forward_axis':'+X','up_axis':'+Z','centre_source':center,'bounds_source':[lo,hi],'source_radius':source_radius,
          'scale':uniform_scale,'radius':radius,'declared_radius':TARGET_RADIUS,'length':(hi[0]-lo[0])*uniform_scale,
          'scale_policy':'uniform target sphere1.45, 1e-6 float32 guard, within unchanged collision1.5 and camera1.85; preserve proportions',
          'mounts':mounts,'color':[1.375,.6875,.25,1],
          'color_source':'InitialShipV1::glow scoped controller531, original ABGR0x2058B0 /128, not a fake visual identity',
          'occupants':'Not contained in this GLB; native provider composes separate class0/10 actors. Space cockpit visual review remains required.',
          'vertex_count':sum(len(p['vertices']) for p in parts),'triangles':sum(len(p['indices'])//3 for p in parts),
          'provider_support':'PAL only; importing this geometry into NTSC does not provide or validate an NTSC garage provider'}
    return parts,info

def build(base,source,out):
    base,source,out=map(Path,(base,source,out))
    if out.exists():raise ValueError('Refusing to overwrite an existing Original pack')
    if not base.is_dir() or not source.is_dir():raise ValueError('Missing source pack')
    before=file_inventory(base);check_inventory(base,json.loads((base/'inventory.json').read_text()))
    source_before=file_inventory(source);parts,info=bake_original(source)
    assets=json.loads((base/'assets.json').read_text());provenance=json.loads((base/'provenance.json').read_text())
    if any(a['id'].startswith('ship_original') for a in assets['assets']) or (base/'original-ship.tsv').exists():raise ValueError('Base already includes an Original profile')
    if assets.get('region') not in ('pal','ntsc'):raise ValueError('Unsupported base pack region')
    rows=[];entries=[]
    # Copy only after all model/provenance/pose checks succeeded.
    shutil.copytree(base,out)
    for i,part in enumerate(parts):
        name='ship_original'+(f'_part{i}' if i else '')
        vertex_file=name+'.vertices.bin';index_file=name+'.indices.bin';rgba_file=name+'.rgba.bin'
        (out/vertex_file).write_bytes(b''.join(struct.pack('<12f',*v) for v in part['vertices']))
        (out/index_file).write_bytes(struct.pack('<'+'I'*len(part['indices']),*part['indices']))
        image=Image.open(part['texture']).convert('RGBA');(out/rgba_file).write_bytes(image.tobytes())
        entry={'id':name,'vertices':vertex_file,'indices':index_file,'rgba':rgba_file,'width':image.width,'height':image.height,
               'vertex_count':len(part['vertices']),'index_count':len(part['indices']),'vertex_stride':48,
               'wrap_s':part['wrap_s'],'wrap_t':part['wrap_t'],'alpha_mode':part['alpha_mode'],'alpha_cutoff':.5,
               'sphere_uv':part['sphere_uv'],'double_sided':part['double_sided'],'source_material':part['material_name']}
        entries.append(entry);rows.append(' '.join(str(entry[k]) for k in ('id','vertices','indices','rgba','width','height','vertex_stride','wrap_s','wrap_t','alpha_mode')))
    original=['format 1','ship original 0 ship_original 2047 530 31','pose '+POSE_NAME,
              f'bounds {TARGET_RADIUS:.10g} {info["length"]:.10g}','glass ship_original_part1']
    for m in info['mounts']:original.append('mount '+' '.join(f'{v:.10g}' for v in (*m['position'],*m['direction'],m['radius'],m['length'])))
    original.append('color '+' '.join(f'{v:.10g}' for v in info['color']))
    (out/'original-ship.tsv').write_text('\n'.join(original)+'\n',encoding='ascii')
    original_bytes=(base/'assets.tsv').read_bytes()
    if not original_bytes.endswith(b'\n'):raise ValueError('Base asset manifest must end in a newline')
    (out/'assets.tsv').write_bytes(original_bytes+('\n'.join(rows)+'\n').encode('ascii'))
    assets['assets'].extend(entries);assets['original_ship']={'manifest':'original-ship.tsv','identity':IDENTITY,'asset_group':'ship_original'}
    (out/'assets.json').write_text(json.dumps(assets,indent=2)+'\n',encoding='utf-8')
    info.update(format=1,private=True,target_region=assets['region'],base_inventory_sha256=digest((base/'inventory.json').read_bytes()),
                base_unchanged_payload={p:entry for p,entry in before.items() if p not in ('assets.tsv','assets.json','provenance.json')},
                source_inventory=source_before,meshes=entries,native_game_verified=False)
    (out/'original-ship-extension.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
    provenance['original_ship_extension']={k:v for k,v in info.items() if k not in ('base_unchanged_payload','source_inventory')}
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='utf-8')
    (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='utf-8')
    if file_inventory(base)!=before or file_inventory(source)!=source_before:raise RuntimeError('Input pack changed during build')
    return verify(out)

def verify(root):
    root=Path(root);inventory=json.loads((root/'inventory.json').read_text());check_inventory(root,inventory)
    e=json.loads((root/'original-ship-extension.json').read_text());check_inventory(root,e['base_unchanged_payload'])
    if e['identity']!=IDENTITY or len(e['meshes'])!=2 or len(e['mounts'])!=2:raise ValueError('Incomplete Original identity/profile')
    maximum=0.;positions=[];triangles=0;vertices=0
    for mesh in e['meshes']:
        raw=(root/mesh['vertices']).read_bytes();idx=(root/mesh['indices']).read_bytes()
        if len(raw)!=mesh['vertex_count']*48 or len(idx)!=mesh['index_count']*4:raise ValueError('Truncated Original mesh')
        vv=list(struct.iter_unpack('<12f',raw));ii=[x[0] for x in struct.iter_unpack('<I',idx)]
        if any(i>=len(vv) for i in ii) or len(ii)%3:raise ValueError('Invalid Original indices')
        for v in vv:
            if not all(math.isfinite(x) for x in v):raise ValueError('Nonfinite Original vertex')
            if abs(sum(x*x for x in v[3:6])-1)>.001:raise ValueError('Invalid Original normal')
            positions.append(v[:3]);maximum=max(maximum,math.sqrt(sum(x*x for x in v[:3])))
        if mesh['sphere_uv'] and any(v[8:]!=(.5,.5,.5,1.) for v in vv):raise ValueError('Native glass vertex colors were lost')
        vertices+=len(vv);triangles+=len(ii)//3
    if maximum>TARGET_RADIUS or abs(maximum-e['radius'])>1e-8:raise ValueError('Original hull escaped its approved envelope')
    for mount in e['mounts']:
        if mount['mesh_joint'] not in (15,24) or mount['ring_triangles']!=48 or mount['direction']!=[-1.,0.,0.]:raise ValueError('Original nozzle source contract changed')
    for name in ('audio.tsv','flight-effects.tsv','weapons.tsv'):
        if digest((root/name).read_bytes())!=e['base_unchanged_payload'][name]['sha256']:raise ValueError('Original altered an existing feature policy')
    return {'output':str(root),'region':e['target_region'],'original_identity':IDENTITY,'vertices':vertices,'triangles':triangles,
            'radius':maximum,'length':e['length'],'mounts':len(e['mounts']),'audio_unchanged':True,'base_payload_unchanged':True,
            'original_ship_tsv_sha256':digest((root/'original-ship.tsv').read_bytes()),'inventory_sha256':digest((root/'inventory.json').read_bytes()),'native_game_verified':False}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('base','source','out','verify'):parser.add_argument('--'+name,type=Path)
    args=parser.parse_args()
    if args.verify:result=verify(args.verify)
    elif args.base and args.source and args.out:result=build(args.base,args.source,args.out)
    else:parser.error('Provide --base --source --out, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

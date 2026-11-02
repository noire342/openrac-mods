#!/usr/bin/env python3
"""Native seated actors0/10 for the independent Original cockpit.

All extraction is offline/read-only. Source layouts and quaternion/scale
semantics come from OpenRAC's complete exporter and native scene loader.
No ship531 geometry or green crew asset is copied. The sampled actors are
those used by InitialShipV1's actual travelVariant0 boarding continuation.
"""
from __future__ import annotations
import argparse,copy,json,math,re,shutil,struct,sys
from pathlib import Path
from PIL import Image
from asset_formats import bounded,digest,glb_read,values,mul,transform,trs,normalize
from build_assets import Source
from original_ship_assets import inverse,TARGET_RADIUS,verify as verify_hull
from flight_effects_assets import file_inventory,check_inventory

TICK=464
BANK=1
CHUNK_TICKS=96
SEATS={0:(-.9,1.35,5.15),10:(-.9,-1.35,5.15)}
HIP={0:1,10:2}
CAPTURE_SHA256='2c967c12d6de31e29ba111b1848393961278f01ef2e7d4cdfd1bd5cb595e6163'

def core_classes(source,level=3):
    data=source.level_data(level);parts=[]
    for section in (2,10):
        at,size=struct.unpack_from('<II',data,section*8);part=bounded(data,at,size)
        parts.append(source.wad(part) if part[:3]==b'WAD' else part)
    index,core=parts;count,table=struct.unpack_from('<II',index,0x18)
    if not 1<=count<=4096:raise ValueError('Invalid native character class table')
    found={}
    for i in range(count):
        entry=bounded(index,table+i*32,32);at,oclass=struct.unpack_from('<II',entry)
        if oclass in (0,10,531):
            if at>=len(core):raise ValueError('Character class outside native core')
            found[oclass]=core[at:]
    if 531 not in found:
        # A planet need not own the selected travel class. Its shared resident
        # definition is also present in the original transition archive.
        transition=source.lump(0x13b8);base=struct.unpack_from('<I',transition,4)[0]
        n,table=struct.unpack_from('<II',transition,16)
        for i in range(n):
            at,oclass=struct.unpack_from('<II',bounded(transition,table+i*32,32))
            if oclass==531:found[531]=transition[base+at:]
    if set(found)!={0,10,531}:raise ValueError('Native boarding character/carrier resources missing')
    return found

def scene_chunk(source,bank=BANK,tick=TICK):
    """Native load_chunks_with_optional_fade: table at TOC12e8 +8*bank.

    PAL's supported60Hz mode uses its first ten records, as the native loader
    does when pal_mode==0. The blob is a chunk table followed by WAD packets.
    """
    if not 0<=bank<10 or not 0<=tick<598:raise ValueError('Unsupported boarding sample')
    blob=source.lump(0x12e8+bank*8);chunk_id=tick//CHUNK_TICKS
    offset,size=struct.unpack_from('<II',bounded(blob,chunk_id*8,8))
    if not size:raise ValueError('Missing native boarding chunk')
    packed=bounded(blob,0x800+offset,size);raw=source.wad(packed)
    if len(raw)<20:raise ValueError('Truncated native boarding record')
    end,_,_,count,_=struct.unpack_from('<5I',raw)
    if end!=598 or not 1<=count<=16:raise ValueError('Unexpected native boarding sequence')
    actors={}
    for i in range(count):
        at=struct.unpack_from('<I',bounded(raw,20+i*4,4))[0]
        oclass,_,_,positions=struct.unpack('<4I',bounded(raw,at,16))
        seq=at+16
        if oclass in actors:raise ValueError('Ambiguous native boarding actor')
        if not seq+28<len(raw) or positions>=len(raw):raise ValueError('Boarding actor outside chunk')
        actors[oclass]={'sequence':raw[seq:],'position_offset':positions,'record_offset':at,'raw':raw}
    if not all(c in actors for c in (0,10,531)):raise ValueError('Boarding actors0/10 and carrier are required')
    return actors,{'toc_offset':hex(0x12e8+bank*8),'bank':bank,'tick':tick,'chunk':chunk_id,
                   'frame':tick%CHUNK_TICKS,'key':(tick%CHUNK_TICKS)//2,
                   'bank_sha256':digest(blob),'chunk_sha256':digest(raw),'chunk_bytes':len(raw),
                   'source':'initialize_level_transition_with_audio selects travelVariant0+1; native actor records, not ship531 geometry'}

def native_pose(raw_class,sequence,key):
    """Exact native-key channels, with post-scale kept after the hierarchy."""
    from moby_anim import bind_pose,keys
    joints=raw_class[8];matrices,common=struct.unpack_from('<II',raw_class,0x14)
    bind,rest=bind_pose(raw_class,joints,matrices,common)
    times,frames,_=keys(sequence,0,joints,rest)
    if not 0<=key<len(frames):raise ValueError('Native character key outside sequence')
    frame=frames[key];world=[];post=[]
    for j,joint in enumerate(bind):
        node={'translation':frame[j,'translation'],'rotation':frame[j,'rotation'],'scale':frame[j,'scale']}
        local=trs(node);world.append(mul(world[joint.parent],local) if joint.parent is not None else local)
        post.append(mul(world[-1],trs({'scale':frame[j,'post_scale']})))
    return world,post,{'native_joints':joints,'key':key,'tick_in_chunk':times[key],
                       'cpu_class_header_sha256':digest(raw_class[:common+joints*16]),
                       'post_scaled_joints':[j for j in range(joints) if frame[j,'post_scale']!=(1,1,1)],
                       'zero_scale_joints':[j for j in range(joints) if 0 in frame[j,'scale']]}

def skin_actor(model,world,post,captured_post=False):
    doc,raw=glb_read(model);skin=doc['skins'][0];palette=[];native_joints=len(world)
    names=[doc['nodes'][node].get('name','') for node in skin['joints']]
    helpers={int(m[1]) for name in names if (m:=re.fullmatch(r'joint_(\d+)_post',name))}
    for node,ib in zip(skin['joints'],values(doc,raw,skin['inverseBindMatrices'])):
        name=doc['nodes'][node].get('name','');match=re.fullmatch(r'joint_(\d+)(_post)?',name)
        if not match or int(match[1])>=native_joints:raise ValueError('GLB/native character joint identity mismatch')
        # The exporter introduces helper bones only for non-inherited scales.
        # Native P*S applies the same post-scale to that joint's vertices.
        joint=int(match[1]);palette.append(mul(post[joint] if match[2] else world[joint],ib))
    parts=[]
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            attrs=primitive['attributes'];material=doc['materials'][primitive['material']]
            pp=values(doc,raw,attrs['POSITION']);nn=values(doc,raw,attrs['NORMAL'])
            uv=values(doc,raw,attrs['TEXCOORD_0']);jj=values(doc,raw,attrs['JOINTS_0']);ww=values(doc,raw,attrs['WEIGHTS_0'])
            colors=values(doc,raw,attrs['COLOR_0']) if 'COLOR_0' in attrs else [(1,1,1,1)]*len(pp)
            vertices=[]
            for p,n,t,js,ws,color in zip(pp,nn,uv,jj,ww,colors):
                if abs(sum(ws)-1)>1e-4 or any(j>=len(palette) for j in js):raise ValueError('Invalid native character skin weights')
                if captured_post:
                    # Captured native matrices already include non-inherited
                    # post-scale. The actual GLB routes those vertices solely to
                    # helper bones; a weighted plain parent would need P too.
                    for j,w in zip(js,ws):
                        match=re.fullmatch(r'joint_(\d+)',names[j])
                        if w>0 and match and int(match[1]) in helpers:raise ValueError('Captured PS cannot substitute a weighted plain post-scale parent')
                position=tuple(sum(transform(palette[j],p)[k]*w for j,w in zip(js,ws)) for k in range(3))
                # ReRAC skins normals by the same palette. Native zero-scale
                # animation intentionally collapses hidden subparts: no inverse.
                normal=normalize(tuple(sum(transform(palette[j],n,0)[k]*w for j,w in zip(js,ws)) for k in range(3)))
                vertices.append((*position,*normal,*t,*color))
            indices=[x[0] for x in values(doc,raw,primitive['indices'])]
            pbr=material['pbrMetallicRoughness'];texture=doc['textures'][pbr['baseColorTexture']['index']]
            image=doc['images'][texture['source']];path=(model.parent/image['uri']).resolve()
            if not path.is_relative_to(model.parent.parent.resolve()) or not path.is_file():raise ValueError('Character texture outside exported level')
            sampler=doc.get('samplers',[{}])[texture.get('sampler',0)] if doc.get('samplers') else {}
            metal=material.get('name','').startswith('metal_')
            parts.append({'vertices':vertices,'indices':indices,'texture':path,'material':material.get('name'),
                          'alpha_mode':'BLEND' if metal else material.get('alphaMode','OPAQUE'),
                          'alpha_cutoff':material.get('alphaCutoff',.5),'sphere_uv':metal,
                          'wrap_s':sampler.get('wrapS',10497),'wrap_t':sampler.get('wrapT',10497)})
    return parts

def build_crew(source,levels,original_info):
    classes=core_classes(source);actors,sample=scene_chunk(source)
    # Original's native provider derives only the carrier's body motion from
    # P3*S3. Removing that affine motion yields a static local cockpit pose.
    carrier_world,_,_=native_pose(classes[531],actors[531]['sequence'],sample['key'])
    from moby_anim import bind_pose
    count=classes[531][8];bind,_=bind_pose(classes[531],count,*struct.unpack_from('<II',classes[531],0x14))
    carrier_motion=mul(carrier_world[3],bind[3].inverse_bind);unmove=inverse(carrier_motion)
    ratios={c:struct.unpack_from('<f',classes[c],0x24)[0]/struct.unpack_from('<f',classes[531],0x24)[0] for c in (0,10)}
    result={};facts={}
    for oclass in (0,10):
        model=levels/'level_03/mobys'/f'moby_{oclass}.glb'
        world,post,info=native_pose(classes[oclass],actors[oclass]['sequence'],sample['key'])
        parts=skin_actor(model,world,post);pelvis=world[HIP[oclass]][12:15]
        for part in parts:
            vertices=[]
            for v in part['vertices']:
                offset=transform(unmove,tuple((v[k]-pelvis[k])*ratios[oclass] for k in range(3)),0)
                position=tuple((SEATS[oclass][k]+offset[k]-original_info['centre_source'][k])*original_info['scale'] for k in range(3))
                normal=normalize(transform(unmove,v[3:6],0))
                vertices.append(struct.unpack('<12f',struct.pack('<12f',*position,*normal,*v[6:])))
            part['vertices']=vertices
        allp=[v[:3] for part in parts for v in part['vertices']]
        info.update(model_sha256=digest(model.read_bytes()),native_scale_ratio=ratios[oclass],pelvis_joint=HIP[oclass],
                    provider_seat_source=SEATS[oclass],seat_source='Current InitialShipV1::update_seats policy; separately inspect geometric fit, not an extracted native attachment',
                    max_radius=max(math.sqrt(sum(x*x for x in p)) for p in allp),
                    bounds=[[min(p[k] for p in allp) for k in range(3)],[max(p[k] for p in allp) for k in range(3)]],
                    vertices=sum(len(p['vertices']) for p in parts),triangles=sum(len(p['indices'])//3 for p in parts),
                    materials=[p['material'] for p in parts])
        result[oclass]=parts;facts[str(oclass)]=info
    return result,{'sample':sample,'actors':facts,'carrier_motion':carrier_motion,
                   'basis':'Actor native pose relative to its pelvis; remove carrier P3*S3; apply actor/carrier scale ratio; anchor to Original provider seat; apply exact hull centre+scale'}

def captured_crew(source,levels,original_info,path,expected_sha=CAPTURE_SHA256):
    data=path.read_bytes()
    if digest(data)!=expected_sha:raise ValueError('Native cockpit capture hash mismatch')
    capture=json.loads(data)
    if (capture.get('format'),capture.get('readOnly'),capture.get('loadedLevel'),capture.get('loads'),
        capture.get('canonicalSceneTick'),capture.get('nativeTime'),capture.get('nativeBank'),capture.get('nativeFrame'),
        capture.get('shipState'),capture.get('levelMode'))!=(1,True,18,2,464,464,4,80,0,6):
        raise ValueError('Capture is not the validated live Original boarding frame')
    body=capture['original']
    if body.get('classId')!=2047 or len(body.get('jointsRelativeToOriginalBody',[]))!=31:raise ValueError('Native Original31 bind is absent')
    if not math.isfinite(body['scale']) or body['scale']<=0:raise ValueError('Invalid native Original scale')
    classes=core_classes(source);crew={};facts={}
    for cls,key in [(0,'ratchet'),(10,'clank')]:
        actor=capture[key];palette=actor.get('jointsRelativeToOriginalBody',[])
        if actor.get('classId')!=cls or len(palette)!=(111 if cls==0 else 75):raise ValueError('Native cockpit actor/palette mismatch')
        if any(len(m)!=16 or not all(math.isfinite(x) for x in m) for m in palette):raise ValueError('Invalid native cockpit matrix')
        if not math.isfinite(actor['scale']) or actor['scale']<=0:raise ValueError('Invalid native actor scale')
        ratio=actor['scale']/body['scale'];expected_ratio=struct.unpack_from('<f',classes[cls],0x24)[0]/struct.unpack_from('<f',classes[531],0x24)[0]
        if abs(ratio-expected_ratio)>1e-6:raise ValueError('Captured actor/carrier scale disagrees with the verified native class')
        model=levels/'level_03/mobys'/f'moby_{cls}.glb';parts=skin_actor(model,palette,palette,captured_post=True)
        for part in parts:
            part['vertices']=[struct.unpack('<12f',struct.pack('<12f',*((v[k]-original_info['centre_source'][k])*original_info['scale'] for k in range(3)),*v[3:])) for v in part['vertices']]
        common=struct.unpack_from('<I',classes[cls],0x18)[0];joints=len(palette)
        ps=[v[:3] for part in parts for v in part['vertices']]
        facts[str(cls)]={'native_joints':joints,'model_sha256':digest(model.read_bytes()),
          'cpu_class_header_sha256':digest(classes[cls][:common+joints*16]),'native_scale_ratio':ratio,
          'pelvis_joint':HIP[cls],'provider_seat_source':palette[HIP[cls]][12:15],
          'source':'Actual live cinema actor after Original seat modifier; not gameplay Hero or authored-key approximation',
          'bounds':[[min(p[k] for p in ps) for k in range(3)],[max(p[k] for p in ps) for k in range(3)]],
          'max_radius':max(math.sqrt(dot(p,p)) for p in ps),'vertices':sum(len(p['vertices']) for p in parts),
          'triangles':sum(len(p['indices'])//3 for p in parts),'materials':[p['material'] for p in parts],
          'post_scale':'Native matrices already contain PS; post-helper parents have no weighted vertices; no second scale applied'}
        crew[cls]=parts
    return crew,{'capture':{'sha256':expected_sha,'path':str(path.resolve()),
                 'source':'Read-only native fixture qa-menu-cockpit-11402-41; Original2047/31 and live cinema0/10'},
                 'sample':{'bank':1,'tick':464,'chunk':4,'key':40,'native_bank':4,'native_frame':80,
                           'source':'Native sampled matrices, not nominal authored-key40 evaluation'},'actors':facts,
                 'basis':'Captured jointsRelativeToOriginalBody include native actor/carrier scale, bases and seat modifiers; apply GLB inverse bind once, then exact hull centre/scale'}

def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def dot(a,b):return sum(x*y for x,y in zip(a,b))

def fit_cockpit(crew,base,facts,scale):
    """Fit the authored character poses to the real530 canopy, without scaling.

    This is an explicit adaptation. The provider's historical seat coordinates
    are not claimed to be native530 attachments. Convex supporting roof planes
    and the back opening bound the upper bodies; feet may be below the glazing.
    """
    vv=list(struct.iter_unpack('<12f',(base/'ship_original_part1.vertices.bin').read_bytes()))
    ii=[x[0] for x in struct.iter_unpack('<I',(base/'ship_original_part1.indices.bin').read_bytes())]
    points=[v[:3] for v in vv];floor=min(p[2] for p in points);planes={}
    for i in range(0,len(ii),3):
        a,b,c=(points[j] for j in ii[i:i+3]);n=cross(tuple(b[k]-a[k] for k in range(3)),tuple(c[k]-a[k] for k in range(3)))
        if dot(n,n)<1e-15:continue
        n=normalize(n);d=dot(n,a);distances=[dot(n,p)-d for p in points]
        if min(distances)>=-1e-6:n=tuple(-x for x in n);d=-d
        elif max(distances)>1e-6:continue
        if n[2]<-1e-6:continue
        planes[tuple(round(x,6) for x in (*n,d))]=(n,d)
    back=((-1.,0.,0.),-min(p[0] for p in points));planes[(*back[0],round(back[1],6))]=back
    if len(planes)<8:raise ValueError('Canopy has no usable convex roof support')
    constraints=[];margin=.004
    for cls,parts in crew.items():
        upper=[v[:3] for part in parts for v in part['vertices'] if v[2]>=floor+.01]
        if not upper:raise ValueError('Character upper body absent')
        for n,d in planes.values():constraints.append((cls,n,max(dot(n,p) for p in upper)-d+margin))
    best=None
    for ix in range(41):
        dx=ix*.005
        for iy in range(13):
            inward=iy*.005
            for iz in range(13):
                down=iz*.005
                cost=dx*dx+inward*inward+down*down*2
                if best and cost>=best[0]:continue
                if all(over+dot(n,(dx,-inward if cls==0 else inward,-down))<=1e-6 for cls,n,over in constraints):
                    best=(cost,dx,inward,down)
    if best is None:raise ValueError('Authored occupants do not fit the Original canopy without excessive displacement')
    _,dx,inward,down=best
    for cls,parts in crew.items():
        shift=(dx,-inward if cls==0 else inward,-down)
        for part in parts:
            part['vertices']=[struct.unpack('<12f',struct.pack('<12f',*(v[k]+shift[k] for k in range(3)),*v[3:])) for v in part['vertices']]
        facts['actors'][str(cls)]['fitted_seat_source']=[facts['actors'][str(cls)]['provider_seat_source'][k]+shift[k]/scale for k in range(3)]
        facts['actors'][str(cls)]['fit_shift_space']=list(shift)
        ps=[v[:3] for part in parts for v in part['vertices']]
        facts['actors'][str(cls)]['bounds']=[[min(p[k] for p in ps) for k in range(3)],[max(p[k] for p in ps) for k in range(3)]]
        facts['actors'][str(cls)]['max_radius']=max(math.sqrt(dot(p,p)) for p in ps)
    facts['canopy_fit']={'method':'Convex supporting roof planes plus canopy back opening, upper bodies only; translation without pose/scale edits',
        'source':'Original530 closed canopy mesh from generated-initial-ship-04; fitted adaptation, not native attachment',
        'support_planes':len(planes),'skin_margin':margin,'space_grid_step':.005,
        'forward_shift':dx,'lateral_inset':inward,'down_shift':down,
        'max_roof_overflow':max(over+dot(n,(dx,-inward if cls==0 else inward,-down))-margin for cls,n,over in constraints)}
    return facts

def valid_mesh(vertices,indices):
    if not vertices or not indices or len(indices)%3 or any(i<0 or i>=len(vertices) for i in indices):raise ValueError('Invalid cockpit triangle stream')
    for v in vertices:
        if len(v)!=12 or not all(math.isfinite(x) for x in v):raise ValueError('Invalid cockpit vertex')
        if dot(v[3:6],v[3:6])<.000001 or any(x<0 or x>2.001 for x in v[8:]):raise ValueError('Invalid cockpit normal or colour')
        if dot(v[:3],v[:3])>(TARGET_RADIUS+.000001)**2:raise ValueError('Cockpit actor escaped the unchanged hull envelope')

def build(base,iso,editor,levels,out,capture,capture_sha=CAPTURE_SHA256):
    base,iso,editor,levels,out=map(Path,(base,iso,editor,levels,out))
    if out.exists():raise ValueError('Refusing to overwrite an existing cockpit pack')
    verify_hull(base);before=file_inventory(base);source=Source(iso,editor,'pal')
    try:
        original=json.loads((base/'original-ship-extension.json').read_text(encoding='utf-8'))
        crew,facts=captured_crew(source,levels,original,Path(capture),capture_sha);fit_cockpit(crew,base,facts,original['scale'])
        facts['disc']=source.identity
        facts['metal_overlays']=[]
        for cls,parts in crew.items():
            opaque={v[:3] for part in parts if part['alpha_mode']!='BLEND' for v in part['vertices']}
            for part in parts:
                points={v[:3] for v in part['vertices']}
                part['metal_overlay']=bool(part['sphere_uv'] and part['alpha_mode']=='BLEND' and points<=opaque)
                if part['sphere_uv']:facts['metal_overlays'].append({'actor':cls,'material':part['material'],
                    'unique_positions':len(points),'coincident_opaque_positions':len(points&opaque),'overlay':part['metal_overlay']})
        entries=[];roles=[];source_files={}
        for cls,parts in crew.items():
            model=levels/'level_03/mobys'/f'moby_{cls}.glb';source_files[str(model.resolve())]=digest(model.read_bytes())
            for part in parts:
                valid_mesh(part['vertices'],part['indices']);source_files[str(part['texture'])]=digest(part['texture'].read_bytes())
        # No source or previous pack is modified. Copy after pose and fit validation.
        shutil.copytree(base,out)
        index=2
        for cls,parts in crew.items():
            for part in parts:
                name=f'ship_original_part{index}';index+=1
                vf=name+'.vertices.bin';ix=name+'.indices.bin';tex=name+'.rgba.bin'
                (out/vf).write_bytes(b''.join(struct.pack('<12f',*v) for v in part['vertices']))
                (out/ix).write_bytes(struct.pack('<'+'I'*len(part['indices']),*part['indices']))
                with Image.open(part['texture']) as source_image:
                    image=source_image.convert('RGBA');image_size=image.size;(out/tex).write_bytes(image.tobytes())
                entry={'id':name,'vertices':vf,'indices':ix,'rgba':tex,'width':image_size[0],'height':image_size[1],
                       'vertex_count':len(part['vertices']),'index_count':len(part['indices']),'vertex_stride':48,
                       'wrap_s':part['wrap_s'],'wrap_t':part['wrap_t'],'alpha_mode':part['alpha_mode'],
                       'alpha_cutoff':part['alpha_cutoff'],'sphere_uv':part['sphere_uv'],'metal_overlay':part['metal_overlay'],'source_material':part['material'],
                       'role':f'actor{cls}'}
                entries.append(entry);roles.append(f'part {name} actor{cls} {int(part["sphere_uv"])} {int(part["metal_overlay"])}')
        rows=[' '.join(str(e[k]) for k in ('id','vertices','indices','rgba','width','height','vertex_stride','wrap_s','wrap_t','alpha_mode','alpha_cutoff')) for e in entries]
        (out/'assets.tsv').write_bytes((base/'assets.tsv').read_bytes()+('\n'.join(rows)+'\n').encode('ascii'))
        assets=json.loads((base/'assets.json').read_text());assets['assets'].extend(entries)
        assets['original_ship']['cockpit']={'manifest':'original-ship.tsv','actors':[0,10],'sample':facts['sample']}
        (out/'assets.json').write_text(json.dumps(assets,indent=2)+'\n',encoding='utf-8')
        header=(base/'original-ship.tsv').read_text(encoding='ascii').replace('format 1\n','format 2\n',1)
        records=['sample 1 464 4 40','capture '+capture_sha,'part ship_original body 0 0','part ship_original_part1 glass 1 0']
        for cls,parts in crew.items():
            f=facts['actors'][str(cls)]
            records.append(f'actor {cls} {f["native_joints"]} {f["pelvis_joint"]} {len(parts)} {f["model_sha256"]} {f["cpu_class_header_sha256"]}')
        records.extend(roles)
        (out/'original-ship.tsv').write_text(header+'\n'.join(records)+'\n',encoding='ascii')
        facts.update(format=2,private=True,base_inventory_sha256=digest((base/'inventory.json').read_bytes()),
                     base_unchanged_payload={p:e for p,e in before.items() if p not in ('assets.tsv','assets.json','provenance.json','original-ship.tsv')},
                     source_files=source_files,meshes=entries,native_game_verified=False,
                     pose_scope='Baked from the validated native live0/10 matrices after Original modifiers. Hull remains the independent closed cruise pose; this is not a full-flight visual certificate.')
        (out/'original-cockpit-extension.json').write_text(json.dumps(facts,indent=2)+'\n',encoding='utf-8')
        provenance=json.loads((base/'provenance.json').read_text());provenance['original_cockpit_extension']={k:v for k,v in facts.items() if k!='base_unchanged_payload'}
        (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='utf-8')
        (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='utf-8')
        if file_inventory(base)!=before:raise RuntimeError('Base pack changed during cockpit import')
        for path,expected in source_files.items():
            if digest(Path(path).read_bytes())!=expected:raise RuntimeError('Character source changed during cockpit import')
        return verify(out)
    finally:source.close()

def verify(root):
    root=Path(root);check_inventory(root,json.loads((root/'inventory.json').read_text()))
    meta=json.loads((root/'original-cockpit-extension.json').read_text());check_inventory(root,meta['base_unchanged_payload'])
    capture_hash=meta.get('capture',{}).get('sha256','')
    if set(meta['actors'])!={'0','10'} or meta['sample']['bank']!=1 or meta['sample']['tick']!=464 or meta['sample']['key']!=40 or not re.fullmatch('[0-9a-f]{64}',capture_hash):raise ValueError('Incomplete native cockpit provenance')
    counts={0:0,10:0}
    opaque={c:set() for c in counts};overlays=[]
    for mesh in meta['meshes']:
        if mesh['role'] not in ('actor0','actor10'):raise ValueError('Unexpected cockpit role')
        raw=(root/mesh['vertices']).read_bytes();indices=(root/mesh['indices']).read_bytes()
        if len(raw)!=mesh['vertex_count']*48 or len(indices)!=mesh['index_count']*4:raise ValueError('Truncated cockpit mesh')
        vertices=list(struct.iter_unpack('<12f',raw));ii=[x[0] for x in struct.iter_unpack('<I',indices)];valid_mesh(vertices,ii)
        cls=int(mesh['role'][5:]);counts[cls]+=1
        if mesh.get('metal_overlay'):
            if not mesh['sphere_uv'] or mesh['alpha_mode']!='BLEND':raise ValueError('Invalid native cockpit overlay semantic')
            overlays.append((cls,{v[:3] for v in vertices}))
        elif mesh['alpha_mode']!='BLEND':opaque[cls].update(v[:3] for v in vertices)
    if any(not points<=opaque[cls] for cls,points in overlays):raise ValueError('Native chrome does not coincide with its opaque actor')
    if any(counts[c]!=len(meta['actors'][str(c)]['materials']) for c in counts):raise ValueError('Incomplete actor material parts')
    return {'pack':str(root),'format':2,'actors':counts,'vertices':sum(m['vertex_count'] for m in meta['meshes']),
            'triangles':sum(m['index_count']//3 for m in meta['meshes']),
            'fit':meta['canopy_fit'],'inventory_sha256':digest((root/'inventory.json').read_bytes()),
            'original_ship_tsv_sha256':digest((root/'original-ship.tsv').read_bytes()),'native_game_verified':False}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('iso','editor','levels','base','out','verify','capture'):p.add_argument('--'+name,type=Path)
    p.add_argument('--capture-sha256',default=CAPTURE_SHA256)
    a=p.parse_args()
    if a.verify:result=verify(a.verify)
    elif all((a.iso,a.editor,a.levels,a.base,a.out,a.capture)):result=build(a.base,a.iso,a.editor,a.levels,a.out,a.capture,a.capture_sha256)
    else:p.error('Provide --base --iso --editor --levels --out --capture, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

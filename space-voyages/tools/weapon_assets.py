#!/usr/bin/env python3
"""Append native Gemlik laser art and explicit MOD gun ports to a NEW pack.

531 stays unarmed. Cinematic ships532/533 are not claimed to have native
combat logic: their existing nacelle ports become integrated emitters by
an explicit mod design choice. No blue nose headlights are used as guns.
Native firing policy/projectile art come from Gemlik's different fighter69.
"""
from __future__ import annotations
import argparse,json,math,shutil,struct
from pathlib import Path
from asset_formats import bake_ship,digest,glb_read,indexed,values
from build_assets import Source,write_mesh
from flight_effects_assets import (add,cross,distance_to_segment,dot,file_inventory,
    check_inventory,gemlik_effects,magnitude,scale,ship_mounts,sub)
from station_assets import Inputs,read_model

METADATA=('assets.json','assets.tsv','provenance.json','inventory.json')

def hull2(points):
    """Convex boundary of an authored port, excluding triangulation centres."""
    points=sorted(set(tuple(p) for p in points))
    if len(points)<3:raise ValueError('Port has no polygon boundary')
    def turn(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    lo=[];hi=[]
    for p in points:
        while len(lo)>1 and turn(lo[-2],lo[-1],p)<=1e-14:lo.pop()
        lo.append(p)
    for p in reversed(points):
        while len(hi)>1 and turn(hi[-2],hi[-1],p)<=1e-14:hi.pop()
        hi.append(p)
    result=lo[:-1]+hi[:-1]
    if len(result)<3:raise ValueError('Degenerate port boundary')
    return result

def aperture_radius(position,points):
    boundary=hull2((p[1],p[2]) for p in points)
    radius=min(distance_to_segment(position,(position[0],*a),(position[0],*b))
               for a,b in zip(boundary,boundary[1:]+boundary[:1]))
    if not(.001<radius<1):raise ValueError('Invalid muzzle aperture')
    return radius,len(boundary)

def nacelle_ports_532(path):
    parts,bake=bake_ship(path)
    if len(parts)!=1:raise ValueError('Expected original single-material532')
    vertices,indices,_,_=parts[0];positions=[v[:3] for v in vertices];result=[]
    # The paired rear rings establish each original nacelle's Y/Z axis. Its
    # forward planar disk is the distinct +X facing surface on that axis.
    engines,_=ship_mounts(path,532)
    for engine in engines:
        axis=engine['position'];faces=[]
        for i in range(0,len(indices),3):
            ids=indices[i:i+3];p,q,r=(positions[j] for j in ids)
            normal=cross(sub(q,p),sub(r,p));area=magnitude(normal)
            if area<1e-12 or normal[0]/area<.95:continue
            centre=scale(add(add(p,q),r),1/3)
            if math.hypot(centre[1]-axis[1],centre[2]-axis[2])<engine['radius']*2:
                faces.append((ids,centre,area))
        if len(faces)!=12:raise ValueError('Original532 front nacelle disk topology changed')
        total=sum(f[2] for f in faces)
        centre=tuple(sum(f[1][k]*f[2] for f in faces)/total for k in range(3))
        points=[positions[j] for f in faces for j in f[0]]
        if max(abs(p[0]-centre[0]) for p in points)>1e-6:raise ValueError('Nacelle emitter disk is not planar')
        radius,count=aperture_radius(centre,points)
        result.append({'position':list(centre),'direction':[1,0,0],'aperture_radius':radius,
            'source_surface':'Original forward nacelle disk, repurposed as integrated MOD emitter; not a native gun hardpoint',
            'surface_triangles':len(faces),'boundary_vertices':count,'source_vertex_indices':sorted({j for f in faces for j in f[0]})})
    return result,bake

def nacelle_ports_533(path):
    doc,raw=glb_read(path);parts,bake=bake_ship(path)
    if len(parts)!=1:raise ValueError('Expected original single-material533')
    prim=doc['meshes'][0]['primitives'][0];attrs=prim['attributes']
    joints=values(doc,raw,attrs['JOINTS_0']);weights=values(doc,raw,attrs['WEIGHTS_0'])
    skin={node:i for i,node in enumerate(doc['skins'][0]['joints'])}
    positions=[v[:3] for v in parts[0][0]];result=[]
    for node in (5,21):
        selected=[i for i,(js,ws) in enumerate(zip(joints,weights))
                  if sum(w for j,w in zip(js,ws) if j==skin[node])>.9999]
        if len(selected)<16:raise ValueError('Missing original533 nacelle barrel')
        tip=max(positions[i][0] for i in selected)
        mouth=sorted({positions[i] for i in selected if abs(positions[i][0]-tip)<1e-7})
        if len(mouth)!=8:raise ValueError('Original533 forward barrel mouth is not an octagon')
        centre=(tip,sum(p[1] for p in mouth)/len(mouth),sum(p[2] for p in mouth)/len(mouth))
        radius,count=aperture_radius(centre,mouth)
        result.append({'position':list(centre),'direction':[1,0,0],'aperture_radius':radius,
            'source_surface':'Original forward nacelle barrel mouth, repurposed as MOD emitter; not native cinematic-ship combat',
            'authored_joint':node,'boundary_vertices':count,
            'source_vertex_indices':[i for i in selected if abs(positions[i][0]-tip)<1e-7]})
    return result,bake

def laser_source(levels,region):
    root=levels/'level_13';manifest=json.loads((root/'manifest.json').read_text())
    if (manifest.get('game'),manifest.get('version'),manifest.get('level'))!=('rac1',region,13):
        raise ValueError('Wrong regional Gemlik export')
    entry=manifest['classes']['moby']['1009'];path=root/entry['mesh'];inputs=Inputs()
    parts=read_model(path,inputs,{})
    if len(parts)!=1:raise ValueError('Original1009 laser must have one material')
    vertices,indices,key,image,factor,texture=parts[0];native_scale=entry['scale']
    if not(.39<native_scale<.40):raise ValueError('Unexpected native laser class scale')
    if len(vertices)!=456 or len(indices)!=1152:raise ValueError('Original laser topology changed')
    vv=[(*(x*native_scale for x in p),*n,*uv,*(c[k]*factor[k] for k in range(4))) for p,n,uv,c,_ in vertices]
    lo=[min(v[k] for v in vv) for k in range(3)];hi=[max(v[k] for v in vv) for k in range(3)]
    radius=max(math.hypot(v[1],v[2]) for v in vv)
    return vv,indices,image,{'native_class':1009,'native_level':13,'class_scale':native_scale,
        'vertices':len(vv),'triangles':len(indices)//3,'source_model':str(path.resolve()),
        'source_model_sha256':digest(path.read_bytes()),'source_texture':texture,'rgba_sha256':digest(image.tobytes()),
        'source_files':inputs.check(),'native_bounds':[lo,hi],'native_beam_radius':radius,
        'wrap_s':key[3],'wrap_t':key[4],
        'blend':'Native spawn mode0x200 additive SRC_ALPHA/ONE; alpha24/128, white color; GLB alphaMode alone is not runtime blend state',
        'tip_semantics':'Original maxX retained on disk; renderer shifts by -maxX before per-ship scaling so physics position is the forward tip',
        'geometry_policy':'Original vertex/normal/UV/color/index order; only original class scale applied to positions; no AI or generated rod'}

def muzzle_data(base,levels,region,laser):
    provenance=json.loads((base/'provenance.json').read_text());manifest=json.loads((levels/'level_98/manifest.json').read_text())
    if provenance.get('disc')!=region or (manifest.get('game'),manifest.get('version'),manifest.get('level'))!=('rac1',region,98):
        raise ValueError('Mixed regional ship/pack data')
    result={};records=['format 1','ship 531 0','ship 532 1','ship 533 1','tuning 400 200 4','bolt projectile_laser']
    for cl,reader in ((532,nacelle_ports_532),(533,nacelle_ports_533)):
        path=levels/'level_98/mobys'/f'moby_{cl}.glb';sha=digest(path.read_bytes());ship=provenance['ships'][str(cl)]
        if ship['model_sha256']!=sha:raise ValueError('Replacement weapons need explicit authored mounts; never guess a changed muzzle')
        muzzles,bake=reader(path)
        if any(bake[k]!=ship[k] for k in bake):raise ValueError('Muzzle/body normalization mismatch')
        body_class_scale=manifest['classes']['moby'][str(cl)]['scale']
        body_ratio=bake['scale']/body_class_scale
        port_radius=min(m['aperture_radius'] for m in muzzles)
        visual_scale=min(body_ratio,port_radius/laser['native_beam_radius'])
        if not(0<visual_scale<1):raise ValueError('Invalid native projectile visual scale')
        for m in muzzles:records.append('muzzle '+str(cl)+' '+' '.join(format(x,'.10g') for x in m['position']+m['direction']))
        records.append('bolt_scale '+str(cl)+' '+format(visual_scale,'.10g'))
        result[str(cl)]={'source_model':str(path.resolve()),'source_model_sha256':sha,'muzzles':muzzles,
            'body_native_class_scale':body_class_scale,'body_to_native_ratio':body_ratio,
            'minimum_port_radius':port_radius,'bolt_scale':visual_scale,
            'scaled_beam_radius':visual_scale*laser['native_beam_radius'],
            'scale_policy':'min(body bake/native class scale, minimum aperture radius/native beam radius); visual only'}
    return records,result

def build(base,levels,region,out):
    base,levels,out=base.resolve(),levels.resolve(),out.resolve()
    if out.exists():raise ValueError('Refusing to overwrite an asset pack')
    if out.is_relative_to(base) or base.is_relative_to(out):raise ValueError('Input/output paths overlap')
    old_inventory=json.loads((base/'inventory.json').read_text());check_inventory(base,old_inventory)
    old_metadata={name:(base/name).read_bytes() for name in METADATA}
    vv,ii,image,laser=laser_source(levels,region);records,muzzles=muzzle_data(base,levels,region,laser)
    original=json.loads(old_metadata['assets.json'])
    if any(e['id']=='projectile_laser' for e in original['assets']):raise ValueError('Laser extension already present')
    shutil.copytree(base,out)
    for name,raw in old_metadata.items():(out/('base-flightfx-01.'+name)).write_bytes(raw)
    entry=write_mesh(out,'projectile_laser',vv,ii,image,stride=48)
    entry.update({'native_level':13,'native_class':1009,'wrap_s':laser['wrap_s'],'wrap_t':laser['wrap_t'],
                  'alpha_mode':'BLEND','alpha_cutoff':.5,'double_sided':True,'role':'native additive laser; dedicated projectile renderer'})
    original['assets'].append(entry);(out/'assets.json').write_text(json.dumps(original,indent=2)+'\n',encoding='ascii')
    prefix=old_metadata['assets.tsv']
    if not prefix.endswith(b'\n'):raise ValueError('Legacy TSV must end in newline')
    row=' '.join(str(entry[k]) for k in ('id','vertices','indices','rgba','width','height','vertex_stride','wrap_s','wrap_t','alpha_mode'))+'\n'
    (out/'assets.tsv').write_bytes(prefix+row.encode('ascii'))
    (out/'weapons.tsv').write_text('\n'.join(records)+'\n',encoding='ascii')
    extension={'format':1,'private':True,'region':region,'base':str(base),'levels':str(levels),
        'base_inventory':old_inventory,'base_metadata_sha256':{k:digest(v) for k,v in old_metadata.items()},
        'policy':'531 unarmed;532/533 original forward nacelle geometry explicitly repurposed as integrated MOD weapons. No blue nose headlights or centre-ship origin.',
        'native_firing_source':'Gemlik update_fighter_jet_gun_and_aim on different fighter69: Square/L1 held, alternating emitters, cooldown4 native60Hz ticks, speed400 plus ship velocity, range200',
        'native_function_sources':['games/rac1/src/overlays/l13_gemlik/turrets_and_fighter_jet.c:2327',
            'games/rac1/src/overlays/shared/homing_missile_and_water_sheet.c:102',
            'port/game/common/frontend/frame.cpp:1094'],
        'rewards':'No reward, inventory, mission, visit or save code is imported by this asset extension',
        'laser':laser,'ships':muzzles,
        'effects':'impact/explosion sprites omitted until their exact native particle texture source is verified; no invented substitute'}
    (out/'weapons-extension.json').write_text(json.dumps(extension,indent=2)+'\n',encoding='ascii')
    provenance=json.loads(old_metadata['provenance.json']);provenance['space_weapons']=extension
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='ascii')
    (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='ascii')
    return verify(out)

def verify_weapons(root):
    root=root.resolve();e=json.loads((root/'weapons-extension.json').read_text());base=Path(e['base'])
    check_inventory(root,json.loads((root/'inventory.json').read_text()));check_inventory(base,e['base_inventory'])
    for name,sha in e['base_metadata_sha256'].items():
        if digest((base/name).read_bytes())!=sha or digest((root/('base-flightfx-01.'+name)).read_bytes())!=sha:raise ValueError('Base metadata drift')
    for name,expected in e['base_inventory'].items():
        if name not in METADATA and digest((root/name).read_bytes())!=expected['sha256']:raise ValueError('An earlier pack payload changed')
    old=json.loads((base/'assets.json').read_text());new=json.loads((root/'assets.json').read_text())
    if new['assets'][:-1]!=old['assets'] or new['assets'][-1]['id']!='projectile_laser':raise ValueError('Existing mesh catalog changed')
    if not(root/'assets.tsv').read_bytes().startswith((base/'assets.tsv').read_bytes()):raise ValueError('Existing TSV prefix changed')
    vv,ii,image,laser=laser_source(Path(e['levels']),e['region'])
    vertices=b''.join(struct.pack('<12f',*v) for v in vv);indices=struct.pack('<'+'I'*len(ii),*ii)
    if (root/'projectile_laser.vertices.bin').read_bytes()!=vertices or (root/'projectile_laser.indices.bin').read_bytes()!=indices or (root/'projectile_laser.rgba.bin').read_bytes()!=image.tobytes():
        raise ValueError('Native laser geometry, UVs, colours, indices or texels changed')
    records,muzzles=muzzle_data(base,Path(e['levels']),e['region'],laser)
    if (root/'weapons.tsv').read_text()!='\n'.join(records)+'\n' or muzzles!=e['ships']:raise ValueError('Weapon port metadata changed')
    for ship in muzzles.values():
        if ship['scaled_beam_radius']>ship['minimum_port_radius']+1e-8:raise ValueError('Laser exceeds muzzle aperture')
    return {'verified':True,'output':str(root),'region':e['region'],'base_payloads_byte_identical':True,
        'meshes':len(new['assets']),'laser_vertices':len(vv),'laser_triangles':len(ii)//3,
        'unarmed_ship_class':531,'muzzle_counts':{k:len(v['muzzles']) for k,v in muzzles.items()},
        'bolt_scales':{k:v['bolt_scale'] for k,v in muzzles.items()},
        'beam_fits_both_port_apertures':True,'weapons_tsv_sha256':digest((root/'weapons.tsv').read_bytes())}

RETICLE_SLOTS=(('ring',17,64,40),('marks',18,64,40),('center',8,32,10))
VISUAL_GROWTH=((532,3,6),(533,6,6))

def reticle_data(source):
    read=gemlik_effects(source);payloads={};records=[];provenance={}
    for name,slot,size,draw_size in RETICLE_SLOTS:
        ii,pp,w,h=read(slot)
        if (w,h)!=(size,size):raise ValueError('Unexpected native Gemlik reticle dimensions')
        rgba=indexed(ii,pp,w,h).tobytes();filename='reticle-'+name+'.rgba.bin'
        payloads[filename]=rgba;records.append(f'reticle {name} {filename} {w} {h} 1 1 1 0')
        provenance[name]={'native_level':13,'effect_slot':slot,'width':w,'height':h,
            'rgba_sha256':digest(rgba),'indices_sha256':digest(ii),'native_clut_sha256':digest(pp),
            'native_draw_size':[draw_size,draw_size],
            'native_uv':[1/size,1/size,(size-1)/size,(size-1)/size],
            'conversion':'Original RGB, CSM1 logical CLUT and PS2 alpha times2 clamped255; no upscale/recolour/generated frames'}
    return payloads,records,provenance

def build_visibility(base,iso,editor,region,out):
    base,out=base.resolve(),out.resolve()
    if out.exists():raise ValueError('Refusing to overwrite an asset pack')
    if out.is_relative_to(base) or base.is_relative_to(out):raise ValueError('Input/output paths overlap')
    # Validate the complete48-mesh02pack before adding metadata; all old data
    # remain immutable. This intentionally does not rewrite laser geometry.
    base_result=verify(base)
    old_inventory=json.loads((base/'inventory.json').read_text())
    old_metadata={name:(base/name).read_bytes() for name in ('weapons.tsv','provenance.json','inventory.json')}
    source=Source(iso,editor,region)
    try:
        if source.region!=base_result['region']:raise ValueError('Mixed regional reticle/pack data')
        payloads,records,provenance=reticle_data(source);identity=source.identity
    finally:source.close()
    prefix=old_metadata['weapons.tsv']
    if not prefix.endswith(b'\n') or b'\nreticle ' in prefix or b'\nbolt_growth ' in prefix:
        raise ValueError('Expected an unextended flightfx02 weapons manifest')
    records.extend(f'bolt_growth {cl} {amount} {distance}' for cl,amount,distance in VISUAL_GROWTH)
    extension={'format':1,'private':True,'region':identity['region'],'base':str(base),
        'iso':str(iso.resolve()),'editor':str(editor.resolve()),'disc_identity':identity,
        'base_inventory':old_inventory,'base_metadata_sha256':{k:digest(v) for k,v in old_metadata.items()},
        'reticle':provenance,'added_raw_bytes':sum(map(len,payloads.values())),
        'original_draw':'DrawGemlikTurretHud: FX17+18 at40x40, FX8 at10x10, angle0, central pivot.5/.5, UVpixel1..size-1',
        'original_color':'GS ABGR0xFF20FF20 => RGBA32/255/32/255 divided128; multiply original texel then clamp final alpha; no baked tint',
        'aiming':'Project shippos+normalize(aimstep+shipvelocity)*23, only for armed mod ships532/533',
        'bolt_growth':{str(cl):{'multiplier':amount,'distance':distance} for cl,amount,distance in VISUAL_GROWTH},
        'adaptation':'Visual plasma expansion from measured bore-fit birth radius to3x532/6x533 after6worldunits. Explicit MOD adaptation, not claimed1:1 native laser geometry/appearance.',
        'invariants':'Original laser mesh/texture/alpha24/128, birth fit, cannon positions, speed/range/cadence, physics radius and all prior scene assets unchanged'}
    shutil.copytree(base,out)
    for name,raw in old_metadata.items():(out/('base-flightfx-02.'+name)).write_bytes(raw)
    for name,raw in payloads.items():(out/name).write_bytes(raw)
    (out/'weapons.tsv').write_bytes(prefix+('\n'.join(records)+'\n').encode('ascii'))
    (out/'visibility-extension.json').write_text(json.dumps(extension,indent=2)+'\n',encoding='ascii')
    overall=json.loads(old_metadata['provenance.json']);overall['weapon_visibility']=extension
    (out/'provenance.json').write_text(json.dumps(overall,indent=2)+'\n',encoding='ascii')
    (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='ascii')
    return verify_visibility(out)

def verify_visibility(root):
    root=root.resolve();e=json.loads((root/'visibility-extension.json').read_text());base=Path(e['base'])
    check_inventory(root,json.loads((root/'inventory.json').read_text()));check_inventory(base,e['base_inventory'])
    for name,expected in e['base_inventory'].items():
        if name not in ('weapons.tsv','provenance.json','inventory.json') and digest((root/name).read_bytes())!=expected['sha256']:
            raise ValueError('Original flightfx02 payload changed')
    for name,sha in e['base_metadata_sha256'].items():
        if digest((base/name).read_bytes())!=sha or digest((root/('base-flightfx-02.'+name)).read_bytes())!=sha:
            raise ValueError('Original flightfx02 metadata changed')
    source=Source(Path(e['iso']),Path(e['editor']),e['region'])
    try:
        payloads,records,provenance=reticle_data(source)
        if source.identity!=e['disc_identity']:raise ValueError('Native reticle source disc drift')
    finally:source.close()
    if provenance!=e['reticle']:raise ValueError('Native reticle provenance changed')
    for name,raw in payloads.items():
        if (root/name).read_bytes()!=raw:raise ValueError('Original reticle RGB/alpha texels changed')
    records.extend(f'bolt_growth {cl} {amount} {distance}' for cl,amount,distance in VISUAL_GROWTH)
    if (root/'weapons.tsv').read_bytes()!=(base/'weapons.tsv').read_bytes()+('\n'.join(records)+'\n').encode('ascii'):
        raise ValueError('Visibility metadata did not preserve original weapon rules')
    return {'verified':True,'output':str(root),'region':e['region'],'meshes':len(json.loads((root/'assets.json').read_text())['assets']),
        'base_payloads_byte_identical':True,'original_laser_and_physics_metadata_unchanged':True,
        'reticle_native_slots':[17,18,8],'added_raw_bytes':sum(map(len,payloads.values())),
        'visual_growth':e['bolt_growth'],'weapons_tsv_sha256':digest((root/'weapons.tsv').read_bytes())}

def verify(root):
    return verify_visibility(root) if (root/'visibility-extension.json').exists() else verify_weapons(root)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','levels','out'):p.add_argument('--'+name,type=Path)
    p.add_argument('--visibility',action='store_true',help='Extend flightfx02 to a NEW03pack with native reticle and explicit visual-only plasma growth')
    p.add_argument('--iso',type=Path);p.add_argument('--editor',type=Path)
    p.add_argument('--region',choices=('pal','ntsc'));p.add_argument('--verify',type=Path);a=p.parse_args()
    if a.verify:result=verify(a.verify)
    elif a.visibility and all((a.base,a.iso,a.editor,a.out)):result=build_visibility(a.base,a.iso,a.editor,a.region or 'auto',a.out)
    elif all((a.base,a.levels,a.region,a.out)):result=build(a.base,a.levels,a.region,a.out)
    else:p.error('Provide --base --levels --region --out, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

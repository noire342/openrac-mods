#!/usr/bin/env python3
"""Append native Gemlik health/flight FX to a NEW private 0.4 asset pack.

Never writes to a disc, level export or previous pack. The body, sky, font,
asteroids and destination assets are copied byte-for-byte. No artwork is
distributed with this script. Exhaust mounts follow the *baked* nozzle rings,
not the differently posed native cinematic glow-offset table.
"""
from __future__ import annotations
import argparse, json, math, shutil, struct
from pathlib import Path
from PIL import Image
from asset_formats import bake_ship, bounded, digest, glb_read, indexed, values
from build_assets import Source

ENGINE_JOINTS = {531:(6,5,7,4,25,24,26,23),532:(19,20),533:(30,31)}
NATIVE_COLORS = {531:(176/128,88/128,32/128,1),
                 532:(176/128,88/128,32/128,1),533:(0,1,48/128,1)}
BOOT_TABLES = {'pal':(0x1605c0,(0x1d9c40,0x1d9cc0,0x1d9ce0)),
               'ntsc':(0x1604c0,(0x1d98b0,0x1d9930,0x1d9950))}

def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def scale(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def magnitude(a): return math.sqrt(dot(a,a))
def unit(a):
    n=magnitude(a)
    if n<1e-12: raise ValueError('Degenerate exhaust normal')
    return scale(a,1/n)

def logical_palette(raw):
    """PS2 CSM1 CLUT permutation, original RGB and 0..128 alpha -> RGBA8."""
    if len(raw)!=1024: raise ValueError('Native CLUT must contain 256 entries')
    out=bytearray()
    for i in range(256):
        j=(i&~24)|((i&8)<<1)|((i&16)>>1)
        r,g,b,a=raw[j*4:j*4+4];out.extend((r,g,b,min(255,a*2)))
    return bytes(out)

def gemlik_effects(source):
    data=source.level_data(13);sections=[]
    for section in (2,10):
        at,size=struct.unpack_from('<II',data,section*8)
        part=bounded(data,at,size);sections.append(source.wad(part) if part[:3]==b'WAD' else part)
    index,core=sections;count,table=struct.unpack_from('<II',index,0x58)
    bank=struct.unpack_from('<I',index,0x68)[0]
    if count<=50 or count>512: raise ValueError('Missing native Gemlik effects')
    def read(slot):
        palette,pixels,w,h=struct.unpack_from('<4i',index,table+slot*16)
        if not(0<w<=2048 and 0<h<=2048): raise ValueError('Invalid native effect dimensions')
        ii=bounded(core,bank+pixels,w*h);pp=bounded(core,bank+palette,1024)
        return ii,pp,w,h
    return read

def elf_bytes(boot,address,size):
    """Read a verified source ELF's actual PT_LOAD mapping, never a RAM delta."""
    if boot[:7]!=b'\x7fELF\x01\x01\x01': raise ValueError('Expected native ELF32 little endian boot')
    phoff=struct.unpack_from('<I',boot,28)[0];step,count=struct.unpack_from('<HH',boot,42)
    if step<32 or count>128: raise ValueError('Invalid ELF program headers')
    for i in range(count):
        typ,offset,va,_,length,_,_,_=struct.unpack('<8I',bounded(boot,phoff+i*step,32))
        if typ==1 and va<=address and address+size<=va+length:
            return bounded(boot,offset+address-va,size)
    raise ValueError('Native constant table outside ELF load segment')

def native_tables(boot,region):
    counts_at,tables_at=BOOT_TABLES[region]
    counts=struct.unpack('<3i',elf_bytes(boot,counts_at,12))
    if counts!=(8,2,2): raise ValueError('Unexpected original exhaust count')
    return {'counts_address':hex(counts_at),'counts':list(counts),
            'positions':{str(cl):{'address':hex(at),'rows':[list(row) for row in struct.iter_unpack('<4f',elf_bytes(boot,at,n*16))]}
                         for cl,at,n in zip((531,532,533),tables_at,counts)},
            'use':'Reference only: native cinematic object-space quads; not applied unchanged to static baked ships'}

def distance_to_segment(p,a,b):
    ab=sub(b,a);n=dot(ab,ab)
    t=max(0.,min(1.,dot(sub(p,a),ab)/n)) if n>1e-20 else 0.
    return magnitude(sub(p,add(a,scale(ab,t))))

def nozzle_mount(positions,triangles):
    """An authored annular nozzle's outward faces define its aperture.

    Positions have already been skinned and normalized identically to the body.
    Use only backward-facing ring faces. The area centroid defines the port
    centre, its area normal the outward axis. Place it on the outermost plane;
    the inner aperture is the nearest projected ring edge. No world-space or
    ship-specific hand-tuned offsets enter this calculation.
    """
    faces=[]
    for tri in triangles:
        p,q,r=(positions[i] for i in tri);normal=cross(sub(q,p),sub(r,p));area=magnitude(normal)
        if area>1e-12 and normal[0]/area<-.5:
            faces.append((tri,scale(add(add(p,q),r),1/3),normal,area))
    if len(faces)<8: raise ValueError('Original nozzle ring has too few outward faces')
    total=sum(f[3] for f in faces);centre=tuple(sum(f[1][k]*f[3] for f in faces)/total for k in range(3))
    direction=unit(tuple(sum(f[2][k] for f in faces) for k in range(3)))
    selected={i for tri,*_ in faces for i in tri}
    plane=max(dot(sub(positions[i],centre),direction) for i in selected)
    position=add(centre,scale(direction,plane))
    projected={i:sub(positions[i],scale(direction,dot(sub(positions[i],centre),direction))) for i in selected}
    radius=min(distance_to_segment(centre,projected[tri[k]],projected[tri[(k+1)%3]])
               for tri,*_ in faces for k in range(3))
    if not(.005<radius<1) or direction[0]>=-.5: raise ValueError('Invalid authored nozzle aperture')
    return {'position':list(position),'direction':list(direction),'radius':radius,
            'length':radius*4,'ring_triangles':len(faces),'ring_centroid':list(centre),
            'outer_plane_offset':plane,'radius_method':'distance from area centre to nearest projected ring edge',
            'length_method':'4 times aperture radius; volumetric adaptation, not an original flame length'}

def ship_mounts(path,ship_class):
    doc,raw=glb_read(path);parts,bake=bake_ship(path)
    skin=doc['skins'][0];joint_to_skin={node:i for i,node in enumerate(skin['joints'])}
    primitives=[p for node in doc['nodes'] if 'mesh' in node for p in doc['meshes'][node['mesh']]['primitives']]
    if len(primitives)!=len(parts): raise ValueError('Ship primitive bake mismatch')
    result=[]
    for joint in ENGINE_JOINTS[ship_class]:
        target=joint_to_skin[joint];pp=[];ii=[]
        for primitive,part in zip(primitives,parts):
            vertices,indices,_,_=part;attrs=primitive['attributes']
            joints=values(doc,raw,attrs['JOINTS_0']);weights=values(doc,raw,attrs['WEIGHTS_0'])
            # Requiring exclusively weighted vertices prevents unrelated hull
            # faces being mistaken for an engine when a replacement changes rig.
            selected=[sum(w for j,w in zip(js,ws) if j==target)>.9999 for js,ws in zip(joints,weights)]
            base=len(pp);pp.extend(v[:3] for v in vertices)
            for i in range(0,len(indices),3):
                tri=indices[i:i+3]
                if all(selected[k] for k in tri): ii.append(tuple(base+k for k in tri))
        mount=nozzle_mount(pp,ii);mount['authored_joint']=joint;result.append(mount)
    return result,bake

def file_inventory(root):
    return {p.relative_to(root).as_posix():{'bytes':p.stat().st_size,'sha256':digest(p.read_bytes())}
            for p in sorted(root.rglob('*')) if p.is_file() and p.name!='inventory.json'}

def check_inventory(root,inventory):
    for name,entry in inventory.items():
        p=(root/name).resolve()
        if not p.is_relative_to(root.resolve()) or not p.is_file() or p.stat().st_size!=entry['bytes'] or digest(p.read_bytes())!=entry['sha256']:
            raise ValueError('Asset inventory mismatch: '+name)

def extension_data(source,boot,levels,base,overrides=None):
    provenance=json.loads((base/'provenance.json').read_text());region=source.region
    if provenance.get('disc')!=region: raise ValueError('Cannot mix asset-pack and disc regions')
    manifest=json.loads((levels/'level_98/manifest.json').read_text())
    if (manifest.get('game'),manifest.get('version'),manifest.get('level'))!=('rac1',region,98):
        raise ValueError('Cannot mix level-export and disc regions')
    effects=gemlik_effects(source);health_i,health_p,w,h=effects(49)
    if (w,h)!=(128,128): raise ValueError('Native Gemlik health is not 128 square')
    health_palette=logical_palette(health_p);flame_i,flame_p,fw,fh=effects(5)
    if (fw,fh)!=(32,32): raise ValueError('Native Gemlik glow is not 32 square')
    flame=indexed(flame_i,flame_p,fw,fh)
    payloads={'health.indices.bin':health_i,'health.palette.rgba.bin':health_palette,'flame.rgba.bin':flame.tobytes()}
    records=['format 1',f'health health.indices.bin health.palette.rgba.bin {w} {h}',
             f'sprite flame flame.rgba.bin {fw} {fh} 1 1 1 0']
    mounts={};sources={};overrides=overrides or {}
    for cl in ENGINE_JOINTS:
        path=levels/'level_98/mobys'/f'moby_{cl}.glb';sha=digest(path.read_bytes())
        base_ship=provenance['ships'][str(cl)];expected=base_ship['model_sha256']
        if sha!=expected:
            entry=overrides.get(str(cl))
            if not entry or entry.get('model_sha256')!=expected: raise ValueError(f'Ship {cl} replacement requires explicit matching mount metadata')
            mm=entry['mounts'];origin='Explicit replacement metadata matching baked model SHA-256'
        else:
            mm,bake=ship_mounts(path,cl)
            if bake!= {k:base_ship[k] for k in bake}: raise ValueError('Mount/body normalization mismatch')
            origin='Original authored nozzle-ring joint surfaces, sampled/skinned sequence1 frame0 exactly as body'
        if not(0<len(mm)<=16): raise ValueError('Invalid replacement mount count')
        for m in mm:
            numbers=m['position']+m['direction']+[m['radius'],m['length']]
            if len(numbers)!=8 or not all(math.isfinite(x) for x in numbers) or abs(magnitude(m['direction'])-1)>.001 or magnitude(m['position'])>16 or not(0<m['radius']<=4) or not(0<m['length']<=64):
                raise ValueError('Invalid exhaust metadata')
            records.append('mount '+str(cl)+' '+' '.join(format(x,'.10g') for x in numbers))
        records.append('color '+str(cl)+' '+' '.join(format(x,'.10g') for x in NATIVE_COLORS[cl]))
        mounts[str(cl)]={'source':origin,'source_model_sha256':sha,'baked_model_sha256':expected,'mounts':mm,'color':list(NATIVE_COLORS[cl])}
        sources[str(path.resolve())]=sha
    extension={'format':1,'private':True,'region':region,'disc_identity':source.identity,
        'health':{'native_level':13,'effect_slot':49,'width':w,'height':h,'indices_sha256':digest(health_i),
                  'native_clut_sha256':digest(health_p),'logical_rgba_sha256':digest(health_palette),
                  'threshold':'logical index j < fill restored; j >= fill opaque black, except native entries0/1; fill2..253',
                  'native_draw':'DrawGemlikTurretHud: health and radar share the same 128x128 quad; health drawn before radarFX50'},
        'flame':{'native_level':13,'effect_slot':5,'width':fw,'height':fh,'columns':1,'rows':1,'frames':1,'fps':0,
                 'rgba_sha256':digest(flame.tobytes()),'source':'Gemlik resident glow FX5; DrawResidentEffectQuads calls GetEffectTex(5)',
                 'caveat':'Transition WAD exposes only FX0..3; title FX5 is different. This is explicitly Gemlik FX5, not a claimed level98 sprite.'},
        'native_cinematic_reference':native_tables(boot,region),'ships':mounts,'source_models':sources,
        'replacement_policy':'Texture-only changes retain mounts. Different nozzle geometry/rig requires explicit model-SHA-bound mount metadata; never guess anchors.',
        'sprite_policy':'Original texels and CLUT conversion only; no AI, recolouring, upscale or fabricated animation frames',
        'color_policy':'Original ABGR RGB bytes divided by128 (GS MODULATE); brightness alpha separate from RGB; stored alpha1',
        'optional_effects':'hit/explosion not included: no separately verified native sprite selected'}
    return payloads,records,extension

def build(base,levels,iso,editor,region,out,overrides=None):
    base,levels,out=base.resolve(),levels.resolve(),out.resolve()
    if out.exists(): raise ValueError('Refusing to overwrite a pack')
    if out.is_relative_to(base) or base.is_relative_to(out): raise ValueError('Pack paths overlap')
    old=json.loads((base/'inventory.json').read_text());check_inventory(base,old)
    old_provenance=(base/'provenance.json').read_bytes();old_inventory=(base/'inventory.json').read_bytes()
    source=Source(iso,editor,region)
    try:
        from disc import Disc
        with Disc(iso) as disc:boot=disc.executable()
        payloads,records,extension=extension_data(source,boot,levels,base,overrides)
    finally: source.close()
    extension.update({'base':str(base),'base_inventory':old,
                      'base_provenance_sha256':digest(old_provenance),'base_inventory_sha256':digest(old_inventory)})
    shutil.copytree(base,out)
    (out/'base-0.4.provenance.json').write_bytes(old_provenance)
    (out/'base-0.4.inventory.json').write_bytes(old_inventory)
    for name,raw in payloads.items():(out/name).write_bytes(raw)
    (out/'flight-effects.tsv').write_text('\n'.join(records)+'\n',encoding='ascii')
    (out/'flightfx-extension.json').write_text(json.dumps(extension,indent=2)+'\n',encoding='ascii')
    provenance=json.loads(old_provenance);provenance['flight_effects']=extension
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='ascii')
    (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='ascii')
    return verify(out)

def verify(root):
    root=root.resolve();e=json.loads((root/'flightfx-extension.json').read_text())
    check_inventory(root,json.loads((root/'inventory.json').read_text()))
    base=Path(e['base']);check_inventory(base,e['base_inventory'])
    for name,expected in e['base_inventory'].items():
        if name in ('provenance.json','inventory.json'):continue
        if digest((root/name).read_bytes())!=expected['sha256']: raise ValueError('Existing asset payload changed: '+name)
    if digest((root/'base-0.4.provenance.json').read_bytes())!=e['base_provenance_sha256'] or digest((root/'base-0.4.inventory.json').read_bytes())!=e['base_inventory_sha256']:
        raise ValueError('Base metadata copy changed')
    if (root/'assets.tsv').read_bytes()!=(base/'assets.tsv').read_bytes(): raise ValueError('Legacy mesh records changed')
    ii=(root/'health.indices.bin').read_bytes();pp=(root/'health.palette.rgba.bin').read_bytes();ff=(root/'flame.rgba.bin').read_bytes()
    if len(ii)!=128*128 or len(pp)!=1024 or len(ff)!=32*32*4: raise ValueError('Effect byte count mismatch')
    if digest(ii)!=e['health']['indices_sha256'] or digest(pp)!=e['health']['logical_rgba_sha256'] or digest(ff)!=e['flame']['rgba_sha256']:
        raise ValueError('Effect source bytes changed')
    for path,sha in e['source_models'].items():
        if digest(Path(path).read_bytes())!=sha: raise ValueError('Source model drift')
    counts={k:len(v['mounts']) for k,v in e['ships'].items()}
    if counts!={'531':8,'532':2,'533':2} and not any('Explicit replacement' in v['source'] for v in e['ships'].values()):
        raise ValueError('Native exhaust mount count mismatch')
    return {'verified':True,'output':str(root),'region':e['region'],'base_payloads_byte_identical':True,
            'health_dimensions':[128,128],'flame_dimensions':[32,32],'mount_counts':counts,
            'added_raw_bytes':len(ii)+len(pp)+len(ff),'native_tables_verified_from_boot':True,
            'flight_effects_tsv_sha256':digest((root/'flight-effects.tsv').read_bytes())}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','levels','iso','editor','out','mount-overrides'):p.add_argument('--'+name,type=Path)
    p.add_argument('--region',choices=('pal','ntsc','auto'),default='auto');p.add_argument('--verify',type=Path);a=p.parse_args()
    if a.verify:result=verify(a.verify)
    elif all((a.base,a.levels,a.iso,a.editor,a.out)):
        result=build(a.base,a.levels,a.iso,a.editor,a.region,a.out,json.loads(a.mount_overrides.read_text()) if a.mount_overrides else None)
    else:p.error('Provide --base --levels --iso --editor --out, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

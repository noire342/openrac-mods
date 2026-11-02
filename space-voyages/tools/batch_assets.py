#!/usr/bin/env python3
"""Lossless shader-aware atlases for PRIVATE full-galaxy scene assets.

Source geometry, UVs, colors and image texels remain unchanged. The fragment
shader wraps the interpolated UV before mapping it into its padded atlas tile.
No mipmap, mesh simplification, vertex omission or runtime API is introduced.
"""
from __future__ import annotations
from array import array
import argparse,json,math,shutil,struct,sys
from pathlib import Path
from PIL import Image
from asset_formats import digest

REPEAT=10497;CLAMP=33071;MAX_VERTICES=262144;GUTTER=2
ROOTS=('station_blarg','station_gemlik','fleet_drek')

def power2(value):return 1<<(max(1,value)-1).bit_length()

def pack_tiles(tiles,maximum=2048):
    """Deterministic shelves, testing power-of-two widths; rects include gutters.

    Power-of-two atlas dimensions make every rect component exactly
    representable as float32, including all integer pixel offsets.
    """
    if not tiles:raise ValueError('Cannot pack an empty atlas')
    minimum=power2(max(t['width']+2*GUTTER for t in tiles))
    candidates=[];width=minimum
    ordered=sorted(tiles,key=lambda t:(-t['height'],-t['width'],t['key']))
    while width<=maximum:
        shelves=[];placements={};bottom=0;failed=False
        for tile in ordered:
            tw,th=tile['width']+2*GUTTER,tile['height']+2*GUTTER
            fitting=[(width-s['x']-tw,i,s) for i,s in enumerate(shelves) if s['height']>=th and s['x']+tw<=width]
            if fitting:
                _,_,shelf=min(fitting,key=lambda q:(q[0],q[1]))
            else:
                shelf={'y':bottom,'height':th,'x':0};shelves.append(shelf);bottom+=th
                if bottom>maximum:failed=True;break
            placements[tile['key']]=(shelf['x'],shelf['y']);shelf['x']+=tw
        if not failed:
            height=power2(bottom)
            if height<=maximum:candidates.append((width*height,max(width,height),abs(width-height),width,height,placements))
        width*=2
    if not candidates:raise ValueError(f'Atlas cannot fit within {maximum} pixels')
    _,_,_,width,height,placements=min(candidates,key=lambda p:p[:5])
    return width,height,placements

def padded_tile(image,wrap_s,wrap_t,gutter=GUTTER):
    """Opposite-edge gutters for REPEAT, duplicated-edge gutters for CLAMP."""
    image=image.convert('RGBA');w,h=image.size
    if wrap_s not in (REPEAT,CLAMP) or wrap_t not in (REPEAT,CLAMP):raise ValueError('Unsupported source sampler')
    if gutter<1 or gutter>min(w,h):raise ValueError('Invalid gutter size')
    out=Image.new('RGBA',(w+2*gutter,h+2*gutter));out.paste(image,(gutter,gutter))
    if wrap_s==REPEAT:
        left=image.crop((w-gutter,0,w,h));right=image.crop((0,0,gutter,h))
    else:
        left=image.crop((0,0,1,h)).resize((gutter,h),Image.Resampling.NEAREST)
        right=image.crop((w-1,0,w,h)).resize((gutter,h),Image.Resampling.NEAREST)
    out.paste(left,(0,gutter));out.paste(right,(gutter+w,gutter))
    # Copy padded rows, so corner texels obey both independent axis samplers.
    for row in range(gutter):
        top=(h-gutter+row if wrap_t==REPEAT else 0)+gutter
        bottom=(row if wrap_t==REPEAT else h-1)+gutter
        out.paste(out.crop((0,top,out.width,top+1)),(0,row))
        out.paste(out.crop((0,bottom,out.width,bottom+1)),(0,gutter+h+row))
    return out

def make_atlas(tiles,wrap_s,wrap_t,maximum=2048):
    width,height,positions=pack_tiles(tiles,maximum);image=Image.new('RGBA',(width,height))
    rects={};pixels={}
    for tile in tiles:
        x,y=positions[tile['key']];w,h=tile['width'],tile['height']
        source=Image.frombytes('RGBA',(w,h),tile['rgba'])
        image.paste(padded_tile(source,wrap_s,wrap_t),(x,y))
        rects[tile['key']]=(w/width,h/height,(x+GUTTER)/width,(y+GUTTER)/height)
        pixels[tile['key']]=(x+GUTTER,y+GUTTER,w,h)
    return image,rects,pixels

def chunks(entries,maximum=MAX_VERTICES):
    """Keep each original material intact, and never split/remap its vertices."""
    result=[];current=[];count=0
    for entry in entries:
        n=entry['vertex_count']
        if n>maximum:raise ValueError('An original material exceeds the declared vertex cap')
        if current and count+n>maximum:result.append(current);current=[];count=0
        current.append(entry);count+=n
    if current:result.append(current)
    return result

def expand_vertices(source,rect):
    if len(source)%48:raise ValueError('Source static vertex buffer must have stride48')
    result=bytearray(len(source)//48*64);rect_bytes=struct.pack('<4f',*rect)
    for n in range(len(source)//48):
        result[n*64:n*64+48]=source[n*48:n*48+48]
        result[n*64+48:(n+1)*64]=rect_bytes
    return result

def bilinear(image,u,v,wrap_s=CLAMP,wrap_t=CLAMP):
    """CPU oracle for GL_LINEAR normalized coordinates, without mipmaps."""
    w,h=image.size;px=u*w-.5;py=v*h-.5;x=math.floor(px);y=math.floor(py);fx=px-x;fy=py-y
    def index(i,n,mode):return i%n if mode==REPEAT else min(n-1,max(0,i))
    result=[0.]*4
    for dy,wy in ((0,1-fy),(1,fy)):
        for dx,wx in ((0,1-fx),(1,fx)):
            color=image.getpixel((index(x+dx,w,wrap_s),index(y+dy,h,wrap_t)))
            for c in range(4):result[c]+=color[c]*wx*wy
    return tuple(result)

def atlas_sample(image,rect,u,v,wrap_s,wrap_t):
    # This operation belongs in the fragment shader, AFTER UV interpolation.
    wrap=lambda t,mode:t-math.floor(t) if mode==REPEAT else min(1,max(0,t))
    return bilinear(image,rect[2]+wrap(u,wrap_s)*rect[0],rect[3]+wrap(v,wrap_t)*rect[1])

def check_bilinear(source,atlas,rect,wrap_s,wrap_t):
    coordinates=(-2.4,-1.,-1e-6,0.,1e-6,.125,.5,.999999,1.,1.000001,1.9,3.)
    maximum=0.
    for u in coordinates:
        for v in coordinates:
            original=bilinear(source,u,v,wrap_s,wrap_t);mapped=atlas_sample(atlas,rect,u,v,wrap_s,wrap_t)
            maximum=max(maximum,max(abs(a-b) for a,b in zip(original,mapped)))
    if maximum>1e-7:raise ValueError(f'Atlas bilinear sampling mismatch: {maximum}')
    return maximum

def build(source,out,maximum=2048):
    source=source.resolve();out=out.resolve()
    if out.exists():raise ValueError('Refusing to overwrite any asset build')
    manifest=json.loads((source/'assets.json').read_text());original_provenance=json.loads((source/'provenance.json').read_text())
    if not manifest.get('full_galaxy'):raise ValueError('Batch input must be a full-galaxy build')
    inventory=json.loads((source/'inventory.json').read_text())
    # Verify all original bytes before consuming or copying the build.
    for name,expected in inventory.items():
        raw=(source/name).read_bytes()
        if len(raw)!=expected['bytes'] or digest(raw)!=expected['sha256']:raise ValueError(f'Invalid source inventory: {name}')
    out.mkdir(parents=True);entries=[];groups={};artificial_names=set()
    for entry in manifest['assets']:
        if entry.get('assembly') in ROOTS:
            if entry.get('vertex_stride')!=48:raise ValueError('Expected native color vertex48 input')
            key=(entry['assembly'],entry['wrap_s'],entry['wrap_t'],entry['alpha_mode'],entry.get('alpha_cutoff',.5),entry.get('double_sided',True))
            groups.setdefault(key,[]).append(entry)
            artificial_names.update((entry['vertices'],entry['indices'],entry['rgba'],entry['id']+'.png'))
        else:entries.append(entry.copy())
    for file in source.iterdir():
        if file.is_file() and file.name not in artificial_names|{'assets.json','assets.tsv','provenance.json','inventory.json'}:shutil.copyfile(file,out/file.name)
    counters=dict.fromkeys(ROOTS,0);records=[];atlas_records=[];sample_error=0.
    source_identity={'manifest_sha256':digest((source/'assets.json').read_bytes()),'inventory_sha256':digest((source/'inventory.json').read_bytes())}
    for group_index,(key,members) in enumerate(groups.items()):
        assembly,wrap_s,wrap_t,alpha,cutoff,double_sided=key;tiles={};source_tiles={}
        for entry in members:
            rgba=(source/entry['rgba']).read_bytes();tile_key=f'{entry["width"]}x{entry["height"]}_{digest(rgba)}'
            tiles.setdefault(tile_key,{'key':tile_key,'width':entry['width'],'height':entry['height'],'rgba':rgba})
            source_tiles[entry['id']]=tile_key
        image,rects,pixel_rects=make_atlas(list(tiles.values()),wrap_s,wrap_t,maximum)
        atlas_name=f'atlas_{assembly}_g{group_index}';rgba_name=atlas_name+'.rgba.bin'
        image.save(out/(atlas_name+'.png'));(out/rgba_name).write_bytes(image.tobytes())
        for tile in tiles.values():sample_error=max(sample_error,check_bilinear(Image.frombytes('RGBA',(tile['width'],tile['height']),tile['rgba']),image,rects[tile['key']],wrap_s,wrap_t))
        group_record={'assembly':assembly,'atlas':rgba_name,'width':image.width,'height':image.height,'source_materials':len(members),'unique_tiles':len(tiles),'wrap_s':wrap_s,'wrap_t':wrap_t,'alpha_mode':alpha,'alpha_cutoff':cutoff,'double_sided':double_sided,'tiles':{t:{'interior_pixels':pixel_rects[t],'rect':rects[t],'rgba_sha256':digest(tiles[t]['rgba'])} for t in tiles},'chunks':[]}
        for part_members in chunks(members):
            part=counters[assembly];counters[assembly]+=1;name=assembly+(f'_part{part}' if part else '')
            vertex_name=name+'.vertices.bin';index_name=name+'.indices.bin';vertex_base=index_base=0;sources=[]
            with (out/vertex_name).open('wb') as vertices,(out/index_name).open('wb') as indices:
                for entry in part_members:
                    raw=(source/entry['vertices']).read_bytes();rect=rects[source_tiles[entry['id']]]
                    if len(raw)!=entry['vertex_count']*48:raise ValueError('Source vertex count mismatch')
                    vertices.write(expand_vertices(raw,rect));ii=array('I');ii.frombytes((source/entry['indices']).read_bytes())
                    if sys.byteorder!='little':ii.byteswap()
                    if len(ii)!=entry['index_count'] or max(ii)>=entry['vertex_count']:raise ValueError('Source index count/range mismatch')
                    adjusted=array('I',(i+vertex_base for i in ii))
                    if sys.byteorder!='little':adjusted.byteswap()
                    indices.write(adjusted.tobytes())
                    sources.append({'id':entry['id'],'vertex_base':vertex_base,'index_base':index_base,'vertex_count':entry['vertex_count'],'index_count':entry['index_count'],'tile':source_tiles[entry['id']],'rect':rect})
                    vertex_base+=entry['vertex_count'];index_base+=entry['index_count']
            entry={'id':name,'vertices':vertex_name,'indices':index_name,'rgba':rgba_name,'width':image.width,'height':image.height,'vertex_count':vertex_base,'index_count':index_base,'vertex_stride':64,'assembly':assembly,'native_level':part_members[0]['native_level'],'part':part,'wrap_s':wrap_s,'wrap_t':wrap_t,'alpha_mode':alpha,'alpha_cutoff':cutoff,'double_sided':double_sided,'atlas':True,'atlas_gutter':GUTTER,'atlas_group':group_index,'sources':sources}
            entries.append(entry);group_record['chunks'].append(name)
        atlas_records.append(group_record)
        print(f'{assembly}: {len(members)} materials -> {len(group_record["chunks"])} draw(s), atlas {image.width}x{image.height}',flush=True)
    artificial=[e for e in entries if e.get('atlas')]
    budget=sum(e['vertex_count']*e.get('vertex_stride',32)+e['index_count']*4+e['width']*e['height']*4 for e in entries)
    if budget>512*1024*1024:raise ValueError('Aggregate reader allocation exceeds512MiB')
    # Recheck immutable source inventory after all copies and reads.
    for name,expected in inventory.items():
        raw=(source/name).read_bytes()
        if len(raw)!=expected['bytes'] or digest(raw)!=expected['sha256']:raise ValueError(f'Source changed during batching: {name}')
    batch={'format':1,'source':str(source),'source_identity':source_identity,'source_verified_unchanged':True,'source_artificial_parts':sum(len(v) for v in groups.values()),'output_artificial_parts':len(artificial),'atlas_groups':len(atlas_records),'unique_atlas_rgba_bytes':sum(a['width']*a['height']*4 for a in atlas_records),'reader_aggregate_bytes_with_texture_duplicates':budget,'max_vertices_per_chunk':MAX_VERTICES,'gutter_pixels':GUTTER,'filter':'GL_LINEAR, no mipmaps, no anisotropy','fragment_mapping':'wrapped=fract(uv) for REPEAT, clamp(uv,0,1) for CLAMP_TO_EDGE; atlasUV=rect.zw+wrapped*rect.xy; rect is constant per original material; wrap only after UV interpolation','max_cpu_bilinear_error_in_byte_units':sample_error,'source_vertex_and_triangle_counts_preserved':True,'atlases':atlas_records}
    result={**manifest,'batched':True,'vertex_format':'position3 normal3 uv2 [color4 when stride>=48] [atlas_rect4 when stride=64], float32 little-endian','assets':entries}
    (out/'assets.json').write_text(json.dumps(result,indent=2)+'\n')
    (out/'assets.tsv').write_text('\n'.join(' '.join(str(e.get(k,default)) for k,default in (('id',''),('vertices',''),('indices',''),('rgba',''),('width',0),('height',0),('vertex_stride',32),('wrap_s',REPEAT),('wrap_t',REPEAT),('alpha_mode','OPAQUE'))) for e in entries)+'\n',encoding='ascii')
    provenance={**original_provenance,'batching':batch};(out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    files={f.name:{'bytes':f.stat().st_size,'sha256':digest(f.read_bytes())} for f in out.iterdir() if f.is_file()}
    (out/'inventory.json').write_text(json.dumps(files,indent=2)+'\n')
    return verify(out,source)

def verify(root,source=None):
    """Bitwise geometry preservation and texture equivalence against input."""
    manifest=json.loads((root/'assets.json').read_text());provenance=json.loads((root/'provenance.json').read_text());batch=provenance['batching']
    source=Path(source or batch['source']);original=json.loads((source/'assets.json').read_text());by_id={e['id']:e for e in original['assets']}
    assert digest((source/'assets.json').read_bytes())==batch['source_identity']['manifest_sha256']
    assert digest((source/'inventory.json').read_bytes())==batch['source_identity']['inventory_sha256']
    assert len({e['id'] for e in manifest['assets']})==len(manifest['assets'])
    assert {e['id'] for e in manifest['assets'] if not e.get('atlas')}=={e['id'] for e in original['assets'] if e.get('assembly') not in ROOTS}
    budget=sum(e['vertex_count']*e.get('vertex_stride',32)+e['index_count']*4+e['width']*e['height']*4 for e in manifest['assets'])
    assert budget==batch['reader_aggregate_bytes_with_texture_duplicates'] and budget<=512*1024*1024
    seen=set();counts=[0,0];output_counts=[0,0];sampler_errors=0.;atlas_cache={}
    for entry in manifest['assets']:
        if not entry.get('atlas'):
            old=by_id[entry['id']]
            for field in ('vertices','indices','rgba'):assert (root/entry[field]).read_bytes()==(source/old[field]).read_bytes()
            continue
        assert entry['vertex_stride']==64 and entry['vertex_count']<=MAX_VERTICES
        vv=(root/entry['vertices']).read_bytes();ii=array('I');ii.frombytes((root/entry['indices']).read_bytes())
        if sys.byteorder!='little':ii.byteswap()
        assert len(vv)==entry['vertex_count']*64 and len(ii)==entry['index_count'] and max(ii)<entry['vertex_count']
        output_counts[0]+=entry['vertex_count'];output_counts[1]+=entry['index_count']
        if entry['rgba'] not in atlas_cache:atlas_cache[entry['rgba']]=Image.frombytes('RGBA',(entry['width'],entry['height']),(root/entry['rgba']).read_bytes())
        atlas=atlas_cache[entry['rgba']]
        for item in entry['sources']:
            assert item['id'] not in seen;seen.add(item['id']);old=by_id[item['id']]
            raw=(source/old['vertices']).read_bytes();old_indices=array('I');old_indices.frombytes((source/old['indices']).read_bytes())
            if sys.byteorder!='little':old_indices.byteswap()
            rect_bytes=struct.pack('<4f',*item['rect']);offset=item['vertex_base']*64
            for n in range(old['vertex_count']):
                assert vv[offset+n*64:offset+n*64+48]==raw[n*48:n*48+48]
                assert vv[offset+n*64+48:offset+(n+1)*64]==rect_bytes
            assert all(ii[item['index_base']+n]==i+item['vertex_base'] for n,i in enumerate(old_indices))
            image=Image.frombytes('RGBA',(old['width'],old['height']),(source/old['rgba']).read_bytes())
            rect=item['rect'];x=round(rect[2]*entry['width']);y=round(rect[3]*entry['height'])
            assert atlas.crop((x,y,x+old['width'],y+old['height'])).tobytes()==image.tobytes()
            sampler_errors=max(sampler_errors,check_bilinear(image,atlas,rect,entry['wrap_s'],entry['wrap_t']))
            counts[0]+=old['vertex_count'];counts[1]+=old['index_count']
    expected={e['id'] for e in original['assets'] if e.get('assembly') in ROOTS}
    assert seen==expected and counts==output_counts
    inventory=json.loads((root/'inventory.json').read_text())
    for name,info in inventory.items():
        data=(root/name).read_bytes();assert len(data)==info['bytes'] and digest(data)==info['sha256'],name
    return {'output':str(root),'source_artificial_draws':len(seen),'batched_artificial_draws':batch['output_artificial_parts'],'total_meshes':len(manifest['assets']),'atlas_groups':batch['atlas_groups'],'preserved_artificial_vertices':counts[0],'preserved_artificial_triangles':counts[1]//3,'reader_aggregate_bytes':batch['reader_aggregate_bytes_with_texture_duplicates'],'unique_atlas_rgba_bytes':batch['unique_atlas_rgba_bytes'],'max_bilinear_error_byte_units':sampler_errors,'bitwise_geometry_and_uv_preserved':True,'original_texture_pixels_preserved':True,'inventory_verified':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=Path);parser.add_argument('--out',type=Path);parser.add_argument('--verify',type=Path);parser.add_argument('--maximum-atlas',type=int,choices=(2048,4096),default=2048);args=parser.parse_args()
    if args.verify:result=verify(args.verify,args.source)
    elif args.source and args.out:result=build(args.source,args.out,args.maximum_atlas)
    else:parser.error('Provide --source and --out, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()

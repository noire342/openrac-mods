#!/usr/bin/env python3
"""Build PRIVATE space-voyages art from a verified NTSC-U or PAL installation.

Writes only a new output folder. Source models, textures and ISO stay intact.
Requires Pillow and the separately installed OpenRAC editor for WAD decoding.
"""
from __future__ import annotations
import argparse,json,math,struct,sys
from array import array
from pathlib import Path
from PIL import Image
from asset_formats import bake_ship,bounded,digest,indexed,pif,sphere,sphere_surface

PLANETS=((0,'veldin'),(1,'novalis'),(2,'aridia'),(3,'kerwan'),(4,'eudora'),
         (5,'rilgar'),(7,'umbris'),(8,'batalia'),(9,'gaspar'),(10,'orxon'),
         (11,'pokitaru'),(12,'hoven'),(14,'oltanis'),(15,'quartu'),(16,'kalebo_iii'))

def match_source_region(requested,actual):
    if actual not in ('ntsc','pal'):raise ValueError('Unsupported R&C1 disc region')
    if requested not in ('auto','ntsc','pal'):raise ValueError('Unknown requested asset region')
    if requested!='auto' and requested!=actual:raise ValueError(f'Requested {requested} assets, but the verified disc is {actual}')
    return actual

def validate_levels(levels,region,full):
    required=(98,6,13,17) if full else (98,)
    for level in required:
        manifest=json.loads((levels/f'level_{level:02}/manifest.json').read_text())
        if manifest.get('game')!='rac1' or manifest.get('version')!=region or manifest.get('level')!=level:
            raise ValueError(f'Level {level} does not belong to the verified {region} R&C1 disc')

class Source:
    def __init__(self,iso,editor,region='auto'):
        sys.path.insert(0,str(editor));from formats import wad
        from disc import Disc
        # The editor checks the boot executable's exact known SHA-1. A name or
        # an identical TOC alone cannot identify PAL versus NTSC-U safely.
        with Disc(iso) as disc:
            version=disc.version;boot=disc.executable()
        self.region=match_source_region(region,version.key);self.version=version
        self.identity={'region':self.region,'label':version.label,'executable':version.executable,
                       'executable_sha1':version.executable_sha1,'executable_sha256':digest(boot),
                       'gameplay_source':version.gameplay,'normal_table_address':hex(version.normal_table),
                       'iso_bytes':iso.stat().st_size,'verified_by':'known boot executable SHA-1; ISO read-only'}
        self.wad=wad;self.file=iso.open('rb');self.size=iso.stat().st_size
        self.file.seek(version.toc_lba*2048);self.toc=self.file.read(version.toc_size)
        if self.toc[:8]!=struct.pack('<II',1,version.toc_size):self.close();raise ValueError('Unsupported RAC1 TOC')
    def lump(self,offset):
        lba,count=struct.unpack_from('<II',self.toc,offset)
        if count*2048>64*1024*1024 or (lba+count)*2048>self.size:raise ValueError('Invalid ISO lump range')
        self.file.seek(lba*2048);data=self.file.read(count*2048)
        return self.wad(data) if data[:3]==b'WAD' else data
    def close(self):self.file.close()
    def level_data(self,level):
        if not 0<=level<19:raise ValueError('Invalid R&C1 source level')
        lba,_=struct.unpack_from('<II',self.toc,self.version.level_table+level*8)
        if lba*2048+0x2434>self.size:raise ValueError('Level header outside ISO')
        self.file.seek(lba*2048);header=self.file.read(0x2434)
        data_lba,count=struct.unpack_from('<II',header,8)
        if count*2048>64*1024*1024 or (data_lba+count)*2048>self.size:raise ValueError('Invalid source level data range')
        self.file.seek(data_lba*2048);return self.file.read(count*2048)

def write_texture(out,name,image):
    image=image.convert('RGBA');image.save(out/(name+'.png'));(out/(name+'.rgba.bin')).write_bytes(image.tobytes());return image.size

def write_mesh(out,name,vertices,indices,image,stride=32):
    if stride not in (32,48):raise ValueError('Unsupported vertex stride')
    components=stride//4
    flat=vertices if isinstance(vertices,array) else array('f',(f for v in vertices for f in v))
    count=len(flat)//components
    if len(flat)%components or not count or len(indices)%3 or any(i<0 or i>=count for i in indices):raise ValueError('Invalid output mesh')
    if any(not math.isfinite(f) for f in flat):raise ValueError('Non-finite mesh component')
    index_data=array('I',indices)
    if sys.byteorder!='little':flat=array('f',flat);flat.byteswap();index_data.byteswap()
    (out/(name+'.vertices.bin')).write_bytes(flat.tobytes())
    (out/(name+'.indices.bin')).write_bytes(index_data.tobytes())
    w,h=write_texture(out,name,image)
    return {'id':name,'vertices':name+'.vertices.bin','indices':name+'.indices.bin','rgba':name+'.rgba.bin','width':w,'height':h,'vertex_count':count,'index_count':len(indices),'vertex_stride':stride}

def glyph_tables(overlay):
    # Same bounded MIPS call-site walk as OpenRAC port/assets/world/font.cpp.
    from formats import overlay_sections
    sections=overlay_sections(overlay)['sections'];found={}
    for section in sections:
        if section['type']!=1:continue
        b=bounded(overlay,section['offset'],section['bytes']);words=struct.unpack('<'+'I'*(len(b)//4),b)
        for at,word in enumerate(words):
            face=word&65535
            if word&0xffff0000 not in (0x24040000,0x34040000) or face not in (1,2,3) or at==0 or words[at-1]>>26!=3:continue
            for hi in range(at+1,min(at+4,len(words))):
                if words[hi]&0xffff0000!=0x3c0a0000:continue
                for lo in range(hi+1,min(hi+12,len(words))):
                    if words[lo]&0xffff0000==0x254a0000 and words[lo-1]>>26==3:
                        address=((words[hi]&65535)<<16)+struct.unpack('<h',struct.pack('<H',words[lo]&65535))[0]
                        if face in found and found[face]!=address:raise ValueError('Native font call sites disagree')
                        found[face]=address;break
                break
    output={}
    for face,address in found.items():
        for s in sections:
            if s['address']<=address and address+232*4<=s['address']+s['bytes']:
                output[face]=bounded(overlay,s['offset']+address-s['address'],232*4);break
    if len(output)!=3:raise ValueError('Cannot locate all native font metrics')
    return output,found

def font_assets(source,out):
    title=source.lump(0x14e8);word=lambda i:struct.unpack_from('<I',title,i*4)[0]
    base=word(1);table=word(0x17);bank=base+word(0x1a)
    palette,pixels,w,h=struct.unpack_from('<iiii',title,table+2*16)
    image=indexed(bounded(title,bank+pixels,w*h),bounded(title,bank+palette,1024),w,h)
    write_texture(out,'font',image)
    # Metrics are located in the source overlay, not guessed from a relocated RAM.
    data=source.level_data(1)
    offset,size=struct.unpack_from('<II',data,0);overlay=bounded(data,offset,size)
    if overlay[:3]==b'WAD':overlay=source.wad(overlay)
    tables,addresses=glyph_tables(overlay);glyph=tables[2]
    lines=[f'{w} {h}']
    for code in range(232):
        u,v,top,advance=struct.unpack_from('<BBbb',glyph,code*4);lines.append(f'{code} {u} {v} {top} {advance}')
    (out/'font.tsv').write_text('\n'.join(lines)+'\n',encoding='ascii')
    return {'source':'title lump FX2, Novalis source overlay small glyph table','region':source.region,'texture_sha256':digest(image.tobytes()),'glyph_source_address':hex(addresses[2]),'glyph_sha256':digest(glyph),'cell_size':16,'pad_cell_width':24}

def radar_asset(source,out):
    # The fighter's texture_base is9; its original draw uses base+0x29=50.
    # FX49 is the combat health arc and is deliberately not copied into the UI.
    data=source.level_data(13)
    sections={}
    for section,name in [(2,'index'),(10,'core')]:
        at,size=struct.unpack_from('<II',data,section*8);part=bounded(data,at,size)
        sections[name]=source.wad(part) if part[:3]==b'WAD' else part
    index,core=sections['index'],sections['core'];count,table=struct.unpack_from('<II',index,0x58)
    if count<=50:raise ValueError('Gemlik radar FX50 missing')
    bank=struct.unpack_from('<I',index,0x68)[0]
    palette,pixels,w,h=struct.unpack_from('<iiii',index,table+50*16)
    if (w,h)!=(128,128):raise ValueError('Unexpected native radar dimensions')
    image=indexed(bounded(core,bank+pixels,w*h),bounded(core,bank+palette,1024),w,h)
    write_texture(out,'radar',image)
    return {'source':'Gemlik native FX50, original fighter radar frame/grid; combat health FX49 excluded','region':source.region,'native_level':13,'fx_texture':50,'width':w,'height':h,'rgba_sha256':digest(image.tobytes())}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso',type=Path,required=True);p.add_argument('--levels',type=Path,required=True)
    p.add_argument('--editor',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--region',choices=('auto','ntsc','pal'),default='auto',help='Verified disc region; auto detects the known boot executable; mixed-region level exports are rejected')
    p.add_argument('--ship-overrides',type=Path,help='Active graphics pack; matching level98/level01 ship replacements win')
    p.add_argument('--full-galaxy',action='store_true',help='All 15 globes, original static Blarg/Gemlik/Fleet assemblies and three player ships')
    args=p.parse_args();out=args.out.resolve()
    if out.exists():raise ValueError('Refusing to overwrite an asset build')
    source=Source(args.iso,args.editor,args.region);entries=[];provenance={'format':1,'private':True,'game':'rac1','disc':source.region,'disc_identity':source.identity,'planets':{},'ships':{}}
    try:
        validate_levels(args.levels,source.region,args.full_galaxy)
        out.mkdir(parents=True)
        transition=source.lump(0x13b8);word=lambda i:struct.unpack_from('<I',transition,i*4)[0]
        table=word(1)+word(0x13);count=struct.unpack_from('<I',transition,table)[0]
        if count!=133:raise ValueError('Unexpected native planet/caption table')
        mesh=sphere()
        planet_catalog=PLANETS if args.full_galaxy else ((1,'novalis'),(3,'kerwan'),(4,'eudora'))
        for level,name in planet_catalog:
            at=table+struct.unpack_from('<I',transition,table+4+level*4)[0]
            preview=pif(transition[at:]);preview.save(out/(name+'-original-preview.png'))
            texture=sphere_surface(preview)
            entry=write_mesh(out,'planet_'+name,*mesh,texture);entry['native_level']=level;entries.append(entry)
            provenance['planets'][name]={'native_level':level,'source':'transition lump planet_picture','toc_offset':'0x13b8','decompressed_pif_offset':at,'source_preview_rgba_sha256':digest(preview.tobytes()),'surface':'original visible disk deprojected; far side reconstructed by mirrored original sampling; no native 360-degree map exists in this source'}
        for oclass in (531,532,533):
            original=args.levels/'level_98/mobys'/f'moby_{oclass}.glb';model=original
            if args.ship_overrides:
                candidates=[args.ship_overrides/f'level_{level:02}/mobys/moby_{oclass}.glb' for level in (98,1,0)]
                model=next((c for c in candidates if c.is_file()),original)
            parts,info=bake_ship(model,sequence=1)
            for part,(vertices,indices,texture,alpha) in enumerate(parts):
                image=Image.open(texture).convert('RGBA')
                # Native metal/glass UV is retained; the lightweight preview
                # renderer blends its original material alpha as supplied.
                entry=write_mesh(out,f'ship_{oclass}'+(f'_part{part}' if part else ''),vertices,indices,image)
                entry['ship_class']=oclass;entry['part']=part;entry['alpha_mode']=alpha;entries.append(entry)
            provenance['ships'][str(oclass)]={**info,'model_sha256':digest(model.read_bytes()),'replacement_used':model!=original,'parts':len(parts),'textures_sha256':[digest(p[2].read_bytes()) for p in parts]}
        if args.full_galaxy:
            from station_assets import ARTIFICIAL_DESTINATIONS,Inputs,build_station
            inputs=Inputs();provenance['artificial_destinations']={}
            for level,name in ARTIFICIAL_DESTINATIONS:
                print(f'Building native static assembly: {name}',flush=True)
                parts,info=build_station(args.levels,level,name,out,write_mesh,inputs,region=source.region)
                entries.extend(parts);provenance['artificial_destinations'][name]=info
                print(f'{name}: {len(parts)} batches, {info["vertex_count"]} vertices, {info["triangle_count"]} triangles',flush=True)
            provenance['input_readonly_check']={'verified_unchanged':True,'files':inputs.check()}
            provenance['catalog']={'distinct_planets':15,'artificial_destinations':3,'duplicate_visits':{'veldin':[0,18],'orxon':[10]},'geometry':'15 reconstructed native-preview globes; 3 complete terrain+TIE LOD0 assemblies'}
        provenance['font']=font_assets(source,out)
        provenance['radar']=radar_asset(source,out)
        # Source space backdrop is already exported from the flight's six shells.
        sky=args.levels/'level_98/sky.png'
        if sky.is_file():
            image=Image.open(sky).convert('RGBA');write_texture(out,'space_sky',image)
            provenance['sky']={'source':'native level98 sky panorama','source_sha256':digest(sky.read_bytes())}
        (out/'assets.json').write_text(json.dumps({'format':1,'region':source.region,'full_galaxy':args.full_galaxy,'vertex_stride':32,'vertex_format':'position3 normal3 uv2 [color4 when stride=48] float32 little-endian','indices':'uint32 little-endian','assets':entries},indent=2)+'\n')
        rows=[]
        for entry in entries:
            row=[entry[k] for k in ('id','vertices','indices','rgba','width','height')]
            if args.full_galaxy:row.extend((entry['vertex_stride'],entry.get('wrap_s',10497),entry.get('wrap_t',10497),entry.get('alpha_mode','OPAQUE')))
            rows.append(' '.join(map(str,row)))
        (out/'assets.tsv').write_text('\n'.join(rows)+'\n',encoding='ascii')
        (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
        inventory={f.name:{'bytes':f.stat().st_size,'sha256':digest(f.read_bytes())} for f in out.iterdir() if f.is_file()}
        (out/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
        print(json.dumps({'output':str(out),'region':source.region,'assets':len(entries),'planets':[p[0] for p in planet_catalog],'ships':[531,532,533],'font':'native small','private':True},indent=2))
    finally:source.close()

if __name__=='__main__':main()

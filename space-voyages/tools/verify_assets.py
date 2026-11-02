#!/usr/bin/env python3
"""Validate geometry, references, bounds and provenance of a PRIVATE build."""
from __future__ import annotations
import argparse,json,math,struct,sys
from array import array
from pathlib import Path
from asset_formats import digest
from build_assets import PLANETS

def verify(root):
    manifest=json.loads((root/'assets.json').read_text());provenance=json.loads((root/'provenance.json').read_text())
    assert manifest['format']==1 and manifest['vertex_stride']==32
    records=manifest['assets'];assert {r['id'] for r in records}>={'planet_novalis','planet_kerwan','planet_eudora','ship_531','ship_532','ship_533'}
    assert len({r['id'] for r in records})==len(records),'Duplicate asset ID'
    full=manifest.get('full_galaxy',False);assemblies={};ship_bounds={};geometry_bytes=0;rgba_bytes=0
    if full:
        assert {r['id'] for r in records if r['id'].startswith('planet_')}=={'planet_'+name for _,name in PLANETS}
        assert set(provenance['artificial_destinations'])=={'station_blarg','station_gemlik','fleet_drek'}
        assert len(records)<=1024
        assert provenance['input_readonly_check']['verified_unchanged'] is True
    tsv=(root/'assets.tsv').read_text().splitlines();assert len(tsv)==len(records)
    for entry,line in zip(records,tsv):
        fields=line.split();assert fields[:6]==[str(entry[k]) for k in ('id','vertices','indices','rgba','width','height')]
        raw=(root/entry['vertices']).read_bytes();indices=(root/entry['indices']).read_bytes();rgba=(root/entry['rgba']).read_bytes()
        stride=entry.get('vertex_stride',32);assert stride in (32,48)
        if full:assert fields[6:]==[str(stride),str(entry.get('wrap_s',10497)),str(entry.get('wrap_t',10497)),entry.get('alpha_mode','OPAQUE')]
        assert len(raw)==entry['vertex_count']*stride and len(indices)==entry['index_count']*4
        assert len(rgba)==entry['width']*entry['height']*4
        vv=array('f');vv.frombytes(raw);ii=array('I');ii.frombytes(indices)
        if sys.byteorder!='little':vv.byteswap();ii.byteswap()
        components=stride//4;assert len(ii)%3==0 and max(ii)<entry['vertex_count']
        assert all(math.isfinite(f) for f in vv)
        assert all(abs(sum(vv[k+j]**2 for j in (3,4,5))-1)<.001 for k in range(0,len(vv),components)),entry['id']
        lo=[min(vv[c::components]) for c in range(3)];hi=[max(vv[c::components]) for c in range(3)]
        geometry_bytes+=len(raw)+len(indices);rgba_bytes+=len(rgba)
        if entry['id'].startswith('planet_'):
            assert all(abs(sum(vv[k+j]**2 for j in (0,1,2))-1)<.00001 for k in range(0,len(vv),components))
            assert all(x==255 for x in rgba[3::4]),'3D planets must not keep the 2D preview alpha disk'
            # No triangle should collapse at a pole or seam.
            for k in range(0,len(ii),3):
                p,q,r=(vv[ii[k+j]*components:ii[k+j]*components+3] for j in range(3));a=[q[c]-p[c] for c in range(3)];b=[r[c]-p[c] for c in range(3)]
                cross=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
                assert sum(x*x for x in cross)>1e-12,'Degenerate planet triangle'
        elif entry['id'].startswith('ship_'):
            assert max(abs(x) for x in lo+hi)<5,entry['id']
            bounds=ship_bounds.setdefault(entry['ship_class'],[lo[:],hi[:]])
            for c in range(3):bounds[0][c]=min(bounds[0][c],lo[c]);bounds[1][c]=max(bounds[1][c],hi[c])
        else:
            assert full and stride==48 and entry['assembly'] in provenance['artificial_destinations']
            assert entry['wrap_s'] in (10497,33071) and entry['wrap_t'] in (10497,33071)
            assert entry['alpha_mode'] in ('OPAQUE','MASK','BLEND')
            radius=max(math.sqrt(sum(vv[k+j]**2 for j in (0,1,2))) for k in range(0,len(vv),components))
            assert radius<=1.000001,entry['id']
            assert all(0<=vv[k+j]<=4 for k in range(0,len(vv),components) for j in (8,9,10,11))
            item=assemblies.setdefault(entry['assembly'],{'radius':0.,'parts':0,'vertices':0,'triangles':0})
            item['radius']=max(item['radius'],radius);item['parts']+=1;item['vertices']+=entry['vertex_count'];item['triangles']+=entry['index_count']//3
    assert set(ship_bounds)=={531,532,533}
    for lo,hi in ship_bounds.values():assert abs(hi[0]-lo[0]-2.5)<.0001
    if full:
        assert geometry_bytes+rgba_bytes<512*1024*1024
        for name,item in assemblies.items():
            source=provenance['artificial_destinations'][name]
            assert abs(item['radius']-1)<.000001
            assert (item['parts'],item['vertices'],item['triangles'])==(source['batches'],source['vertex_count'],source['triangle_count'])
            assert source['tiny_ties_excluded']==0 and source['lod']=='exact exported LOD0; no simplification'
    lines=(root/'font.tsv').read_text().splitlines();w,h=map(int,lines[0].split())
    assert (w,h)==(256,128) and len((root/'font.rgba.bin').read_bytes())==w*h*4
    glyphs={}
    for line in lines[1:]:
        code,u,v,top,advance=map(int,line.split());glyphs[code]=(u,v,top,advance)
        assert 0<=u<256 and 0<=v<128 and -128<=top<=127 and -128<=advance<=127
    assert len(glyphs)==232 and glyphs[65][3]>0 and glyphs[97][3]>0
    assert glyphs[18]==(48,96,0,18),'Native triangle glyph must be preserved'
    if 'radar' in provenance:
        radar=(root/'radar.rgba.bin').read_bytes()
        assert len(radar)==128*128*4 and digest(radar)==provenance['radar']['rgba_sha256']
        assert provenance['radar']['native_level']==13 and provenance['radar']['fx_texture']==50
    inventory=json.loads((root/'inventory.json').read_text())
    for name,expected in inventory.items():
        path=root/name;assert path.stat().st_size==expected['bytes'] and digest(path.read_bytes())==expected['sha256'],name
    assert [provenance['planets'][n]['native_level'] for n in ('novalis','kerwan','eudora')]==[1,3,4]
    assert all('far side reconstructed' in p['surface'] for p in provenance['planets'].values())
    assert provenance['private'] is True
    return {'meshes':len(records),'planets':len(provenance['planets']),'static_assemblies':assemblies,'geometry_bytes':geometry_bytes,'rgba_bytes':rgba_bytes,'native_ship_classes':[531,532,533],'native_font_glyphs':232,'native_radar_fx50':'radar' in provenance,'finite_unit_normals':True,'native_preview_provenance':True,'inventory_integrity':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path);args=parser.parse_args()
    print(json.dumps(verify(args.root),indent=2))

if __name__=='__main__':main()
